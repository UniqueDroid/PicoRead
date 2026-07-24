# PicoRead

![PicoRead Logo](/src/images/picoread_logo_github.png)

PicoRead is my ([DerJan](https://github.com/UniqueDroid)'s) personal playground fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader) — a genuinely excellent open-source e-reader firmware built by Dave Allie and contributors. I'm not trying to compete with or replace it; I just wanted a place to bolt on my own ideas without waiting on someone else's review queue. If you're not already tinkering with the upstream project, go check it out first — everything good here was built on top of it. Not affiliated with the upstream maintainers or with Xteink.

**Hardware:** ESP32-C3-based Xteink [X4](https://www.xteink.com/products/xteink-x4) and [X3](https://www.xteink.com/products/xteink-x3) e-ink readers.

## What's in the box

Everything CrossPoint Reader already does well is still here: EPUB 2/3 rendering, hyphenation and kerning, chapter navigation, footnotes, bookmarks, go-to-percent, auto page turn, orientation control, focus reading, KOReader sync, `.xtc/.xtch`/`.txt`/`.bmp` support, a folder browser with hidden-file toggle and SD-cache management, a full wireless toolkit (file transfer, EPUB Optimizer, web settings, WebSocket uploads, WebDAV, hotspot/join-network modes with QR helpers, Calibre connect, OPDS browsing), four themes, sleep screens, remappable buttons, and 24 languages with RTL support. That's the foundation — full credit to upstream for all of it.

On top of that, this fork so far adds:

- **Web-based firmware updates** — a Firmware Update page in the web menu with a live progress bar and SHA256 verification before anything gets flashed. No cable required once you're already on a PicoRead build.
- **Cross-book bookmarks** — a Bookmarks tile on the home screen jumps straight to any bookmark across your recent books, not just the one you're currently reading.
- **Offline dictionary lookup** — inline word lookup in the reader menu using FreeDict dictionaries you install yourself.
- **A working PDF trick, no firmware bloat** — the reader has always been able to flip through a folder of `.bmp` images with the Left/Right buttons. Export a PDF's pages as images on your computer, drop them in one folder on the SD card, and you've got a readable "PDF" with zero code changes and zero flash cost.
- **A leaner EPUB Optimizer** — it now strips embedded fonts on request, since the reader never uses `@font-face` anyway and those files are pure dead weight in most EPUBs.
- **PicoRead theme** — a new default theme with a 2-column tile home screen and a 3-cover "Continue Reading" strip up top, alongside the existing Classic/Lyra/RoundedRaff themes (pick any of them in Settings > Display).
- **Reading Statistics** — a home-screen tile with per-book stats (sessions, reading time, pages turned, average session length, pages/minute) plus an "All Books" aggregate card.
- **RSS Reader** — subscribe to feeds, sync them over Wi-Fi, and read articles fully offline afterwards. See [RSS Reader](#rss-reader) below.
- **Wikipedia** — today's featured article, an on-this-day digest, and a random article, synced with one tap and read fully offline afterward. See [Wikipedia](#wikipedia) below.
- **Restart / Shut Down** — both are now one tap away in Settings > System instead of requiring a button-combo or waiting out the sleep timer.
- **Flappy** — a slow-tick, e-ink-appropriate take on Flappy Bird for killing 5 minutes between chapters. Home screen tile, one button to flap.

More to come as I find time. See [Credits](#credits) for the full attribution.

---

## USB-locked devices

Some Xteink units bought through third-party resellers (AliExpress and similar) ship with USB flashing locked. Units bought directly from xteink.com are not affected — skip this section.

If flashing fails and you suspect a lock, first try the web installer below with the device connected. Only reach for the **Xteink Unlocker** (part of the upstream CrossPoint tooling, at https://crosspointreader.com/#unlock-tool) if the browser genuinely can't see the device at all.

> **Before you touch the unlocker: it only officially lists CrossPoint and CrossInk, not PicoRead.** Once the device is unlocked, use its "Custom .bin" option to flash a PicoRead build instead. Flashing an unsupported firmware on a locked device can brick it permanently or trap it on that firmware with no way back — if USB gets re-locked and the firmware you're stuck with has no OTA path, that's it. Don't experiment here.

## Installing

**Easiest: the web installer.** Go to https://uniquedroid.github.io/unique-esp-web-flasher, connect the device over USB-C, and click "Install PicoRead". It's a small self-hosted tool built on [ESP Web Tools](https://github.com/esphome/esp-web-tools) that always mirrors this repo's latest release — works the same whether it's a first flash or you're overwriting an existing install.

**Already on a PicoRead build?** Skip USB entirely — open **File Transfer** on the device, go to **Firmware Update** in the web menu (or use **Settings > Check for Updates** on-device), and it'll check this repo's latest release, verify the SHA256 checksum, and flash with a progress bar.

**Prefer the generic flasher?** Grab a `firmware.bin` from [Releases](https://github.com/UniqueDroid/PicoRead/releases) (or build your own), head to https://crosspointreader.com/#flash-tools, and use its "Custom .bin" option — PicoRead isn't in that tool's own release list, but any compatible `.bin` works.

**Command line**, if you'd rather script it:

```bash
pip install esptool
# find the port: `dmesg` after connecting on Linux, or on macOS:
#   log stream --predicate 'subsystem == "com.apple.iokit"' --info
esptool.py --chip esp32c3 --port /dev/ttyACM0 --baud 921600 write_flash 0x10000 /path/to/firmware.bin
```

**Want to go back to stock CrossPoint?** Same web flasher, https://crosspointreader.com/#flash-tools, pick the official release instead of a custom `.bin`.

Building it yourself instead of downloading a release? Jump to [Development](#development) below.

---

## RSS Reader

The RSS tile on the home screen manages a small offline feed reader. Subscribed feeds list at the top (or a "no feeds yet" placeholder), with the actions below a divider:

- **Add Feed** — enter a feed URL (RSS 2.0 or Atom) on-device via the on-screen keyboard. Connects to Wi-Fi automatically if you're not already connected.
- **Sync Now** — re-fetches every subscribed feed and saves its articles to the SD card as plain `.txt` files (title, link, then the article text with HTML stripped — and any invisible zero-width Unicode padding some feeds embed stripped too), written one at a time as they're parsed rather than collected in RAM first. No EPUB generation involved — it reuses the existing `.txt` reader, so there's no new reading UI or extra RAM cost.
- **Import from SD Card** — bulk-add feeds from a JSON file at `/rss_feeds_import.json` on the SD card root, no Wi-Fi needed for this step. Handy for setting up a batch of feeds from a computer instead of typing each URL on-device. A sample file is at [`sdcard/rss_feeds_import.json`](./sdcard/rss_feeds_import.json):

  ```json
  {
    "feeds": [
      {
        "url": "https://www.heise.de/rss/heise-atom.xml",
        "title": "heise online"
      },
      {
        "url": "https://www.tagesschau.de/xml/rss2"
      }
    ]
  }
  ```

  `url` is required; `title` is optional — if omitted, the feed URL is used as the display name until the next sync, which fills in the real title from the feed itself.

  A few ready-made feed lists are also included, so you don't have to hunt down URLs yourself:

  | File | Contents |
  |---|---|
  | [`sdcard/rss_feeds_import_NEWS_DE.json`](./sdcard/rss_feeds_import_NEWS_DE.json) | German general news (Tagesschau, SPIEGEL, ZEIT, FAZ, SZ, n-tv, WELT, stern, FOCUS, ZDFheute) |
  | [`sdcard/rss_feeds_import_NEWS_EN.json`](./sdcard/rss_feeds_import_NEWS_EN.json) | English general news (BBC, NYT, The Guardian, CNN, Washington Post, NPR, ABC News, Al Jazeera, TIME, Financial Times) |
  | [`sdcard/rss_feeds_import_TECHNEWS_DE.json`](./sdcard/rss_feeds_import_TECHNEWS_DE.json) | German tech news (heise, Golem, ComputerBase, t3n, CHIP, WinFuture, Caschys Blog, PCGH, Hardwareluxx, Notebookcheck) |
  | [`sdcard/rss_feeds_import_TECHNEWS_EN.json`](./sdcard/rss_feeds_import_TECHNEWS_EN.json) | English tech news (Ars Technica, The Verge, TechCrunch, Wired, Engadget, CNET, Tom's Hardware, VentureBeat, 9to5Mac, ZDNET) |
  | [`sdcard/rss_feeds_import_TECHNEWS_AND_NEWS_DE.json`](./sdcard/rss_feeds_import_TECHNEWS_AND_NEWS_DE.json) | The two German lists above combined (20 feeds) |
  | [`sdcard/rss_feeds_import_TECHNEWS_AND_NEWS_EN.json`](./sdcard/rss_feeds_import_TECHNEWS_AND_NEWS_EN.json) | The two English lists above combined (20 feeds) |

- **Manage Feeds** — lists every feed for one-tap removal, plus a "Delete All Feeds" row to clear everything at once.

Tapping a synced feed opens its first article directly — no folder listing in between. Paging past the first/last page of an article jumps straight into the previous/next one, and the status bar shows your position in the feed ("3 / 12"). Back returns to the RSS overview instead of Home. Synced articles live under `.picoread/rss/<feed-name>/` (folder named after the feed, sanitized for the SD card's filesystem) and follow the same caching philosophy as the rest of the firmware (see [How the caching works](#how-the-caching-works)).

---

## Wikipedia

The Wikipedia tile on the home screen fetches three things from Wikipedia's own REST API, in the language you've set in Settings:

- **Article of the Day** — the current featured article.
- **On This Day** — a digest of historical events for today's date.
- **Random Article** — exactly that.

None of these fetch on their own when selected — tap **Sync Now** first to download all three and save them to the SD card, then read them fully offline afterward (handy for e.g. syncing once before leaving the house). Selecting an entry before ever syncing offers to sync right away instead of just bouncing back to the list. Back returns to the Wikipedia overview instead of Home. Files live under `.picoread/wikipedia/` and are overwritten on each sync — no history is kept.

---

## Custom SD-card fonts

No firmware reflash needed for this one — install your own TTF/OTF as SD-card fonts:

1. Open https://crosspointreader.com/fonts (upstream's font builder — the `.cpfont` output isn't CrossPoint-specific, it works here unchanged).
2. Upload up to four styles, set the family name, sizes, and Unicode range.
3. Drop the generated `.cpfont` files under `/fonts/YourFont/` (or `/.fonts/YourFont/` to keep the SD root tidy) and pick the font from the device's font settings.

Runs through `lib/EpdFont/scripts/fontconvert_sdcard.py` unmodified, so a local build produces byte-identical output.

---

## Documentation

- [User Guide](./USER_GUIDE.md)
- [Web server usage](./docs/webserver.md) / [endpoints reference](./docs/webserver-endpoints.md)
- [Project scope](./SCOPE.md)
- [Contributing docs](./docs/contributing/README.md) (architecture notes, still accurate even though this fork isn't taking PRs)

---

## Development

```bash
git clone --recursive https://github.com/UniqueDroid/PicoRead
cd PicoRead
# forgot --recursive? git submodule update --init --recursive

pio run --target upload
```

You'll need [pioarduino](https://github.com/pioarduino/pioarduino) (CLI or the VS Code plugin), Python 3.8+, `clang-format` 21, and a USB-C cable that actually does data.

Before sending anything anywhere:

```bash
./bin/clang-format-fix
pio check -e default
pio run -e default
```

For live serial debugging: `python3 -m pip install pyserial colorama matplotlib`, then `python3 scripts/debugging_monitor.py` (macOS: pass the device path, e.g. `/dev/cu.usbmodem2101`). Windows may need minor tweaks.

---

## How the caching works

RAM is the whole ballgame on an ESP32-C3 — ~380KB usable, so the firmware leans hard on the SD card instead. First open of a book parses and caches everything under `.picoread/` on the card; every open after that reads straight from cache:

```text
.picoread/
├── epub_<hash>/         one directory per book, keyed by content hash
│   ├── progress.bin     reading position
│   ├── cover.bmp        generated cover
│   ├── book.bin         title/author/spine/TOC
│   ├── css_rules.cache  parsed CSS
│   ├── img_*            rendered image cache
│   └── sections/        per-chapter layout cache
├── settings.json
├── state.json
└── recent.json
```

Deleting, moving, or renaming a book file changes its hash, so the firmware and web UI clean up the old cache automatically; editing files directly on the card from a computer won't, so you can end up with orphaned cache folders that way. If something looks stale or corrupted, deleting `.picoread/` forces a full rebuild. Format details: [file formats document](./docs/file-formats.md).

---

## If PicoRead doesn't do what you want

It's one person's fork, evolving in whatever direction I find interesting — not a general-purpose platform. If you need something outside that direction, the wider CrossPoint ecosystem has plenty of other forks worth a look:

- [CrossInk](https://github.com/uxjulia/CrossInk) — Bionic Reading, guide dots, and a different default font stack (ChareInk/Lexend/Bitter).
- [papyrix-reader](https://github.com/bigbag/papyrix-reader) — FB2/MD support, Arabic script, SD-card themes.
- [crosspoint-reader-cjk](https://github.com/aBER0724/crosspoint-reader-cjk) — built for Chinese/Japanese/Korean.
- [inx](https://github.com/obijuankenobiii/inx) — reworked UI with tabbed navigation.
- [crosspoint-reader-papers3](https://github.com/juicecultus/crosspoint-reader-papers3) — port to the M5Stack Paper S3.
- [t5s3-reader](https://github.com/ShallowGreen123/t5s3-reader) — port to the LilyGO T5 ePaper S3/T5S3.
- ~~[crosspet](https://github.com/trilwu/crosspet)~~ / ~~[PlusPoint](https://github.com/ngxson/pluspoint-reader)~~ — both unmaintained, but worth knowing about.

Building your own hardware rather than buying one? [de-link](https://github.com/iandchasse/de-link) is worth a look too.

Since this is a solo fork, I'm not set up to take contributions the way the upstream project is — but the [contributing docs](./docs/contributing/README.md) are still a good architecture reference if you're poking around the code, and the upstream [ideas board](https://github.com/crosspoint-reader/crosspoint-reader/discussions/categories/ideas) is the right place for feature discussion that isn't specific to my fork.

---

## Credits

Everything foundational here — the reader engine, the activity architecture, the HAL, the years of design work that make this run well on a 380KB-RAM chip — is [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)'s, by Dave Allie and contributors, MIT-licensed (see [LICENSE](./LICENSE)). Go star it, and if you're able to, [chip in for the people actually maintaining it](https://app.royalty.dev/crosspoint-reader/crosspoint-reader) — this fork wouldn't exist without their work.

CrossPoint itself credits [diy-esp32-epub-reader](https://github.com/atomic14/diy-esp32-epub-reader) as its original inspiration.
