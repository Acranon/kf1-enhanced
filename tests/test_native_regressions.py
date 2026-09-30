"""Run with `nix develop --command python3 -m unittest discover -s tests`."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class NativeRegressions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix="kf-native-regressions-")
        cls.addClassCleanup(cls.directory.cleanup)
        for name in ["native_regressions", "lighting_regressions", "resource_failures",
                     "cutscene_resources", "collision_results", "map_grids", "menu_outcomes"]:
            subprocess.run(
                [
                    "clang++", "-std=c++20", "-O1", "-g", "-fno-rtti",
                    "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
                    "-fsanitize=address,undefined", "-ftrivial-auto-var-init=pattern",
                    "-I", str(ROOT / "include"), str(ROOT / "tests" / f"{name}.cpp"),
                    *([str(source) for source in sorted((ROOT / "codecs").glob("*.cpp"))] if name == "cutscene_resources" else []),
                    "-o", str(Path(cls.directory.name) / name),
                ],
                check=True, text=True,
            )
        cls.binary = Path(cls.directory.name) / "native_regressions"

    def test_map_grid_copy_alignment_and_bounds(self):
        for scenario in ("aligned", "unaligned", "truncated"):
            with self.subTest(scenario=scenario):
                result = subprocess.run([Path(self.directory.name) / "map_grids", scenario],
                                        capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 77 if scenario == "truncated" else 0,
                                 result.stderr)
                self.assertEqual(result.stderr, "Truncated map grids\n" if scenario == "truncated" else "")

    def test_cutscene_resource_boundaries(self):
        binary = Path(self.directory.name) / "cutscene_resources"
        for consumer in ["lit", "map", "unlit", "placements"]:
            cases = ["valid", "truncated"]
            cases += (["unterminated", "outside-grid", "unaligned", "fixed-height"] if consumer == "placements"
                      else ["short-layout", "vertex"])
            if consumer in ["lit", "map"]:
                cases.append("normal")
            for case in cases:
                with self.subTest(consumer=consumer, case=case):
                    result = subprocess.run([binary, consumer, case], capture_output=True,
                                            text=True, timeout=10)
                    self.assertEqual(result.returncode, 0 if case in ("valid", "unaligned", "fixed-height") else 77,
                                     result.stderr)

    def test_menu_outcomes_and_session_restoration(self):
        subprocess.run([Path(self.directory.name) / "menu_outcomes"], check=True)

    def test_collision_results_and_door_probes(self):
        subprocess.run([Path(self.directory.name) / "collision_results"], check=True)

    def test_lighting_preserves_translation_and_supports_aliasing(self):
        subprocess.run([Path(self.directory.name) / "lighting_regressions"], check=True)

    def test_required_file_failure_reaches_host_error_handler(self):
        result = subprocess.run(
            [Path(self.directory.name) / "resource_failures", "required", self.directory.name],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 77, result.stderr)
        self.assertEqual(result.stderr, "Resource missing.dat: not found\n"
                         "Cannot load required resource KF/missing.dat.\n")

    def test_optional_file_failure_still_returns_to_caller(self):
        result = subprocess.run(
            [Path(self.directory.name) / "resource_failures", "optional", self.directory.name],
            capture_output=True, text=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stderr, "Resource missing.dat: not found\n")

    def run_case(self, mode, code, message=""):
        result = subprocess.run([self.binary, mode], capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, code, result.stderr)
        self.assertEqual(result.stderr, message)

    def test_original_common_table_spans(self):
        self.run_case("valid", 0)

    def test_truncated_armor_file(self):
        self.run_case("armor-truncated", 1,
                      "Truncated COM/COM.DAT armor table: need 1176 bytes, have 1175.\n")

    def test_truncated_object_file(self):
        self.run_case("objects-truncated", 1,
                      "Truncated COM/COM.DAT map-object table: need 1280 bytes, have 1279.\n")

    def test_ordinary_records_remain_chunk_bounded(self):
        self.run_case("chunk-only", 1, "Truncated armor chunk: need 1176 bytes, have 756.\n")

    def test_alignment_check(self):
        self.run_case("unaligned", 1, "Unaligned test record: requires 4-byte alignment.\n")

    def test_stream_tail_still_validates_chunk_header(self):
        self.run_case("invalid-header", 1, "Truncated resource chunk payload\n")

    def test_reset_scalars_arrays_nested_arrays_and_pointers(self):
        self.run_case("reset", 0)


if __name__ == "__main__":
    unittest.main()
