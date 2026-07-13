#!/usr/bin/env python3
"""Converts a FreeDict StarDict dictionary into PicoRead's on-device
dictionary format (fixed-width index + a copy of the definition text).

Why not use the StarDict .idx directly on-device: its records are
variable-length (word bytes + 8-byte offset/length), which makes an O(1)
random-access binary search impossible without either loading the whole
index into RAM (megabytes - doesn't fit in the ESP32-C3's ~380KB) or a
fragile byte-offset probe that can misparse the middle of an offset/length
field as a word terminator (both offset and length are 4-byte big-endian
integers that legitimately contain 0x00 bytes). A fixed-width record
sidesteps this entirely: any record index maps directly to a byte offset,
so binary search is a plain seek - no resync heuristics needed.

Usage:
    python3 convert_freedict.py <extracted-stardict-dir> <output-dir>

<extracted-stardict-dir> must contain the .ifo/.idx(.gz)/.dict(.dz) trio
from a FreeDict "stardict" release (e.g. freedict-eng-deu-*.stardict.tar.xz,
already untarred). Produces <output-dir>/{dict.json,index.didx,entries.dict}.
"""

import gzip
import json
import os
import struct
import sys

WORD_SLOT_SIZE = 48  # bytes; see DictionaryIndex.h - must match the firmware
RECORD_SIZE = WORD_SLOT_SIZE + 4 + 4  # word slot + uint32 offset + uint32 length (little-endian)


def read_maybe_gzipped(path_no_ext, extensions):
    for ext in extensions:
        p = path_no_ext + ext
        if os.path.exists(p):
            with open(p, "rb") as f:
                data = f.read()
            if ext.endswith((".gz", ".dz")):
                data = gzip.decompress(data)
            return data
    raise FileNotFoundError(f"None of {[path_no_ext + e for e in extensions]} found")


def parse_ifo(text):
    info = {}
    for line in text.splitlines():
        if "=" in line:
            key, _, value = line.partition("=")
            info[key.strip()] = value.strip()
    return info


def parse_idx_records(data):
    """Yields (word_bytes, offset, length) - offset/length are in the
    ORIGINAL StarDict .dict file, reused as-is since we copy that file
    through unchanged."""
    pos = 0
    n = len(data)
    while pos < n:
        nul = data.index(0, pos)
        word = data[pos:nul]
        offset, length = struct.unpack(">II", data[nul + 1 : nul + 9])
        yield word, offset, length
        pos = nul + 9


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)

    src_dir, out_dir = sys.argv[1], sys.argv[2]
    basename = None
    for f in os.listdir(src_dir):
        if f.endswith(".ifo"):
            basename = f[: -len(".ifo")]
            break
    if not basename:
        print(f"No .ifo file found in {src_dir}", file=sys.stderr)
        sys.exit(1)

    base_path = os.path.join(src_dir, basename)
    ifo_text = read_maybe_gzipped(base_path, [".ifo"]).decode("utf-8")
    info = parse_ifo(ifo_text)

    idx_data = read_maybe_gzipped(base_path, [".idx", ".idx.gz"])
    dict_data = read_maybe_gzipped(base_path, [".dict", ".dict.dz"])

    same_type = info.get("sametypesequence", "")
    is_html = same_type == "h"

    os.makedirs(out_dir, exist_ok=True)

    kept = 0
    skipped_long = 0
    with open(os.path.join(out_dir, "index.didx"), "wb") as out:
        for word, offset, length in parse_idx_records(idx_data):
            if len(word) > WORD_SLOT_SIZE:
                # Only realistic loss: a handful of full example-sentence
                # "headwords" some FreeDict/Ding entries carry (see the
                # library README) - never matched by the reader's
                # single-word lookup cursor anyway.
                skipped_long += 1
                continue
            slot = word + b"\x00" * (WORD_SLOT_SIZE - len(word))
            out.write(slot)
            out.write(struct.pack("<II", offset, length))
            kept += 1

    with open(os.path.join(out_dir, "entries.dict"), "wb") as out:
        out.write(dict_data)

    meta = {
        "bookName": info.get("bookname", basename),
        "wordCount": kept,
        "isHtml": is_html,
    }
    with open(os.path.join(out_dir, "dict.json"), "w", encoding="utf-8") as out:
        json.dump(meta, out, ensure_ascii=False, indent=2)

    print(f"{basename}: {kept} entries kept, {skipped_long} skipped (word > {WORD_SLOT_SIZE} bytes)")
    print(f"Wrote {out_dir}/{{dict.json,index.didx,entries.dict}}")


if __name__ == "__main__":
    main()
