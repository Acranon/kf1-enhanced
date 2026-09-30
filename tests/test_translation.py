"""Translation format and launcher contract tests; no retail assets required."""

import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from scripts.english_patch import MAGIC, embed, from_ppf, ppf3_records, replacement_spans


ROOT = Path(__file__).resolve().parents[1]


def record(name=b"KF/TEST.", size=8, spans=((1, b"XY"), (7, b"Z"))):
    return struct.pack("<III", len(name), size, len(spans)) + name + b"".join(
        struct.pack("<II", offset, len(data)) + data for offset, data in spans
    )


def delta(*records):
    return MAGIC + struct.pack("<I", len(records)) + b"".join(records)


def ppf3(*records, blockcheck=True):
    header = b"PPF30" + bytes([2]) + b" " * 50 + bytes([0, int(blockcheck), 0, 0])
    body = b"".join(struct.pack("<Q", offset) + bytes([len(data)]) + data for offset, data in records)
    return header + (bytes(1024) if blockcheck else b"") + body


class PpfConversion(unittest.TestCase):
    RAW, HEADER = 2352, 24

    def at(self, sector, byte):
        return sector * self.RAW + self.HEADER + byte

    def test_maps_sector_data_to_files_and_drops_headers_and_ecc(self):
        layout = {"A.BIN": (20, 3000), "KF/B.DAT": (30, 10)}
        patch = ppf3(
            (self.at(20, 5), b"xy"),              # A.BIN bytes 5-6
            (self.at(21, 0), b"z"),               # second sector: A.BIN byte 2048
            (21 * self.RAW + 2, b"hdr"),          # sector header: not file data
            (self.at(20, 2048), b"ecc"),          # EDC/ECC after the data area
            (self.at(30, 9), b"end"),             # byte 9 is B.DAT's last; 10-11 are padding
            (self.at(99, 0), b"q"),               # sector owned by no file
        )
        payload = from_ppf(patch, layout)
        self.assertEqual(payload, delta(record(b"A.BIN", 3000, ((5, b"xy"), (2048, b"z"))),
                                        record(b"KF/B.DAT", 10, ((9, b"e"),))))

    def test_rejects_non_ppf3_and_truncated_records(self):
        with self.assertRaises(ValueError):
            list(ppf3_records(b"PPF20" + bytes(80)))
        with self.assertRaises(ValueError):
            list(ppf3_records(ppf3((self.at(1, 0), b"abc"))[:-2]))

    def test_patch_without_file_changes_is_rejected(self):
        with self.assertRaises(ValueError):
            from_ppf(ppf3((5, b"hdr")), {"A.BIN": (20, 100)})


class Translation(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="kf-translation-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        (cls.directory / "english_patch.inc").write_text(embed(b""))
        cls.binary = cls.directory / "test"
        subprocess.run([
            "clang++", "-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-ffunction-sections", "-fdata-sections",
            "-Wl,--gc-sections", "-I", str(ROOT / "include"), "-I", str(cls.directory),
            str(ROOT / "tests/translation_regressions.cpp"),
            str(ROOT / "src/platform/assets.cpp"), str(ROOT / "src/platform/translation.cpp"),
            str(ROOT / "src/platform/language.cpp"), "-o", str(cls.binary),
        ], check=True, capture_output=True, text=True)

    def check_patch(self, payload, success=False):
        path = self.directory / "fixture.kfdelta"
        path.write_bytes(payload)
        result = subprocess.run([self.binary, path], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        if success:
            self.assertEqual(result.stdout, b"aXYdefgZ")

    def test_valid_sparse_replacements(self):
        self.check_patch(delta(record()), True)

    def test_every_truncation(self):
        payload = delta(record())
        for length in range(len(payload)):
            with self.subTest(length=length):
                self.check_patch(payload[:length])

    def test_invalid_paths_and_sizes(self):
        for item in [record(name=b"../TEST."), record(name=b"KF/MISSING."),
                     record(name=b"kf/test."), record(name=b"KF/TEST.\0"),
                     record(size=7), record(spans=((8, b"X"),)),
                     record(spans=((0xffffffff, b"X"),)),
                     record(spans=((1, b"XX"), (2, b"Y"))),
                     record(spans=((2, b"X"), (1, b"Y"))), record(spans=((0, b""),))]:
            with self.subTest(record=item):
                self.check_patch(delta(item))

    def test_version_duplicates_and_trailing_data(self):
        payload = delta(record())
        for invalid in [b"BAD!" + payload[4:], payload[:4] + b"\x02" + payload[5:],
                        delta(record(), record()), payload + b"x", delta()]:
            self.check_patch(invalid)

    def test_generator_reconstructs_changed_and_unchanged_bytes(self):
        for before, after in [(b"abcdefgh", b"aXYdefgZ"), (b"a" * 100, b"X" + b"a" * 98 + b"Z"),
                              (b"unchanged", b"unchanged"), (b"", b"")]:
            output = bytearray(before)
            for offset, data in replacement_spans(before, after):
                output[offset:offset + len(data)] = data
            self.assertEqual(output, after)


class RuntimeLanguage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="kf-language-runtime-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.binary = Path(cls.temp.name) / "test"
        subprocess.run([
            "clang++", "-std=c++20", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-I", str(ROOT / "include"),
            str(ROOT / "tests/language_runtime_regressions.cpp"),
            str(ROOT / "src/platform/language_runtime.cpp"),
            str(ROOT / "src/platform/language.cpp"), "-o", str(cls.binary),
        ], check=True, capture_output=True, text=True)

    def test_pending_switch_reuse_and_cleanup(self):
        subprocess.run([self.binary, "switch"], check=True)

    def test_failed_generation_keeps_current_language_and_can_retry(self):
        subprocess.run([self.binary, "failed-generation"], check=True)

    def test_missing_payload_or_original_resources(self):
        subprocess.run([self.binary, "unavailable"], check=True)

    def test_invalid_alternate_keeps_current_language(self):
        subprocess.run([self.binary, "invalid-alternate"], check=True)

    def test_pending_change_can_be_cancelled(self):
        subprocess.run([self.binary, "cancel-pending"], check=True)


class Launcher(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="kf-launch-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.log = self.root / "calls"
        self.game = self.root / "game"
        self.game.write_text('''#!/usr/bin/env python3
import json, os, pathlib, sys
args = sys.argv[1:]
with open(os.environ['TEST_CALLS'], 'a') as log:
    log.write(json.dumps(args) + '\\n')
if '--extract-to' in args:
    if '--language' in args and args[args.index('--language') + 1] == os.environ.get('TEST_FAIL'):
        sys.exit(1)
    target = pathlib.Path(args[args.index('--extract-to') + 1])
    target.mkdir()
    (target / 'test').write_text('verified')
''')
        self.game.chmod(0o700)
        self.disc = self.root / "Japanese disc.iso"
        self.disc.touch()
        self.env = dict(os.environ, XDG_CACHE_HOME=str(self.root / "cache"), TEST_CALLS=str(self.log))
        for key in ["KF_DISC", "KF_LANGUAGE", "TEST_FAIL", "japanese_resources"]:
            self.env.pop(key, None)

    def launch(self, *args, success=True, **env):
        import json
        result = subprocess.run([
            "bash", "-euc", 'game_binary="$1"; launcher="$2"; shift 2; source "$launcher" "$@"',
            "launcher-test", str(self.game), str(ROOT / "scripts/launch.sh"), *args,
        ], env=dict(self.env, **env), capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0 if success else 1, result.stderr)
        return [json.loads(line) for line in self.log.read_text().splitlines()] if self.log.exists() else []

    def test_default_launch_imports_japanese_then_translates(self):
        calls = self.launch(KF_DISC=str(self.disc))
        self.assertEqual(len(calls), 3)
        self.assertEqual(calls[0][:4], ["--language", "ja", "--disc", str(self.disc)])
        self.assertEqual(calls[1][:3], ["--language", "en", "--data"])
        self.assertIn("--japanese-data", calls[2])
        self.assertTrue(calls[2][calls[2].index("--japanese-data") + 1].endswith("/resources"))

    def test_switch_to_english_and_back_without_disc(self):
        self.launch("--language", "ja", KF_DISC=str(self.disc))
        self.disc.unlink()
        self.log.unlink()
        calls = self.launch("--language", "en")
        self.assertEqual(len(calls), 2)
        self.assertNotIn("--disc", calls[0])
        self.log.unlink()
        self.assertEqual(len(self.launch("--language", "ja")), 1)
        self.log.unlink()
        self.assertEqual(len(self.launch("--language", "en")), 1)

    def test_failed_translation_preserves_japanese_and_does_not_publish(self):
        self.launch("--language", "en", success=False, KF_DISC=str(self.disc), TEST_FAIL="en")
        cache = self.root / "cache/kings-field/SLPS-00017"
        self.assertTrue((cache / "resources/test").is_file())
        self.assertFalse((cache / "resources-en").exists())
        self.launch("--language", "ja")

    def test_missing_disc_and_invalid_language(self):
        self.assertEqual(self.launch("--language", "en", success=False), [])
        self.assertEqual(self.launch("--language", "invalid", success=False), [])

    def test_installed_japanese_resources_need_no_disc_or_cache(self):
        installed = self.root / "installed resources"
        installed.mkdir()
        calls = self.launch(KF_LANGUAGE="ja", japanese_resources=str(installed))
        self.assertEqual(calls, [["--data", str(installed), "--language", "ja"]])
        self.assertFalse((self.root / "cache").exists())

    def test_installed_resources_can_prepare_english_and_switch_back(self):
        installed = self.root / "installed resources"
        installed.mkdir()
        calls = self.launch(japanese_resources=str(installed))
        self.assertEqual(len(calls), 2)
        self.assertEqual(calls[0][:4], ["--language", "en", "--data", str(installed)])
        self.assertEqual(calls[1][calls[1].index("--japanese-data") + 1], str(installed))
        self.assertFalse((self.root / "cache/kings-field/SLPS-00017/resources").exists())


if __name__ == "__main__":
    unittest.main()
