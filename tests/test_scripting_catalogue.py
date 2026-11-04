"""The configure-time catalogue must reject ambiguous or unsafe module wiring."""
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
GENERATOR = REPOSITORY / "cmake/JudasScripting.cmake"


@unittest.skipUnless(shutil.which("cmake"), "CMake is required for catalogue generation")
class ScriptingCatalogueTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="judas-script-catalogue-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / "output"
        self.catalogue = {
            "schema": 1,
            "modules": [
                {"name": "First", "source": "src/First.cpp", "registration": "RegisterFirst"},
                {"name": "Second", "source": "src/Second.cpp", "registration": "RegisterSecond"},
            ],
            "libraries": [
                {"name": "First", "source": "src/First.h", "symbol": "LibraryParts::First"},
                {"name": "Second", "source": "src/Second.h", "symbol": "LibraryParts::Second"},
            ],
            "valueSources": ["src/Values.cpp"],
        }
        for source in ["src/scripting/ScriptLibrary.cpp", "src/Values.cpp"] + [
            row["source"] for kind in ("modules", "libraries") for row in self.catalogue[kind]
        ]:
            file = self.root / source
            file.parent.mkdir(parents=True, exist_ok=True)
            file.write_text("// Fixture source; no compilation required.\n", encoding="utf-8")
        self.manifest = self.root / "src/scripting/Modules.json"
        self.runner = self.root / "generate.cmake"
        self.runner.write_text(
            "cmake_minimum_required(VERSION 3.22)\n"
            f'set(CMAKE_CURRENT_SOURCE_DIR "{self.root.as_posix()}")\n'
            f'set(CMAKE_CURRENT_BINARY_DIR "{self.output.as_posix()}")\n'
            f'include("{GENERATOR.as_posix()}")\n', encoding="utf-8"
        )

    def generate(self, catalogue=None):
        self.manifest.write_text(json.dumps(catalogue or self.catalogue), encoding="utf-8")
        return subprocess.run(
            ["cmake", "-P", str(self.runner)],
            capture_output=True, text=True, encoding="utf-8", errors="strict", timeout=15,
        )

    def rejects(self, catalogue, diagnostic):
        result = self.generate(catalogue)
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn(diagnostic, result.stdout + result.stderr)
        self.assertFalse((self.output / "generated/scripting/ScriptLibraryAppend.inc").exists(),
                         "invalid catalogues must not publish generated registration/library files")

    def test_valid_catalogue_preserves_module_and_library_order(self):
        result = self.generate()
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        generated = self.output / "generated/scripting"
        self.assertEqual((generated / "ScriptModuleRegistrations.inc").read_text(encoding="utf-8"),
                         "JUDAS_SCRIPT_MODULE(RegisterFirst)\nJUDAS_SCRIPT_MODULE(RegisterSecond)\n")
        self.assertEqual((generated / "ScriptLibraryAppend.inc").read_text(encoding="utf-8"),
                         "        source.append(LibraryParts::First);\n"
                         "        source.append(LibraryParts::Second);\n")
        self.assertEqual((generated / "ScriptLibraryIncludes.inc").read_text(encoding="utf-8"),
                         f'#include "{self.root.as_posix()}/src/First.h"\n'
                         f'#include "{self.root.as_posix()}/src/Second.h"\n')

    def test_missing_native_library_and_value_sources_are_rejected(self):
        for kind in ("modules", "libraries", "valueSources"):
            with self.subTest(kind=kind):
                broken = copy.deepcopy(self.catalogue)
                if kind == "valueSources":
                    broken[kind][0] = "src/Missing.cpp"
                else:
                    broken[kind][0]["source"] = "src/Missing.cpp"
                self.rejects(broken, "Invalid Judas scripting source")

    def test_duplicate_native_name_source_and_registration_are_rejected(self):
        for field, message in (("name", "duplicate Judas scripting modules name"),
                               ("source", "Duplicate Judas scripting source"),
                               ("registration", "duplicate Judas scripting registration")):
            with self.subTest(field=field):
                broken = copy.deepcopy(self.catalogue)
                broken["modules"][1][field] = broken["modules"][0][field]
                self.rejects(broken, message)

    def test_duplicate_library_name_source_and_symbol_are_rejected(self):
        for field, message in (("name", "duplicate Judas scripting libraries name"),
                               ("source", "Duplicate Judas scripting source"),
                               ("symbol", "duplicate Judas scripting library symbol")):
            with self.subTest(field=field):
                broken = copy.deepcopy(self.catalogue)
                broken["libraries"][1][field] = broken["libraries"][0][field]
                self.rejects(broken, message)

    def test_duplicate_value_sources_are_rejected(self):
        broken = copy.deepcopy(self.catalogue)
        broken["valueSources"].append(broken["valueSources"][0])
        self.rejects(broken, "Duplicate Judas scripting value source")

    def test_unsafe_or_noncanonical_paths_are_rejected(self):
        paths = [str(self.root / "src/First.cpp"), "src/../src/First.cpp",
                 "src/./First.cpp", "src//First.cpp", r"src\First.cpp",
                 "src/First.cpp;other", "src/First.cpp\nother", "src", "src/"]
        for kind in ("modules", "libraries", "valueSources"):
            for source in paths:
                with self.subTest(kind=kind, source=source):
                    broken = copy.deepcopy(self.catalogue)
                    if kind == "valueSources":
                        broken[kind][0] = source
                    else:
                        broken[kind][0]["source"] = source
                    self.rejects(broken, "Invalid Judas scripting source")

    def test_invalid_registration_names_and_library_symbols_are_rejected(self):
        for kind, field, value in (("modules", "name", "not a module"),
                                   ("modules", "registration", "Register;Other"),
                                   ("libraries", "name", ""),
                                   ("libraries", "symbol", "LibraryParts::")):
            with self.subTest(kind=kind, field=field):
                broken = copy.deepcopy(self.catalogue)
                broken[kind][0][field] = value
                self.rejects(broken, "Invalid or duplicate Judas scripting")

    def test_unknown_schema_is_rejected(self):
        broken = copy.deepcopy(self.catalogue)
        broken["schema"] = 2
        self.rejects(broken, "Unsupported Judas scripting catalogue schema")


if __name__ == "__main__":
    unittest.main()
