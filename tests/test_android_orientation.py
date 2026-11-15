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

class AndroidBenchmarkTests(unittest.TestCase):
    def args(self, *extra):
        return parse_args(['game.judasproj', '--output', '/tmp/benchmark-options.apk', *extra])[0]

    def test_defaults_do_not_enable_benchmark(self):
        a = self.args()
        self.assertFalse(a.benchmark)
        self.assertEqual((a.benchmark_key, a.benchmark_seconds, a.benchmark_warmup), ('', 20, 10))
        root = ET.fromstring(android_manifest(a, '2.0.0-alpha.1'))
        metadata = {x.get(ANDROID + 'name'): x.get(ANDROID + 'value')
                    for x in root.findall('application/meta-data')}
        self.assertEqual(metadata['judas.benchmark'], 'false')
        self.assertEqual(metadata['judas.benchmark.seconds'], '20')
        self.assertEqual(metadata['judas.benchmark.warmup'], '10')

    def test_benchmark_options_reach_manifest(self):
        a = self.args('--benchmark', '--benchmark-key', 'key:F4',
                      '--benchmark-seconds', '30', '--benchmark-warmup', '0')
        root = ET.fromstring(android_manifest(a, '2.0.0-alpha.1'))
        self.assertEqual(root.find('application').get(ANDROID + 'debuggable'), 'true')
        metadata = {x.get(ANDROID + 'name'): x.get(ANDROID + 'value')
                    for x in root.findall('application/meta-data')}
        self.assertEqual([metadata[k] for k in ('judas.benchmark', 'judas.benchmark.key',
                         'judas.benchmark.seconds', 'judas.benchmark.warmup')],
                         ['true', 'key:F4', '30', '0'])
        a.benchmark_key = 'key:A&"B'
        root = ET.fromstring(android_manifest(a, '2.0.0-alpha.1'))
        key = next(x for x in root.findall('application/meta-data')
                   if x.get(ANDROID + 'name') == 'judas.benchmark.key')
        self.assertEqual(key.get(ANDROID + 'value'), a.benchmark_key)

    def test_invalid_or_non_development_benchmark_rejected(self):
        for arguments in [('--benchmark-seconds', '1'), ('--benchmark-seconds', '121'),
                          ('--benchmark-warmup', '-1'), ('--benchmark-warmup', '31'),
                          ('--benchmark-key', 'x' * 65), ('--benchmark-key', 'key:F4\n'),
                          ('--benchmark', '--preset', 'release'),
                          ('--benchmark', '--keystore', '/tmp/private-signing.p12')]:
            with self.subTest(arguments=arguments), contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit):
                self.args(*arguments)

    def test_actual_bootstrap_gates_and_validates_benchmark(self):
        # Exercise the production Java method against Android/SDL boundary doubles.
        # Device lifecycle, graphics and physical phone timing are separate checks.
        javac = (str(Path(os.environ['JAVA_HOME']) / 'bin/javac')
                 if os.environ.get('JAVA_HOME') else shutil.which('javac'))
        java = str(Path(javac).parent / 'java') if javac else None
        if not javac or not Path(javac).is_file(): self.skipTest('JDK unavailable')
        source = (Path(__file__).resolve().parents[1] /
                  'platform/android/java/org/projectjudas/runtime/JudasActivity.java').read_text()
        start = source.index('    private void configureBenchmark(')
        opening = source.index('{', start); depth = 1; end = opening + 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}'); end += 1
        callback = source[start:end]
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = {
                'android/os/Bundle.java': """package android.os;
public class Bundle extends java.util.HashMap<String,Object> {
 public boolean getBoolean(String k,boolean d){Object v=get(k);return v==null?d:(Boolean)v;}
 public String getString(String k,String d){Object v=get(k);return v==null?d:(String)v;}
 public int getInt(String k,int d){Object v=get(k);return v==null?d:(Integer)v;}
}""",
                'android/content/Intent.java': """package android.content;
public class Intent extends android.os.Bundle {
 public boolean hasExtra(String k){return containsKey(k);}
 public boolean getBooleanExtra(String k,boolean d){return getBoolean(k,d);}
 public String getStringExtra(String k){return getString(k,null);}
 public int getIntExtra(String k,int d){return getInt(k,d);}
}""",
                'android/content/pm/ApplicationInfo.java': """package android.content.pm;
public class ApplicationInfo {public static final int FLAG_DEBUGGABLE=2;public int flags;}""",
                'BenchmarkProbe.java': """import java.io.*;
class SDLActivity {
 static final java.util.Map<String,String> env=new java.util.HashMap<>();
 static void nativeSetenv(String key,String value){env.put(key,value);}
 final android.content.pm.ApplicationInfo info=new android.content.pm.ApplicationInfo();
 final android.content.Intent intent=new android.content.Intent();
 android.content.pm.ApplicationInfo getApplicationInfo(){return info;}
 android.content.Intent getIntent(){return intent;}
 File getFilesDir(){return new File(System.getProperty("java.io.tmpdir"),"judas-bootstrap-probe");}
}
public class BenchmarkProbe extends SDLActivity {
""" + callback + """
 static void check(boolean b){if(!b)throw new AssertionError(env.toString());}
 static void invalid(BenchmarkProbe p,android.os.Bundle s)throws Exception {
  try{p.configureBenchmark(s);throw new AssertionError("accepted invalid benchmark");}
  catch(IOException expected){check("0".equals(env.get("JUDAS_APP_BENCHMARK")));}
 }
 public static void main(String[] args)throws Exception {
  BenchmarkProbe p=new BenchmarkProbe();android.os.Bundle s=new android.os.Bundle();
  s.put("judas.benchmark",true);p.intent.put("judas.benchmark",true);
  p.configureBenchmark(s);check("0".equals(env.get("JUDAS_APP_BENCHMARK"))&&!env.containsKey("JUDAS_BENCHMARK_DIR"));
  p.info.flags=2;p.intent.clear();p.configureBenchmark(null);check("0".equals(env.get("JUDAS_APP_BENCHMARK")));
  p.configureBenchmark(s);check("1".equals(env.get("JUDAS_APP_BENCHMARK"))&&"20".equals(env.get("JUDAS_BENCHMARK_SECONDS"))&&"10".equals(env.get("JUDAS_BENCHMARK_WARMUP"))&&"".equals(env.get("JUDAS_BENCHMARK_KEY"))&&"1".equals(env.get("JUDAS_PROFILE_DEPTH")));
  String first=env.get("JUDAS_BENCHMARK_DIR");check(first.startsWith(p.getFilesDir().getAbsolutePath()+File.separator+"benchmark-")&&!new File(first).exists());
  p.configureBenchmark(s);check(!first.equals(env.get("JUDAS_BENCHMARK_DIR")));
  s.put("judas.benchmark.key","key:F4");s.put("judas.benchmark.seconds",120);s.put("judas.benchmark.warmup",0);
  p.configureBenchmark(s);check("key:F4".equals(env.get("JUDAS_BENCHMARK_KEY"))&&"120".equals(env.get("JUDAS_BENCHMARK_SECONDS"))&&"0".equals(env.get("JUDAS_BENCHMARK_WARMUP")));
  p.intent.put("judas.benchmark",false);p.configureBenchmark(s);check("0".equals(env.get("JUDAS_APP_BENCHMARK")));
  p.intent.put("judas.benchmark",true);p.intent.put("judas.benchmark.key","key:Return");p.intent.put("judas.benchmark.seconds",2);p.intent.put("judas.benchmark.warmup",30);
  p.configureBenchmark(s);check("key:Return".equals(env.get("JUDAS_BENCHMARK_KEY"))&&"2".equals(env.get("JUDAS_BENCHMARK_SECONDS"))&&"30".equals(env.get("JUDAS_BENCHMARK_WARMUP")));
  p.intent.put("judas.benchmark.seconds",121);invalid(p,s);
  p.intent.put("judas.benchmark.seconds",2);p.intent.put("judas.benchmark.warmup",-1);invalid(p,s);
  p.intent.put("judas.benchmark.warmup",0);p.intent.put("judas.benchmark.key",String.format("%065d",0));invalid(p,s);
  p.intent.put("judas.benchmark.key","key:F4"+(char)10);invalid(p,s);
  p.info.flags=0;p.configureBenchmark(s);check("0".equals(env.get("JUDAS_APP_BENCHMARK")));
 }
}
"""}
            for relative, text in sources.items():
                path = root / relative; path.parent.mkdir(parents=True, exist_ok=True); path.write_text(text)
            files = [str(x) for x in root.rglob('*.java')]
            compiled = subprocess.run([javac, '--release', '8', '-d', str(root), *files], capture_output=True, text=True, timeout=30)
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            ran = subprocess.run([java, '-cp', str(root), 'BenchmarkProbe'], capture_output=True, text=True, timeout=30)
            self.assertEqual(ran.returncode, 0, ran.stderr)

if __name__ == '__main__': unittest.main()
