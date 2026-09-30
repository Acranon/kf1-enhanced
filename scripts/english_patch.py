"""Build the English translation delta, or embed it for the native/WASM build.

The delta contains replacement spans inside resource files, never a disc image.
It is derived locally, either from the translator's original PPF release (the
Nix build fetches it) or from two verified resource trees. Do not distribute
the generated payload: the translation's redistribution permission is unsettled.
"""

import argparse
import hashlib
from pathlib import Path
import re
import struct


MAGIC = b"KFEN\x01\0\0\0"
JAPANESE = "450b9f09ca34bedc1c8bc150e01b79bfe108c700dad2b2783246b926953deee2"
ENGLISH = "697b2b80d13a3e6e54b2d72f90a49e29ae6a31d6d45970b40aee908b94208595"


def read_tree(root, expected):
    files = {}
    for path in root.rglob("*"):
        if path.is_symlink():
            raise ValueError(f"Symlink in resource tree: {path}")
        if not path.is_file():
            continue
        name = path.relative_to(root).as_posix()
        if len(name) >= 128 or not re.fullmatch(r"[A-Z0-9_.-]+(?:/[A-Z0-9_.-]+)*", name):
            raise ValueError(f"Noncanonical resource path: {name}")
        files[name] = path.read_bytes()
    digest = hashlib.sha256()
    for name, data in sorted(files.items()):
        digest.update(struct.pack("<II", len(name), len(data)))
        digest.update(name.encode("ascii"))
        digest.update(data)
    if len(files) != 428 or digest.hexdigest() != expected:
        raise ValueError(f"Resource tree does not match the supported revision: {root}")
    return files


def replacement_spans(before, after):
    if len(before) != len(after):
        raise ValueError("This translation must preserve resource sizes")
    spans = []
    start = end = None
    for offset, (a, b) in enumerate(zip(before, after)):
        if a == b:
            continue
        # Bridging a short unchanged gap costs less than another span header.
        if end is not None and offset - end > 8:
            spans.append((start, after[start:end]))
            start = None
        if start is None:
            start = offset
        end = offset + 1
    if start is not None:
        spans.append((start, after[start:end]))
    return spans


def create(japanese, english):
    before, after = read_tree(japanese, JAPANESE), read_tree(english, ENGLISH)
    if before.keys() != after.keys():
        raise ValueError("Translation changed the file list")
    records = []
    for name in sorted(before):
        spans = replacement_spans(before[name], after[name])
        if not spans:
            continue
        record = bytearray(struct.pack("<III", len(name), len(after[name]), len(spans)))
        record.extend(name.encode("ascii"))
        for offset, data in spans:
            record.extend(struct.pack("<II", offset, len(data)))
            record.extend(data)
        records.append(record)
    return MAGIC + struct.pack("<I", len(records)) + b"".join(records)


RAW_SECTOR, SECTOR_DATA_OFFSET, SECTOR_DATA = 2352, 24, 2048


def disc_layout(image):
    """Return {path: (first_sector, size)} from a MODE2/2352 ISO9660 disc image."""
    def sector(lba):
        start = lba * RAW_SECTOR + SECTOR_DATA_OFFSET
        return image[start:start + SECTOR_DATA]

    descriptor = sector(16)
    if descriptor[1:6] != b"CD001":
        raise ValueError("Not a MODE2/2352 ISO9660 image")
    files = {}

    def walk(lba, size, prefix):
        data = b"".join(sector(lba + i) for i in range((size + SECTOR_DATA - 1) // SECTOR_DATA))
        position = 0
        while position < size:
            length = data[position]
            if length == 0:
                position = (position // SECTOR_DATA + 1) * SECTOR_DATA
                continue
            record = data[position:position + length]
            extent, extent_size = struct.unpack_from("<I", record, 2)[0], struct.unpack_from("<I", record, 10)[0]
            name = record[33:33 + record[32]]
            if name not in (b"\0", b"\1"):
                name = name.decode("ascii").split(";")[0]
                if record[25] & 2:
                    walk(extent, extent_size, prefix + name + "/")
                else:
                    files[prefix + name] = (extent, extent_size)
            position += length

    root = descriptor[156:190]
    walk(struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0], "")
    return files


def read_layout(path):
    layout = {}
    for line in path.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        name, sector, size = line.split("\t")
        layout[name] = (int(sector), int(size))
    if len(layout) != 428:
        raise ValueError(f"Layout must list the 428 SLPS-00017 files: {path}")
    return layout


def ppf3_records(patch):
    """Yield (image offset, replacement bytes) from a PPF 3.0 patch."""
    if patch[:5] != b"PPF30" or patch[5] != 2:
        raise ValueError("Not a PPF 3.0 patch")
    position = 0x3C + (1024 if patch[0x39] else 0)
    undo = patch[0x3A]
    end = patch.find(b"@BEGIN_FILE_ID.DIZ")
    end = len(patch) if end < 0 else end
    while position < end:
        if position + 9 > end:
            raise ValueError("Truncated PPF record")
        offset, length = struct.unpack_from("<Q", patch, position)[0], patch[position + 8]
        position += 9
        if position + length > end:
            raise ValueError("Truncated PPF record")
        yield offset, patch[position:position + length]
        position += length * (2 if undo else 1)


def from_ppf(patch, layout):
    """Map an image-level PPF onto resource files; bytes outside file data are dropped.

    Sector headers and EDC/ECC bytes are not part of extracted files. The result
    is only accepted at runtime after the full English identity check.
    """
    sectors = {}
    for name, (first, size) in layout.items():
        for index in range(max(1, (size + SECTOR_DATA - 1) // SECTOR_DATA)):
            sectors[first + index] = (name, index, size)
    changed = {}
    for offset, data in ppf3_records(patch):
        for delta, value in enumerate(data):
            sector, within = divmod(offset + delta, RAW_SECTOR)
            if not SECTOR_DATA_OFFSET <= within < SECTOR_DATA_OFFSET + SECTOR_DATA or sector not in sectors:
                continue
            name, index, size = sectors[sector]
            file_offset = index * SECTOR_DATA + within - SECTOR_DATA_OFFSET
            if file_offset < size:
                changed.setdefault(name, {})[file_offset] = value
    records = []
    for name in sorted(changed):
        spans = []
        for file_offset in sorted(changed[name]):
            if spans and spans[-1][0] + len(spans[-1][1]) == file_offset:
                spans[-1][1].append(changed[name][file_offset])
            else:
                spans.append((file_offset, bytearray([changed[name][file_offset]])))
        record = bytearray(struct.pack("<III", len(name), layout[name][1], len(spans)))
        record.extend(name.encode("ascii"))
        for file_offset, data in spans:
            record.extend(struct.pack("<II", file_offset, len(data)))
            record.extend(data)
        records.append(record)
    if not records:
        raise ValueError("The PPF changes no resource file")
    return MAGIC + struct.pack("<I", len(records)) + b"".join(records)


def embed(payload):
    rows = [",".join(str(byte) for byte in payload[i:i + 32]) + ","
            for i in range(0, len(payload), 32)]
    return ("// Generated; do not commit.\n#include <array>\n"
            f"static constexpr std::array<unsigned char, {len(payload)}> english_patch_data = {{\n"
            + "\n".join(rows) + "\n};\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("create")
    build.add_argument("--japanese", type=Path, required=True)
    build.add_argument("--english", type=Path, required=True)
    build.add_argument("--output", type=Path, required=True)
    ppf = commands.add_parser("from-ppf", help="convert the translator's PPF release")
    ppf.add_argument("--ppf", type=Path, required=True)
    ppf.add_argument("--layout", type=Path, required=True)
    ppf.add_argument("--output", type=Path, required=True)
    table = commands.add_parser("layout", help="record the Japanese disc's file extents")
    table.add_argument("--disc", type=Path, required=True, help="Japanese SLPS-00017 BIN (MODE2/2352)")
    table.add_argument("--output", type=Path, required=True)
    cpp = commands.add_parser("embed")
    cpp.add_argument("--input", type=Path)
    cpp.add_argument("--optional", action="store_true", help="treat a missing --input as no payload")
    cpp.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.command == "create":
        payload = create(args.japanese, args.english)
        args.output.write_bytes(payload)
        print(f"Created local English delta: {len(payload)} bytes")
    elif args.command == "from-ppf":
        payload = from_ppf(args.ppf.read_bytes(), read_layout(args.layout))
        args.output.write_bytes(payload)
        print(f"Created local English delta from PPF: {len(payload)} bytes")
    elif args.command == "layout":
        layout = disc_layout(args.disc.read_bytes())
        if len(layout) != 428:
            raise ValueError("The disc does not list the 428 SLPS-00017 files")
        rows = [f"{name}\t{sector}\t{size}" for name, (sector, size) in sorted(layout.items())]
        args.output.write_text("# SLPS-00017 file extents: path, first 2352-byte sector, size in bytes.\n"
                               + "\n".join(rows) + "\n")
    else:
        present = args.input is not None and (args.input.is_file() or not args.optional)
        text = embed(args.input.read_bytes() if present else b"")
        if not args.output.is_file() or args.output.read_text() != text:
            args.output.write_text(text)
            print("English translation payload " + ("embedded" if present else "absent: Japanese only"))


if __name__ == "__main__":
    main()
