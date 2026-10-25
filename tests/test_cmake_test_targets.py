"""Execute the extracted CMake declarations without a compiler or engine build.

The expected inputs preserve the accepted pre-extraction M79–M83 target contract.
Negative controls deliberately remove/change mechanisms instead of counting names.
"""

import pathlib
import re
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
            module = module if module is not None else (ROOT / "cmake/tests/MechanicalTests.cmake").read_text(encoding="utf-8")
            helper = helper if helper is not None else (ROOT / "cmake/JudasTestTargets.cmake").read_text(encoding="utf-8")
            (root / "tests/MechanicalTests.cmake").write_text(module, encoding="utf-8")
            (root / "JudasTestTargets.cmake").write_text(helper, encoding="utf-8")
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
                f'include("{(root / "tests/MechanicalTests.cmake").as_posix()}")\n', encoding="utf-8")
            result = subprocess.run([shutil.which("cmake") or "cmake", "-P", str(script)],
                                    text=True, encoding="utf-8", capture_output=True, timeout=15)
            if not succeeds:
                self.assertNotEqual(result.returncode, 0)
                return result.stderr
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            records = {}
            for line in trace.read_text(encoding="utf-8").splitlines():
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
        self.assertIn("include(cmake/tests/MechanicalTests.cmake)", (ROOT / "CMakeLists.txt").read_text(encoding="utf-8"))
        self.assert_contract(self.declarations())

    def test_missing_target_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text(encoding="utf-8")
        module = module.replace("judas_add_engine_test(judas_structural_tests tests/StructuralTests.cpp WARNINGS -Wpedantic)", "", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_missing_application_sources_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text(encoding="utf-8").replace("APPLICATION WARNINGS", "WARNINGS", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_changed_warning_detected(self):
        module = (ROOT / "cmake/tests/MechanicalTests.cmake").read_text(encoding="utf-8").replace("WARNINGS -Wpedantic", "WARNINGS -Wextra", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(module=module))

    def test_changed_link_owner_detected(self):
        helper = (ROOT / "cmake/JudasTestTargets.cmake").read_text(encoding="utf-8").replace("PRIVATE judas_engine", "PRIVATE wrong_owner", 1)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.declarations(helper=helper))

    def test_unknown_options_and_missing_source_fail(self):
        self.assertIn("Unknown arguments", self.declarations(module="judas_add_engine_test(bad tests/StructuralTests.cpp TYPO)", succeeds=False))
        self.assertIn("Missing test entry point", self.declarations(module="judas_add_engine_test(bad tests/does-not-exist.cpp)", succeeds=False))


class SubsystemTargetContracts(unittest.TestCase):
    """Execute the actual extracted declarations, including platform guards.

    The archived pre-extraction trace supplies minimum retained requirements, not
    an exact copy of the evolving build graph. Source-derived checks cover new
    direct dependencies. Native compilation/platform qualification is separate.
    """

    MODULES = ("LocalViews", "RenderingAssets", "DeveloperWorkflow", "WorldLifecycle")
    COMMANDS = ("add_executable", "target_link_libraries", "judas_warnings",
                "target_include_directories", "target_compile_definitions",
                "set_target_properties")
    PLATFORMS = {
        "linux": {"UNIX": True, "WIN32": False, "ANDROID": False},
        "windows": {"UNIX": False, "WIN32": True, "ANDROID": False},
        "android": {"UNIX": True, "WIN32": False, "ANDROID": True},
    }

    def source(self):
        return "\n".join((ROOT / "cmake/targets" / (name + ".cmake")).read_text(encoding="utf-8")
                         for name in self.MODULES)

    def expected(self, platform):
        import json
        fixture = ROOT / "tests/fixtures/cmake/PreAlphaSubsystemTargets.json"
        return json.loads(fixture.read_text(encoding="utf-8"))["platforms"][platform]

    def declarations(self, platform="linux", source=None, helper=None):
        with tempfile.TemporaryDirectory(prefix="judas-subsystem-targets-") as folder:
            root = pathlib.Path(folder)
            trace = root / "calls.txt"
            module = root / "targets.cmake"
            module.write_text(self.source() if source is None else source, encoding="utf-8")
            implementation = root / "helper.cmake"
            implementation.write_text((ROOT / "cmake/JudasTestTargets.cmake").read_text(encoding="utf-8")
                                      if helper is None else helper, encoding="utf-8")
            script = 'cmake_minimum_required(VERSION 3.20)\n'
            script += f'set(CMAKE_CURRENT_SOURCE_DIR "{ROOT.as_posix()}")\n'
            for name, enabled in self.PLATFORMS[platform].items():
                script += f'set({name} {"TRUE" if enabled else "FALSE"})\n'
            for command in self.COMMANDS:
                script += (f'function({command})\n'
                           f' file(APPEND "{trace.as_posix()}" "{command}|${{ARGV}}\\n")\n'
                           'endfunction()\n')
            script += f'include("{implementation.as_posix()}")\n'
            script += f'include("{module.as_posix()}")\n'
            entry = root / "inspect.cmake"
            entry.write_text(script, encoding="utf-8")
            result = subprocess.run([shutil.which("cmake") or "cmake", "-P", str(entry)],
                                    text=True, encoding="utf-8", capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            return [[line.split("|", 1)[0], line.split("|", 1)[1].split(";")]
                    for line in trace.read_text(encoding="utf-8").splitlines()]

    @staticmethod
    def usage_requirements(args):
        """Normalize scoped CMake usage without depending on call/list order."""
        scope = "UNSCOPED"
        result = set()
        for arg in args:
            if arg in ("PUBLIC", "PRIVATE", "INTERFACE"):
                scope = arg
            else:
                result.add((scope, arg))
        return result

    def assert_contract(self, platform, declarations):
        # The archived trace is a minimum contract, not a duplicate current graph.
        # New dependencies/sources/targets and harmless declaration reordering do
        # not require edits to this fixture. Lost requirements still fail.
        from collections import defaultdict
        actual = defaultdict(list)
        for command, args in declarations:
            actual[(command, args[0])].append(args[1:])
        baseline = self.expected(platform)
        for command, args in baseline:
            key = (command, args[0])
            self.assertIn(key, actual, f"Missing {command} for {args[0]}")
            values = [value for call in actual[key] for value in call]
            if command in ("target_link_libraries", "target_include_directories", "target_compile_definitions"):
                required = self.usage_requirements(args[1:])
                supplied = set().union(*(self.usage_requirements(call) for call in actual[key]))
            elif command == "set_target_properties":
                required = set(zip(args[2::2], args[3::2]))
                # CMake uses the final assignment when a property is set twice.
                supplied = set(dict(pair for call in actual[key]
                                    for pair in zip(call[1::2], call[2::2])).items())
            else:
                required, supplied = set(args[1:]), set(values)
            self.assertFalse(required - supplied,
                             f"{args[0]} lost {command} requirements: {sorted(required - supplied)}")

        all_old_targets = {args[0] for name in self.PLATFORMS
                           for command, args in self.expected(name) if command == "add_executable"}
        required_targets = {args[0] for command, args in baseline if command == "add_executable"}
        current_targets = {target for command, target in actual if command == "add_executable"}
        self.assertEqual(all_old_targets & current_targets, required_targets,
                         "A platform-specific target escaped its platform guard")
        for target in current_targets:
            self.assertEqual(len(actual[("add_executable", target)]), 1, f"Duplicate target {target}")
            sources = actual[("add_executable", target)][0]
            uses_imgui = False
            for source in sources:
                path = ROOT / source
                self.assertTrue(path.is_file(), f"{target}: missing source {source}")
                # These are direct include requirements of the compiled sources,
                # not another hand-maintained list of target/dependency names.
                uses_imgui |= bool(re.search(r'^\s*#\s*include\s*[<"](?:backends/)?imgui[^>"]*[>"]',
                                             path.read_text(encoding="utf-8"), re.MULTILINE))
            if uses_imgui:
                links = set().union(*(self.usage_requirements(call)
                                      for call in actual.get(("target_link_libraries", target), [])))
                self.assertTrue(any(scope != "INTERFACE" and value == "judas_imgui" for scope, value in links),
                                f"{target}: source includes ImGui but has no implementation link to judas_imgui")

    def assert_rejected(self, platform="linux", **changes):
        with self.assertRaises(AssertionError):
            self.assert_contract(platform, self.declarations(platform=platform, **changes))

    def test_required_sources_links_properties_and_platforms(self):
        for platform in self.PLATFORMS:
            with self.subTest(platform=platform):
                self.assert_contract(platform, self.declarations(platform))
        root = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        for name in self.MODULES:
            self.assertIn(f"include(cmake/targets/{name}.cmake)", root)

    def test_valid_additions_and_reordered_link_calls_are_allowed(self):
        candidate = self.source() + "\njudas_add_engine_test(new_test tests/InputTests.cpp)\n"
        candidate = candidate.replace("add_executable(judas_ui_production_tests tests/UIProductionTests.cpp)",
            "add_executable(judas_ui_production_tests tests/UIProductionTests.cpp src/InputSystem.h)")
        # Moving a dependency into another normal link call must not fail a
        # historical list comparison. Additional required libraries are legal.
        original = "target_link_libraries(judas_ui_production_tests PRIVATE judas_world_authoring judas_imgui)"
        replacement = ("target_link_libraries(judas_ui_production_tests PRIVATE judas_imgui)\n"
                       "target_link_libraries(judas_ui_production_tests PRIVATE judas_world_authoring)\n"
                       "target_link_libraries(judas_ui_production_tests PRIVATE new_optional_dependency)")
        self.assertIn(original, candidate)
        candidate = candidate.replace(original, replacement)
        for platform in self.PLATFORMS:
            with self.subTest(platform=platform):
                self.assert_contract(platform, self.declarations(platform=platform, source=candidate))

    def test_missing_and_duplicate_targets_are_detected(self):
        declarations = self.declarations()
        self.assert_rejected(source=self.source().replace("judas_add_engine_test(judas_script_developer_tools_tests tests/ScriptDeveloperToolsTests.cpp)", "", 1))
        declarations.append(next(row for row in declarations if row[0] == "add_executable"))
        with self.assertRaises(AssertionError):
            self.assert_contract("linux", declarations)

    def test_new_target_missing_source_is_detected(self):
        candidate = self.source() + "\nadd_executable(new_broken_test tests/does-not-exist.cpp)\n"
        self.assert_rejected(source=candidate)

    def test_changed_final_language_property_is_detected(self):
        candidate = self.source() + "\nset_target_properties(judas_script_vm_benchmark PROPERTIES C_STANDARD 99)\n"
        self.assert_rejected(source=candidate)

    def test_lost_application_sources_are_detected(self):
        candidate = self.source().replace(" APPLICATION", "", 1)
        self.assertNotEqual(candidate, self.source())
        self.assert_rejected(source=candidate)

    def test_changed_engine_link_is_detected(self):
        helper = (ROOT / "cmake/JudasTestTargets.cmake").read_text(encoding="utf-8")
        helper = helper.replace("PRIVATE judas_engine", "PRIVATE wrong_owner", 1)
        self.assert_rejected(helper=helper)

    def test_lost_native_editor_input_dependency_is_detected(self):
        original = "target_link_libraries(judas_ui_production_tests PRIVATE judas_world_authoring judas_imgui)"
        candidate = self.source().replace(original, original.replace(" judas_imgui", ""), 1)
        self.assertNotEqual(candidate, self.source())
        for platform in self.PLATFORMS:
            with self.subTest(platform=platform):
                self.assert_rejected(platform, source=candidate)

    def test_interface_only_imgui_dependency_is_detected(self):
        original = "target_link_libraries(judas_ui_production_tests PRIVATE judas_world_authoring judas_imgui)"
        candidate = self.source().replace(original,
            "target_link_libraries(judas_ui_production_tests PRIVATE judas_world_authoring INTERFACE judas_imgui)", 1)
        self.assertNotEqual(candidate, self.source())
        self.assert_rejected(source=candidate)

    def test_lost_rendering_definition_is_detected(self):
        candidate = self.source().replace("PRIVATE JUDAS_M92", "PRIVATE WRONG_DEFINITION", 1)
        self.assertNotEqual(candidate, self.source())
        self.assert_rejected(source=candidate)

    def test_lost_windows_socket_link_is_detected(self):
        candidate = self.source().replace("if(WIN32)", "if(FALSE)", 1)
        self.assertNotEqual(candidate, self.source())
        self.assert_rejected("windows", source=candidate)

    def test_wrong_platform_guard_is_detected(self):
        candidate = self.source().replace("if(UNIX AND NOT ANDROID)", "if(UNIX)", 1)
        self.assertNotEqual(candidate, self.source())
        self.assert_rejected("android", source=candidate)


if __name__ == "__main__":
    unittest.main()
