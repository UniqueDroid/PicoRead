# PicoRead

PicoRead is [Jan](https://github.com/UniqueDroid)'s personal fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader), an open-source, fully hackable e-reader firmware. All credit for the original engine, architecture, and design goes to the upstream CrossPoint Reader project — see [Credits](#credits) below. This fork adds Jan's own experiments on top of that foundation and isn't affiliated with the upstream project or its maintainers.

**Now running on:** ESP32C3-based Xteink [X4](https://www.xteink.com/products/xteink-x4) and [X3](https://www.xteink.com/products/xteink-x3).

![PicoRead running on Xteink device](./docs/images/cover.jpg)

> If you're planning to buy an Xteink device, consider purchasing an **X3/X4 Developer Edition** through https://crosspointreader.com. CrossPoint (the upstream project) receives a small share of each sale, helping fund development of the engine PicoRead is built on.

## What can PicoRead do?

- **Reader engine**: EPUB 2/3 rendering with embedded-style option, image handling, hyphenation, kerning, chapter navigation, footnotes, bookmarks, go-to-percent, auto page turn, orientation control, focus reading, KOReader progress sync and more. 

- **Various formats**: native handling for `.epub`, `.xtc/.xtch`, `.txt`, and `.bmp`.

- **PDF support (PicoRead-only)**: convert PDFs to `.xtc` image pages right from the web UI (`/pdf-to-xtc`) — no reflow, but readable as a comic/image-style book.

- **Dictionary lookup (PicoRead-only)**: inline word lookup in the reader menu, using installed FreeDict dictionaries.

- **Cross-book bookmarks (PicoRead-only)**: a "Bookmarks" tile on the home screen jumps straight into any bookmarked spot across your recent books.

- **Screenshots.**

- **Custom fonts**: install your favorite fonts on the SD card.

- **Tilt page turn (X3 only)**.

- **Library workflow**: folder browser, hidden-file toggle, long-press delete, recent books, SD-cache management.

- **Wireless workflows**:
  
  - File transfer web UI
  - EPUB Optimizer
  - Web settings UI/API (edit many device settings from browser)
  - WebSocket fast uploads
  - WebDAV handler
  - AP mode (hotspot) and STA mode (join existing Wi-Fi), both with QR helpers
  - Calibre wireless connect flow
  - OPDS browser with saved servers (up to 8), search, pagination, and direct download
  - OTA update checks and installs from GitHub releases

- **Customization**: multiple themes (Classic, Lyra, Lyra Extended, RoundedRaff), sleep screen modes, front/side button remapping, status bar controls, power-button behavior, refresh cadence, and more.

- **Localization**: 24 UI languages and counting. RTL support.

### Coming soon:

- More themes.

- Much more! stay tuned.

---

## USB-locked devices (Xteink Unlocker)

Some Xteink units purchased from third-party stores (e.g. AliExpress) ship with USB flashing locked from the factory.
If your device is locked, you will need to use the **Xteink Unlocker** tool (from the upstream CrossPoint project) available at
https://crosspointreader.com/#unlock-tool before you can flash PicoRead.

**You do not need this tool if you bought your device directly from xteink.com.** Those units are not locked.

**Not sure if your device is locked?** Power it on, connect the USB-C cable, and try flashing via the web flasher first (see
[Install firmware](#install-firmware) below). If the browser's serial device picker does not show your device, try a different
USB port or browser before assuming the device is locked. Only reach for the unlocker if the device still doesn't appear.

> ### ⚠️ WARNING: READ THIS BEFORE USING THE UNLOCKER ⚠️
> 
> **The only officially supported firmwares in that unlock tool are CrossPoint and CrossInk — PicoRead is not listed there.**
> Use the "Custom .bin" upload option instead (see below) to flash a PicoRead build once the device is unlocked.
> 
> Flashing any other firmware on a USB-locked device may **permanently brick the device** or leave it **permanently
> stuck on that firmware with no recovery path**. Once USB flashing is re-locked, your only way back is via OTA, and if
> the firmware you flashed doesn't support OTA, **there is no way out**.

## Install firmware

### Web installer

1. Connect your device to your computer via USB-C and wake/unlock the device
2. Download a `firmware.bin` from [Releases](https://github.com/UniqueDroid/PicoRead/releases), a local build, or a CI artifact.
3. Go to https://crosspointreader.com/#flash-tools (the upstream project's generic web flasher, works for any compatible `.bin`), select device (X3 or X4), click "Custom .bin" and upload your PicoRead `firmware.bin`. There is no official PicoRead entry in that tool's release picker.

### Revert to Official CrossPoint Firmware

To go back to the official upstream firmware, flash the latest official CrossPoint release using https://crosspointreader.com/#flash-tools.

### Command line

1. Install [`esptool`](https://github.com/espressif/esptool):

```bash
pip install esptool
```

2. Download `firmware.bin` from the [releases page](https://github.com/UniqueDroid/PicoRead/releases).
3. Connect your device via USB-C.
4. Find the device port. On Linux, run `dmesg` after connecting. On macOS:

```bash
log stream --predicate 'subsystem == "com.apple.iokit"' --info
```

5. Flash:

```bash
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

Adjust `/dev/ttyACM0` to match your system.

### Manual

See [Development quick start](#development-quick-start) below.

---

## Custom SD-card fonts

Convert your own TTF/OTF files into `.cpfont` files that load from the SD card. No firmware reflash is needed.

1. Go to https://crosspointreader.com/fonts (the upstream project's font builder tool — output is a generic `.cpfont` format, works for PicoRead too) and open the "SD-card font builder" form.
2. Upload up to four styles (regular, bold, italic, bold-italic), set the family name, point sizes, and Unicode range.
3. Download the generated `.cpfont` files.
4. Copy them to your SD card under `/fonts/YourFont/` (or `/.fonts/YourFont/` to hide the folder).
5. Select the font on the device from the font settings.

Conversion runs the firmware repo's `lib/EpdFont/scripts/fontconvert_sdcard.py` script unmodified, so output matches a local host build.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md)
- [Web server endpoints](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md)

---

## Development quick start

### Prerequisites

- [pioarduino](https://github.com/pioarduino/pioarduino) or VS Code + pioarduino plugin
- Python 3.8+
- `clang-format` 21
- USB-C cable supporting data transfer

### Setup

```bash
git clone --recursive https://github.com/UniqueDroid/PicoRead
cd PicoRead

# if cloned without --recursive:
git submodule update --init --recursive
```

### Build / flash / monitor

```bash
pio run --target upload
```

### Contributor pre-PR checks

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

### Debugging

After flashing the new features, it’s recommended to capture detailed logs from the serial port.

First, make sure all required Python packages are installed:

```python
python3 -m pip install pyserial colorama matplotlib
```

After that run the script:

```sh
# For Linux
# This was tested on Debian and should work on most Linux systems.
python3 scripts/debugging_monitor.py

# For macOS
python3 scripts/debugging_monitor.py /dev/cu.usbmodem2101
```

Minor adjustments may be required for Windows.

---

## Internals

PicoRead is pretty aggressive about caching data down to the SD card to minimise RAM usage. The ESP32-C3 only has ~380KB of usable RAM, so we have to be careful. A lot of the decisions made in the design of the firmware were based on this constraint.

### Data caching

The first time chapters of a book are loaded, they are cached to the SD card. Subsequent loads are served from the
cache. This cache directory exists at `.picoread` on the SD card. The structure is as follows:

```text
.picoread/
├── epub_<hash>/         # one directory per book, named by content hash
│   ├── progress.bin     # reading position (chapter, page, etc.)
│   ├── cover.bmp        # generated cover image
│   ├── book.bin         # metadata: title, author, spine, TOC
│   ├── css_rules.cache  # parsed CSS rule cache
│   ├── img_*            # rendered image cache files
│   └── sections/        # per-chapter layout cache
│       ├── 0.bin
│       ├── 1.bin
│       └── ...
├── settings.json        # device settings
├── state.json           # resume/runtime state
└── recent.json          # recent books list
```

Removing `/.picoread` clears all cached metadata and forces a full regeneration on next open. Book deletes, overwrites, and moves done through the firmware or web UI clear or re-key matching caches; manual SD-card edits may leave stale cache directories behind.

For more details on the internal file structures, see the [file formats document](./docs/file-formats.md).

---

## Contributing

This is Jan's personal fork, so day-to-day development happens solo. If you're curious about the codebase, the [contributing docs](./docs/contributing/README.md) (inherited from upstream) still apply architecturally. For the general CrossPoint Reader project itself, check the upstream [ideas discussion board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas).

---

## Community forks

One of the best things about open source is that anyone can take the code in a different direction. If you need something outside PicoRead's [scope](./SCOPE.md), check out the community forks:

- [CrossInk](https://github.com/uxjulia/CrossInk) — Typography and reading tracking: Bionic Reading (bolds word stems to create fixation points), guide dots between words, improved paragraph indents, and replaces the default fonts with ChareInk/Lexend/Bitter.

- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — Adds FB2 and MD format support. Actively maintained with Arabic script support. Custom themes via SD card.

- ~~[crosspet](https://github.com/trilwu/crosspet) — A Vietnamese fork that adds a Tamagotchi-style virtual chicken that grows based on your reading milestones (pages read, streaks, care). Also: Flashcards, Weather, Pomodoro timer, and mini-games.~~ (Unmaintained)

- [crosspoint-reader-cjk](https://github.com/aBER0724/crosspoint-reader-cjk) — Purpose-built for Chinese, Japanese, and Korean reading.

- [inx](https://github.com/obijuankenobiii/inx) — Completely reimagines the user interface with tabbed navigation.

- ~~[PlusPoint](https://github.com/ngxson/pluspoint-reader) — custom JS apps support.~~ (Unmaintained)

- [crosspoint-reader-papers3](https://github.com/juicecultus/crosspoint-reader-papers3) — CrossPoint port for M5Stack Paper S3.

- [t5s3-reader](https://github.com/ShallowGreen123/t5s3-reader) — CrossPoint port for LilyGo T5 ePaper S3 / T5S3 4.7-inch e-paper device.

Want to build your own device? Be sure to check out the [de-link](https://github.com/iandchasse/de-link) project.

---

## Credits

PicoRead is a personal fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) by Dave Allie and contributors, licensed under MIT (see [LICENSE](./LICENSE)). All credit for the original reader engine, activity architecture, HAL, and the huge amount of design work behind it goes to that upstream project — go star it and, if you can, [support the original maintainers](https://app.royalty.dev/crosspoint-reader/crosspoint-reader).

What's different in this fork so far: a PDF-to-XTC converter, cross-book bookmarks, and offline dictionary lookup (see [What can PicoRead do?](#what-can-picoread-do) above).

---

PicoRead is **not affiliated with Xteink or any device manufacturer**, nor with the upstream CrossPoint Reader project or its maintainers.

Huge shoutout to [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader), which inspired the original CrossPoint project.
