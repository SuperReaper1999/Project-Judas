"""Execute the extracted CMake declarations without a compiler or engine build.

The expected inputs preserve the accepted pre-extraction M79–M83 target contract.
Negative controls deliberately remove/change mechanisms instead of counting names.
"""

import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
APPLICATION_SOURCES = ["src/Application.cpp", "src/RuntimeOptions.cpp",
                       "src/RuntimeDiagnostics.cpp", "src/TestHarness.cpp"]
TARGETS = [
    ("judas_structural_tests", "StructuralTests", False),
    ("judas_structural_application_tests", "StructuralApplicationTests", True),
    ("judas_bending_tests", "BendingTests", False),
    ("judas_bending_application_tests", "BendingApplicationTests", True),
    ("judas_wheel_tests", "WheelTests", False),
    ("judas_wheel_script_tests", "WheelScriptTests", False),
    ("judas_wheel_lifecycle_tests", "WheelLifecycleTests", False),
    ("judas_wheel_application_tests", "WheelApplicationTests", True),
    ("judas_powertrain_tests", "PowertrainTests", False),
    ("judas_powertrain_script_tests", "PowertrainScriptTests", False),
    ("judas_powertrain_lifecycle_tests", "PowertrainLifecycleTests", False),
    ("judas_powertrain_application_tests", "PowertrainApplicationTests", True),
    ("judas_mechanical_damage_tests", "MechanicalDamageTests", False),
    ("judas_mechanical_connection_tests", "MechanicalConnectionTests", False),
    ("judas_mechanical_damage_application_tests", "MechanicalDamageApplicationTests", True),
    ("judas_mechanical_crash_application_tests", "MechanicalCrashApplicationTests", True),
    ("judas_vehicle_damage_application_tests", "MechanicalVehicleCrashApplicationTests", True),
    ("judas_mechanical_damage_script_tests", "MechanicalDamageScriptTests", False),
    ("judas_section_material_tests", "SectionMaterialTests", False),
    ("judas_coarse_crash_application_tests", "CoarseCrashApplicationTests", True),
]


class CMakeTestTargets(unittest.TestCase):
    def declarations(self, module=None, helper=None, succeeds=True):
        with tempfile.TemporaryDirectory(prefix="judas-target-contract-") as folder:
            root = pathlib.Path(folder)
            (root / "tests").mkdir()
            module = module if module is not None else (ROOT / "cmake/tests/MechanicalTests.cmake").read_text()
            helper = helper if helper is not None else (ROOT / "cmake/JudasTestTargets.cmake").read_text()
            (root / "tests/MechanicalTests.cmake").write_text(module)
            (root / "JudasTestTargets.cmake").write_text(helper)
            trace = root / "calls.txt"
            script = root / "inspect.cmake"
            script.write_text('cmake_minimum_required(VERSION 3.20)\n'
                f'set(CMAKE_CURRENT_SOURCE_DIR "{ROOT.as_posix()}")\n'
                f'set(trace "{trace.as_posix()}")\n'
                'function(add_executable target)\n'
                ' file(APPEND "${trace}" "sources|${target}|${ARGN}\\n")\nendfunction()\n'
                'function(target_link_libraries target)\n'
                ' file(APPEND "${trace}" "links|${target}|${ARGN}\\n")\nendfunction()\n'
                'function(judas_warnings target)\n'
                ' file(APPEND "${trace}" "warnings|${target}|${ARGN}\\n")\nendfunction()\n'
                f'include("{(root / "JudasTestTargets.cmake").as_posix()}")\n'
                f'include("{(root / "tests/MechanicalTests.cmake").as_posix()}")\n')
            result = subprocess.run([shutil.which("cmake") or "cmake", "-P", str(script)],
                                    text=True, capture_output=True, timeout=15)
            if not succeeds:
                self.assertNotEqual(result.returncode, 0)
                return result.stderr
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            records = {}
            for line in trace.read_text().splitlines():
                kind, target, values = line.split("|", 2)
                self.assertNotIn(kind, records.setdefault(target, {}), "duplicate target declaration")
                records[target][kind] = values.split(";") if values else []
            return records

    def assert_contract(self, records):
        self.assertEqual(list(records), [target for target, _, _ in TARGETS], "target order/names changed")
        for target, main, application in TARGETS:
            self.assertEqual(records[target], {
                "sources": [f"tests/{main}.cpp"] + (APPLICATION_SOURCES if application else []),
                "links": ["PRIVATE", "judas_engine"], "warnings": ["-Wpedantic"]}, target)

    def test_exact_sources_links_warnings_and_order(self):
        self.assertIn("include(cmake/tests/MechanicalTests.cmake)", (ROOT / "CMakeLists.txt").read_text())
        self.assert_contract(self.declarations())

    def test_missing_target_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text()
        module = module.replace("judas_add_engine_test(judas_structural_tests tests/StructuralTests.cpp WARNINGS -Wpedantic)", "", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_missing_application_sources_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text().replace("APPLICATION WARNINGS", "WARNINGS", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_changed_warning_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text().replace("WARNINGS -Wpedantic", "WARNINGS -Wextra", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_changed_link_owner_detected(self):
        helper = (ROOT / "cmake/JudasTestTargets.cmake").read_text().replace("PRIVATE judas_engine", "PRIVATE wrong_owner", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(helper=helper))

    def test_unknown_options_and_missing_source_fail(self):
        self.assertIn("Unknown arguments", self.declarations(module="judas_add_engine_test(bad tests/StructuralTests.cpp TYPO)", succeeds=False))
        self.assertIn("Missing test entry point", self.declarations(module="judas_add_engine_test(bad tests/does-not-exist.cpp)", succeeds=False))


if __name__ == "__main__":
    unittest.main()
