"""Verify real disc conversion against a separately extracted English reference.

Run in nix develop. All outputs and deliberately damaged fixtures stay in a
temporary directory; neither supplied tree nor disc is modified.
"""

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--disc", type=Path, required=True)
    parser.add_argument("--english-reference", type=Path, required=True)
    args = parser.parse_args()
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")

    def run(*options, success=True):
        result = subprocess.run([str(args.binary.resolve()), *map(str, options)],
                                env=environment, capture_output=True, text=True, timeout=60)
        if result.returncode != (0 if success else 1):
            raise RuntimeError(result.stdout + result.stderr)
        if "AddressSanitizer" in result.stderr or "runtime error:" in result.stderr:
            raise RuntimeError(result.stderr)
        return result

    def contents(root):
        return {p.relative_to(root).as_posix(): p.read_bytes()
                for p in root.rglob("*") if p.is_file()}

    with tempfile.TemporaryDirectory(prefix="kf-real-import-") as temporary:
        root = Path(temporary)
        japanese, english, prepared = [root / name for name in ["ja", "en-disc", "en-tree"]]
        reference = contents(args.english_reference)
        run("--disc", args.disc, "--language", "ja", "--extract-to", japanese, "--extract-only")
        original = contents(japanese)
        run("--disc", args.disc, "--extract-to", english, "--extract-only")
        assert contents(english) == reference
        run("--data", japanese, "--language", "en", "--extract-to", prepared, "--extract-only")
        assert contents(prepared) == reference
        assert contents(japanese) == original
        print("Disc and directory conversions match all 428 English reference files byte for byte.")
        # Output must be exclusive, including an output equal to the input directory.
        for destination in [prepared, japanese]:
            run("--data", japanese, "--language", "en", "--extract-to", destination,
                "--extract-only", success=False)
        assert contents(prepared) == reference and contents(japanese) == original
        corrupt = root / "corrupt"
        shutil.copytree(japanese, corrupt)
        path = corrupt / "KF/COM/COM.DAT"
        data = bytearray(path.read_bytes())
        data[-1] ^= 1
        path.write_bytes(data)
        output = root / "rejected"
        result = run("--data", corrupt, "--language", "en", "--extract-to", output,
                     "--extract-only", success=False)
        assert "do not match" in result.stderr and not output.exists()
        path.unlink()
        path.symlink_to(japanese / "KF/COM/COM.DAT")
        run("--data", corrupt, "--language", "en", "--extract-to", output,
            "--extract-only", success=False)
        assert not output.exists()
        print("Corrupt input, symlink input, and existing outputs rejected; originals unchanged.")


if __name__ == "__main__":
    main()
