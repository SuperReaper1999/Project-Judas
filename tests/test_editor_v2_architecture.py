"""Small ownership/wiring guards; native extension and workflow tests remain authoritative.

These checks deliberately avoid line counts and a fixed inventory of every tool.
They catch the specific architectural regressions this pass replaces: the frame
implementing asset/scene workflows, or directly selecting individual panels.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
EDITOR = ROOT / "src/editor"


def code(text):
    """Drop comments, preserving quoted strings for include/registration checks."""
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/')
    return token.sub(lambda match: "" if match.group().startswith(("//", "/*")) else match.group(), text)


def frame_ownership_errors(text):
    source = re.sub(r'"(?:\\.|[^"\\])*"', '""', code(text))
    forbidden = {
        "direct panel dispatch": r'\bDraw(?:HierarchyPanel|InspectorPanel|AssetBrowserPanel|ConsolePanel)\s*\(',
        "world construction": r'\bm_world\s*->\s*Build\s*\(',
        "standalone export implementation": r'\bExportProject\s*\(',
        "scene serialization implementation": r'\b(?:SaveSceneToFile|LoadSceneFromFile)\s*\(',
    }
    return [name for name, pattern in forbidden.items() if re.search(pattern, source)]


class EditorV2ArchitectureTests(unittest.TestCase):
    def test_application_coordinates_instead_of_implementing_panel_or_file_workflows(self):
        self.assertEqual(frame_ownership_errors((EDITOR / "EditorApplication.cpp").read_text(encoding="utf-8")), [])

    def test_real_panel_implementations_have_local_ownership(self):
        for filename, symbol in (("HierarchyPanel.cpp", "DrawHierarchyPanel"),
                                 ("InspectorPanel.cpp", "DrawInspectorPanel"),
                                 ("AssetBrowserPanel.cpp", "DrawAssetBrowserPanel"),
                                 ("EditorConsolePanel.cpp", "DrawConsolePanel")):
            with self.subTest(panel=symbol):
                source = code((EDITOR / filename).read_text(encoding="utf-8"))
                self.assertRegex(source, r'\bvoid\s+' + symbol + r'\s*\(')
                self.assertNotIn('"EditorApplication.h"', source,
                                 "a panel must not depend on private application implementation")
        old = EDITOR / "EditorPanels.cpp"
        self.assertFalse(old.exists(), "retired monolithic compatibility source must stay removed")

    def test_console_extension_has_no_main_loop_special_case(self):
        frame = code((EDITOR / "EditorApplication.cpp").read_text(encoding="utf-8"))
        self.assertNotRegex(frame, r'\b(?:DrawConsolePanel|RegisterConsolePanel)\s*\(')
        self.assertNotIn('"EditorConsolePanel.h"', frame)
        catalogue = code((EDITOR / "EditorCorePanels.cpp").read_text(encoding="utf-8"))
        self.assertRegex(catalogue, r'\bRegisterConsolePanel\s*\(')
        owner = code((EDITOR / "EditorConsolePanel.cpp").read_text(encoding="utf-8"))
        self.assertRegex(owner, r'\bvoid\s+RegisterConsolePanel\s*\(')
        self.assertRegex(owner, r'\b(?:registry|panels)\.Add\s*\(')

    def test_forbidden_dispatch_and_workflow_negative_controls_are_detected(self):
        samples = (("void Frame(){DrawInspectorPanel(doc,state);}", "direct panel dispatch"),
                   ("void Frame(){m_world->Build(scene);}", "world construction"),
                   ("void Frame(){ExportProject(project, options);}", "standalone export implementation"),
                   ("void Frame(){SaveSceneToFile(scene, path, error);}", "scene serialization implementation"))
        for source, expected in samples:
            with self.subTest(regression=expected):
                self.assertIn(expected, frame_ownership_errors(source))

    def test_coordinating_calls_and_comments_do_not_trigger_false_positives(self):
        source = '''void Frame(){session.Advance(); registry.DrawAll(context); project.Save(); const char* old="ExportProject(...)";}
        // Old Frame used DrawInspectorPanel(doc,state) and ExportProject(...).
        /* m_world->Build(scene); */'''
        self.assertEqual(frame_ownership_errors(source), [])


if __name__ == "__main__":
    unittest.main()
