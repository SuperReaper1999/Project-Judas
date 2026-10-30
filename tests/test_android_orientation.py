"""Exercise the actual APK manifest generator without a device or native build."""
import contextlib
import io
from pathlib import Path
import sys
import os
import shutil
import subprocess
import tempfile
import unittest
import xml.etree.ElementTree as ET
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from build_android import parse_args, android_manifest
ANDROID = '{http://schemas.android.com/apk/res/android}'

class AndroidOrientationTests(unittest.TestCase):
    def args(self, *extra):
        return parse_args(['hinge.judasproj', '--output', '/tmp/hinge-orientation.apk', *extra])[0]

    def test_default_keeps_rotation(self):
        a = self.args()
        self.assertEqual(a.orientation, 'any')
        root = ET.fromstring(android_manifest(a, '1.0.3-alpha.1'))
        self.assertNotIn(ANDROID + 'screenOrientation', root.find('application/activity').attrib)

    def test_each_policy_reaches_real_manifest(self):
        for policy in ('landscape', 'portrait', 'sensorLandscape', 'sensorPortrait'):
            with self.subTest(policy=policy):
                root = ET.fromstring(android_manifest(self.args('--orientation', policy), '1.0.3-alpha.1'))
                activity = root.find('application/activity')
                self.assertEqual(activity.get(ANDROID + 'screenOrientation'), policy)
                self.assertIn('orientation', activity.get(ANDROID + 'configChanges').split('|'))
                self.assertEqual(activity.get(ANDROID + 'resizeableActivity'), 'true')
                self.assertEqual(root.find('uses-sdk').get(ANDROID + 'targetSdkVersion'), '36')

    def test_invalid_policy_rejects(self):
        with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
            self.args('--orientation', 'sideways')
        a = self.args(); a.orientation = 'sideways'
        with self.assertRaises(ValueError):
            android_manifest(a, '1.0.3-alpha.1')

    def test_label_escaping_survives_generation(self):
        a = self.args('--name', 'Hinge & "friends"', '--orientation', 'sensorLandscape')
        root = ET.fromstring(android_manifest(a, '1.0.3-alpha.1'))
        self.assertEqual(root.find('application').get(ANDROID + 'label'), a.name)

    def test_actual_sdl_callback_preserves_manifest_policy(self):
        # Run the production callback body with controlled Activity/SDL boundary
        # doubles. This detects SDL replacing an explicit manifest orientation;
        # it is not an Android Activity lifecycle/device test.
        javac = (str(Path(os.environ['JAVA_HOME']) / 'bin/javac')
                 if os.environ.get('JAVA_HOME') else shutil.which('javac'))
        java = str(Path(javac).parent / 'java') if javac else None
        if not javac or not Path(javac).is_file(): self.skipTest('JDK unavailable')
        source = (Path(__file__).resolve().parents[1] /
                  'platform/android/java/org/projectjudas/runtime/JudasActivity.java').read_text()
        start = source.index('    @Override public void setOrientationBis(')
        opening = source.index('{', start); depth = 1; end = opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}'); end += 1
        callback = source[start:end]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory); pm = root / 'android/content/pm'; pm.mkdir(parents=True)
            (pm / 'ActivityInfo.java').write_text('package android.content.pm; public class ActivityInfo { public static final int SCREEN_ORIENTATION_UNSPECIFIED=-1; public int screenOrientation=-1; }')
            (pm / 'PackageManager.java').write_text('package android.content.pm; public class PackageManager { public static class NameNotFoundException extends Exception {} public boolean missing; public ActivityInfo info=new ActivityInfo(); public ActivityInfo getActivityInfo(Object component,int flags) throws NameNotFoundException { if(missing)throw new NameNotFoundException(); return info; } }')
            (root / 'OrientationProbe.java').write_text('class SDLActivity {\n    android.content.pm.PackageManager manager=new android.content.pm.PackageManager();\n    int delegated=0,requested=-99,requests=0;\n    public android.content.pm.PackageManager getPackageManager(){return manager;}\n    public Object getComponentName(){return this;}\n    public void setOrientationBis(int w,int h,boolean r,String hint){++delegated;requested=13;}\n    public void setRequestedOrientation(int policy){requested=policy;++requests;}\n}\npublic class OrientationProbe extends SDLActivity {\n' + callback + '\n    static void check(boolean b){if(!b)throw new AssertionError();}\n    public static void main(String[] args){\n        OrientationProbe p=new OrientationProbe();\n        p.setOrientationBis(1024,768,true,"");check(p.delegated==1&&p.requests==0&&p.requested==13);\n        for(int policy:new int[]{0,1,6,7}){\n            p.manager.info.screenOrientation=policy;\n            int calls=p.requests;\n            p.setOrientationBis(1024,768,true,"");\n            p.setOrientationBis(360,800,true,"Portrait LandscapeLeft");\n            check(p.delegated==1&&p.requests==calls+2&&p.requested==policy);\n        }\n        p.manager.missing=true;\n        try{p.setOrientationBis(1,1,true,"");throw new AssertionError();}\n        catch(IllegalStateException expected){check(p.delegated==1);}\n    }\n}\n')
            files = [str(x) for x in root.rglob('*.java')]
            compiled = subprocess.run([javac, '--release', '8', '-d', str(root), *files], capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            ran = subprocess.run([java, '-cp', str(root), 'OrientationProbe'], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stderr)

if __name__ == '__main__': unittest.main()
