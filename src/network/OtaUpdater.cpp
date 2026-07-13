#include "OtaUpdater.h"

// clang-format off
// HttpDownloader.h pulls Arduino/SdFat, whose macros collide with lwip's
// ip4_addr.h unless seen first. Pin this order; clang-format would otherwise sort
// the local header last and break the build.
#include "HttpDownloader.h"
#include "OtaBootSwitch.h"
#include <Logging.h>
#include <ReleaseJsonParser.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
// clang-format on

#include <cctype>
#include <cstring>
#include <string>

namespace {
constexpr char latestReleaseUrl[] = "https://api.github.com/repos/UniqueDroid/PicoRead/releases/latest";

// GitHub's asset "digest" field looks like "sha256:<hex>". Returns the bare hex string, or
// empty if the digest is missing, malformed, or not a sha256 digest.
std::string bareSha256Hex(const std::string& digest) {
  constexpr char prefix[] = "sha256:";
  constexpr size_t prefixLen = sizeof(prefix) - 1;
  if (digest.size() != prefixLen + 64 || digest.compare(0, prefixLen, prefix) != 0) {
    return "";
  }
  return digest.substr(prefixLen);
}

// Decodes a 64-char hex string into 32 bytes. Returns false on malformed input.
bool hexDecode32(const std::string& hex, uint8_t out[32]) {
  if (hex.size() != 64) return false;
  for (size_t i = 0; i < 32; i++) {
    char hi = hex[i * 2];
    char lo = hex[i * 2 + 1];
    if (!isxdigit(static_cast<unsigned char>(hi)) || !isxdigit(static_cast<unsigned char>(lo))) return false;
    auto nibble = [](char c) -> uint8_t {
      if (c >= '0' && c <= '9') return c - '0';
      return (c | 0x20) - 'a' + 10;  // fold to lowercase
    };
    out[i] = static_cast<uint8_t>((nibble(hi) << 4) | nibble(lo));
  }
  return true;
}
}  // namespace

OtaUpdater::OtaUpdaterError OtaUpdater::checkForUpdate() {
  LOG_DBG("OTA", "Checking for update (current: %s)", PICOREAD_VERSION);

  // Stream the ~32KB release JSON straight into the parser as it arrives.
  // Buffering the whole body in a std::string would add a growing allocation
  // on top of the TLS session's heap during the fetch; with -fno-exceptions an
  // OOM there aborts. fetchUrl handles the verified-https GET, redirects, and
  // User-Agent (see HttpDownloader).
  ReleaseJsonParser releaseParser;
  const bool ok = HttpDownloader::fetchUrl(latestReleaseUrl, [&releaseParser](const uint8_t* data, size_t len) {
    releaseParser.feed(reinterpret_cast<const char*>(data), len);
    return true;
  });
  if (!ok) {
    LOG_ERR("OTA", "Release check fetch failed");
    return HTTP_ERROR;
  }

  LOG_DBG("OTA", "Parser results: tag=%s firmware=%s", releaseParser.foundTag() ? "yes" : "no",
          releaseParser.foundFirmware() ? "yes" : "no");

  if (!releaseParser.foundTag()) {
    LOG_ERR("OTA", "No tag_name in release JSON");
    return JSON_PARSE_ERROR;
  }

  if (!releaseParser.foundFirmware()) {
    LOG_ERR("OTA", "No firmware.bin asset found");
    return NO_UPDATE;
  }

  latestVersion = releaseParser.getTagName();
  otaUrl = releaseParser.getFirmwareUrl();
  otaSize = releaseParser.getFirmwareSize();
  otaDigest = releaseParser.getFirmwareDigest();
  totalSize = otaSize;
  updateAvailable = true;

  LOG_DBG("OTA", "Found update: tag=%s size=%zu", latestVersion.c_str(), otaSize);
  LOG_DBG("OTA", "Firmware URL: %s", otaUrl.c_str());
  LOG_DBG("OTA", "Firmware digest: %s", otaDigest.empty() ? "(none)" : otaDigest.c_str());
  return OK;
}

bool OtaUpdater::isUpdateNewer() const {
  if (!updateAvailable || latestVersion.empty() || latestVersion == PICOREAD_VERSION) {
    return false;
  }

  int currentMajor, currentMinor, currentPatch;
  int latestMajor, latestMinor, latestPatch;

  const auto currentVersion = PICOREAD_VERSION;

  // semantic version check (only match on 3 segments)
  sscanf(latestVersion.c_str(), "%d.%d.%d", &latestMajor, &latestMinor, &latestPatch);
  sscanf(currentVersion, "%d.%d.%d", &currentMajor, &currentMinor, &currentPatch);

  /*
   * Compare major versions.
   * If they differ, return true if latest major version greater than current major version
   * otherwise return false.
   */
  if (latestMajor != currentMajor) return latestMajor > currentMajor;

  /*
   * Compare minor versions.
   * If they differ, return true if latest minor version greater than current minor version
   * otherwise return false.
   */
  if (latestMinor != currentMinor) return latestMinor > currentMinor;

  /*
   * Check patch versions.
   */
  if (latestPatch != currentPatch) return latestPatch > currentPatch;

  // If we reach here, it means all segments are equal.
  // One final check, if we're on an RC build (contains "-rc"), we should consider the latest version as newer even if
  // the segments are equal, since RC builds are pre-release versions.
  if (strstr(currentVersion, "-rc") != nullptr) {
    return true;
  }

  return false;
}

const std::string& OtaUpdater::getLatestVersion() const { return latestVersion; }

OtaUpdater::OtaUpdaterError OtaUpdater::installUpdate(ProgressCallback onProgress, void* ctx) {
  if (!isUpdateNewer()) {
    return UPDATE_OLDER_ERROR;
  }

  // esp_https_ota is hardwired to esp-tls/mbedTLS, whose precompiled build on this
  // package can't negotiate TLS 1.3 (see SecureClient.h). Drive the OTA partition
  // ourselves and stream the firmware through HttpDownloader, which runs over
  // wolfSSL when FREEINK_NET_WOLFSSL is set, reusing its redirect handling for the
  // GitHub -> CDN hop.
  const esp_partition_t* updatePartition = esp_ota_get_next_update_partition(nullptr);
  if (!updatePartition) {
    LOG_ERR("OTA", "No OTA partition available");
    return INTERNAL_UPDATE_ERROR;
  }

  esp_ota_handle_t otaHandle = 0;
  esp_err_t esp_err = esp_ota_begin(updatePartition, OTA_SIZE_UNKNOWN, &otaHandle);
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_begin failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  /* For better timing and connectivity, we disable power saving for WiFi */
  esp_wifi_set_ps(WIFI_PS_NONE);

  // Verify the download against GitHub's published digest before it's ever allowed to
  // become the boot target. Only meaningful if the release actually has one (older
  // releases published before GitHub added asset digests won't).
  const std::string expectedHex = bareSha256Hex(otaDigest);
  const bool verifyHash = !expectedHex.empty();
  mbedtls_sha256_context shaCtx;
  if (verifyHash) {
    mbedtls_sha256_init(&shaCtx);
    mbedtls_sha256_starts(&shaCtx, /*is224=*/0);
  } else {
    LOG_ERR("OTA", "No asset digest for this release - skipping checksum verification");
  }

  processedSize = 0;
  int lastReportedPct = -1;
  bool flashOk = true;
  const bool fetchOk = HttpDownloader::fetchUrl(otaUrl, [&](const uint8_t* data, size_t len) {
    if (esp_ota_write(otaHandle, data, len) != ESP_OK) {
      flashOk = false;
      return false;  // abort the transfer
    }
    if (verifyHash) {
      mbedtls_sha256_update(&shaCtx, data, len);
    }
    processedSize += len;
    // Fire the callback only on whole-percent change. Per-chunk updates wake the
    // render task, whose framebuffer work contends with TLS on the internal arena,
    // and e-ink can't repaint faster than a percent tick anyway.
    if (onProgress && totalSize > 0) {
      const int pct = static_cast<int>(static_cast<uint64_t>(processedSize) * 100 / totalSize);
      if (pct != lastReportedPct) {
        lastReportedPct = pct;
        onProgress(ctx);
      }
    }
    return true;
  });

  /* Return back to default power saving for WiFi in case of failing */
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

  if (!fetchOk || !flashOk) {
    LOG_ERR("OTA", "Firmware install failed (%s)", flashOk ? "download" : "flash write");
    if (verifyHash) mbedtls_sha256_free(&shaCtx);
    esp_ota_abort(otaHandle);
    return flashOk ? HTTP_ERROR : INTERNAL_UPDATE_ERROR;
  }

  if (verifyHash) {
    uint8_t computed[32];
    mbedtls_sha256_finish(&shaCtx, computed);
    mbedtls_sha256_free(&shaCtx);

    uint8_t expected[32];
    if (!hexDecode32(expectedHex, expected) || memcmp(computed, expected, sizeof(computed)) != 0) {
      LOG_ERR("OTA", "Firmware checksum mismatch (SHA256) - update aborted");
      esp_ota_abort(otaHandle);
      return CHECKSUM_ERROR;
    }
    LOG_INF("OTA", "Firmware checksum verified");
  }

  esp_err = esp_ota_end(otaHandle);  // finalizes the write and validates the image itself
  if (esp_err != ESP_OK) {
    LOG_ERR("OTA", "esp_ota_end failed: %s", esp_err_to_name(esp_err));
    return INTERNAL_UPDATE_ERROR;
  }

  // esp_ota_set_boot_partition() re-verifies the image via esp_image_verify(), which on
  // X3/X4 misreads eFuse block revision through a misaligned bootloader_mmap pointer and
  // rejects a valid image (see OtaBootSwitch.h). Use the same raw otadata write the
  // SD-card flash path uses instead - already proven working on this hardware.
  if (!ota_boot::switchTo(updatePartition)) {
    LOG_ERR("OTA", "otadata switch failed");
    return INTERNAL_UPDATE_ERROR;
  }

  LOG_INF("OTA", "Update completed");
  return OK;
}
