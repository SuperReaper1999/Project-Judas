"""Exercise the actual CTest gate registration without building Judas.

These configure-only projects declare the real manifest's target names, then
include the production registration module. They check executable invocations,
labels, serial execution and known-failure admission, not source-string counts.
"""

import json
import fnmatch
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'cmake/JudasValidation.cmake'
MANIFEST = json.loads((ROOT / 'tools/validation/suites.json').read_text(encoding="utf-8"))


class ValidationRegistrationTests(unittest.TestCase):
    def configure(self, include_known=False, omit_target=None, module=None, generator=None,
                  channel='Experimental'):
        with tempfile.TemporaryDirectory(prefix='judas-ctest-contract-') as directory:
            source = Path(directory)
            build = source / 'build'
            declarations = []
            for target in sorted({entry['target'] for entry in MANIFEST['tests']}):
                if target == omit_target:
                    continue
                # No compilation is necessary to test target admission and CTest.
                # Native wrapper/provenance behaviour has separate gate tests.
                declarations += [f'add_custom_target({target})',
                    f'set_property(TARGET {target} PROPERTY JUDAS_HAS_VALIDATION_ENTRY TRUE)']
            implementation = source / 'registration.cmake'
            manifest_path=source/'registration-manifest.json'
            manifest_path.write_text(json.dumps(MANIFEST),encoding='utf-8')
            implementation_text=MODULE.read_text(encoding="utf-8") if module is None else module
            implementation_text=implementation_text.replace('${CMAKE_SOURCE_DIR}/tools/validation/suites.json',manifest_path.as_posix())
            implementation.write_text(implementation_text,encoding="utf-8")
            (source / 'CMakeLists.txt').write_text('\n'.join([
                'cmake_minimum_required(VERSION 3.20)',
                'project(JudasGateContract NONE)',
                f'set(CMAKE_SOURCE_DIR "{ROOT.as_posix()}")',
                'add_executable(judas IMPORTED)',
                'set_property(TARGET judas PROPERTY IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/judas")',
                'set(JUDAS_ENABLE_VALIDATION ON CACHE BOOL "" FORCE)',
                f'set(JUDAS_ID_channel "{channel}")',
                'set(JUDAS_INCLUDE_KNOWN_FAILURES ' + ('ON' if include_known else 'OFF') +
                    ' CACHE BOOL "" FORCE)',
                *declarations,
                f'include("{implementation.as_posix()}")',
                '']), encoding="utf-8")
            configure_args = [shutil.which('cmake') or 'cmake', '-S', str(source), '-B', str(build)]
            if generator:
                configure_args += ['-G', generator]
            configured = subprocess.run(configure_args, text=True, encoding="utf-8", capture_output=True,
                                        timeout=30)
            if omit_target:
                self.assertNotEqual(configured.returncode, 0)
                return configured.stdout + configured.stderr
            self.assertEqual(configured.returncode, 0, configured.stdout + configured.stderr)
            listed = subprocess.run([shutil.which('ctest') or 'ctest', '--test-dir', str(build),
                                     '-C', 'Release', '--show-only=json-v1'], text=True, encoding="utf-8", capture_output=True,
                                    timeout=15)
            self.assertEqual(listed.returncode, 0, listed.stdout + listed.stderr)
            return {entry['name']: entry for entry in json.loads(listed.stdout)['tests']}

    def assert_contract(self, tests, include_known=False, channel='Experimental'):
        entries = [entry for entry in MANIFEST['tests']
                   if include_known or not entry.get('manual', False)]
        tooling = {'prealpha.gate-negative-controls', 'prealpha.build-organization',
                   'prealpha.gate-registration', 'prealpha.browser-export-boundaries',
                   'prealpha.editor-v2-architecture', 'prealpha.scripting-catalogue'}
        self.assertEqual(set(tests), {'prealpha.' + entry['name'] for entry in entries} | tooling)
        browser_command=tests['prealpha.browser-export-boundaries']['command']
        browser_pattern=browser_command[browser_command.index('-p')+1]
        for required in ['test_browser_export.py','test_browser_workflow.py']:
            self.assertTrue(fnmatch.fnmatchcase(required,browser_pattern),required+' is absent from browser gate discovery')
        for entry in entries:
            case = tests['prealpha.' + entry['name']]
            self.assertEqual(Path(case['command'][1]).resolve(), (ROOT / 'scripts/prealpha_validate.py').resolve())
            self.assertEqual(case['command'][2:],
                             ['--build-dir', case['command'][3], '--test', entry['name'],
                              '--expected-channel', channel])
            properties = {item['name']: item['value'] for item in case['properties']}
            unavailable = channel == 'Alpha' and entry['target'] in {
                'judas_game_water_runtime_tests', 'judas_game_water_script_tests',
                'judas_water_workshop_tests',
                'judas_water_visual_tests',
}
            labels = ['prealpha', *entry['groups']]
            if unavailable:
                labels.append('unavailable-in-channel')
                self.assertIn('SKIP_RETURN_CODE', properties)
                self.assertEqual(properties['SKIP_RETURN_CODE'], 77)
            else:
                self.assertNotIn('SKIP_RETURN_CODE', properties)
            self.assertEqual(properties['LABELS'], sorted(labels))
            self.assertTrue(properties['RUN_SERIAL'])
            self.assertEqual(properties['TIMEOUT'], 150.0)
        for name in tooling:
            command = tests[name]['command']
            self.assertIn('unittest', command)

    def test_ordinary_registration_omits_only_explicit_manual_failures(self):
        self.assert_contract(self.configure())

    def test_alpha_marks_only_advanced_runtime_unavailable(self):
        self.assert_contract(self.configure(channel='Alpha'), channel='Alpha')

    def test_alpha_multi_configuration_discovery(self):
        if not shutil.which('ninja'):
            self.skipTest('Ninja needed for explicit multi-config control')
        self.assert_contract(self.configure(channel='Alpha', generator='Ninja Multi-Config'),
                             channel='Alpha')

    def test_removed_modular_editor_or_scripting_gate_is_detected(self):
        original = MODULE.read_text(encoding='utf-8')
        for name in ('editor-v2-architecture', 'scripting-catalogue'):
            with self.subTest(gate=name):
                changed = original.replace('prealpha.' + name, 'prealpha.missing-' + name)
                self.assertNotEqual(original, changed)
                with self.assertRaises(AssertionError):
                    self.assert_contract(self.configure(module=changed))

    def test_removed_browser_workflow_discovery_is_detected(self):
        original=MODULE.read_text(encoding='utf-8')
        changed=original.replace('test_browser*.py','test_browser_export.py',1)
        self.assertNotEqual(original,changed)
        with self.assertRaises(AssertionError):
            self.assert_contract(self.configure(module=changed))

    def test_removed_channel_guard_is_detected(self):
        module = MODULE.read_text(encoding='utf-8').replace(
            'if(name IN_LIST judas_channel_unavailable)', 'if(FALSE)', 1)
        self.assertNotEqual(module, MODULE.read_text(encoding='utf-8'))
        with self.assertRaises(AssertionError):
            self.assert_contract(self.configure(channel='Alpha', module=module), channel='Alpha')

    def test_removed_advanced_runtime_channel_admission_is_detected(self):
        # This real RuntimeWorld fixture needs Advanced Fluids. Alpha must report
        # it unavailable, while Experimental must continue to execute it.
        alpha = self.configure(channel='Alpha')
        experimental = self.configure()
        for name in ('game-water-runtime', 'water-workshop-controls'):
            with self.subTest(gate=name):
                case = alpha['prealpha.' + name]
                original = list(case['properties'])
                case['properties'] = [item for item in original
                                      if item['name'] != 'SKIP_RETURN_CODE']
                with self.assertRaises(AssertionError):
                    self.assert_contract(alpha, channel='Alpha')
                case['properties'] = original
                case = experimental['prealpha.' + name]
                original = list(case['properties'])
                case['properties'].append({'name': 'SKIP_RETURN_CODE', 'value': 77})
                with self.assertRaises(AssertionError):
                    self.assert_contract(experimental)
                case['properties'] = original

    @unittest.skipUnless(shutil.which('ninja'), 'Ninja needed for explicit multi-config control')
    def test_multi_configuration_discovery(self):
        self.assert_contract(self.configure(generator='Ninja Multi-Config'))

    def test_explicit_failure_registration_preserves_original_case(self):
        self.assert_contract(self.configure(include_known=True), include_known=True)

    def test_absent_manifest_target_fails_configuration(self):
        self.assertIn('Validation manifest names absent target: judas_build_identity_tests',
                      self.configure(omit_target='judas_build_identity_tests'))

    def test_removed_manual_guard_is_detected(self):
        module = MODULE.read_text(encoding="utf-8").replace(
            'if(NOT manual_error AND manual AND NOT JUDAS_INCLUDE_KNOWN_FAILURES)',
            'if(FALSE)', 1)
        self.assertNotEqual(module, MODULE.read_text(encoding="utf-8"))
        # Production has no intentionally failing historical cases. Exercise the
        # generic manual-case admission rule with a scoped synthetic declaration.
        first=MANIFEST['tests'][0]
        previous=first.get('manual')
        first['manual']=True
        try:
            with self.assertRaises(AssertionError):
                self.assert_contract(self.configure(module=module))
        finally:
            if previous is None:first.pop('manual')
            else:first['manual']=previous


if __name__ == '__main__':
    unittest.main()
