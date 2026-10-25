"""Exercise the actual CTest gate registration without building Judas.

These configure-only projects declare the real manifest's target names, then
include the production registration module. They check executable invocations,
labels, serial execution and known-failure admission, not source-string counts.
"""

import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODULE = ROOT / 'cmake/JudasValidation.cmake'
MANIFEST = json.loads((ROOT / 'tools/validation/suites.json').read_text(encoding="utf-8"))


class ValidationRegistrationTests(unittest.TestCase):
    def configure(self, include_known=False, omit_target=None, module=None, generator=None):
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
            implementation.write_text(MODULE.read_text(encoding="utf-8") if module is None else module, encoding="utf-8")
            (source / 'CMakeLists.txt').write_text('\n'.join([
                'cmake_minimum_required(VERSION 3.20)',
                'project(JudasGateContract NONE)',
                f'set(CMAKE_SOURCE_DIR "{ROOT.as_posix()}")',
                'add_executable(judas IMPORTED)',
                'set_property(TARGET judas PROPERTY IMPORTED_LOCATION "${CMAKE_BINARY_DIR}/judas")',
                'set(JUDAS_ENABLE_VALIDATION ON CACHE BOOL "" FORCE)',
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

    def assert_contract(self, tests, include_known=False):
        entries = [entry for entry in MANIFEST['tests']
                   if include_known or not entry.get('manual', False)]
        tooling = {'prealpha.gate-negative-controls', 'prealpha.build-organization',
                   'prealpha.gate-registration'}
        self.assertEqual(set(tests), {'prealpha.' + entry['name'] for entry in entries} | tooling)
        for entry in entries:
            case = tests['prealpha.' + entry['name']]
            self.assertEqual(Path(case['command'][1]).resolve(), (ROOT / 'scripts/prealpha_validate.py').resolve())
            self.assertEqual(case['command'][2:],
                             ['--build-dir', case['command'][3], '--test', entry['name']])
            properties = {item['name']: item['value'] for item in case['properties']}
            self.assertEqual(properties['LABELS'], sorted(['prealpha', *entry['groups']]))
            self.assertTrue(properties['RUN_SERIAL'])
            self.assertEqual(properties['TIMEOUT'], 150.0)
        for name in tooling:
            command = tests[name]['command']
            self.assertIn('unittest', command)

    def test_ordinary_registration_omits_only_explicit_manual_failures(self):
        self.assert_contract(self.configure())

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
        with self.assertRaises(AssertionError):
            self.assert_contract(self.configure(module=module))


if __name__ == '__main__':
    unittest.main()
