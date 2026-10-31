import argparse
import copy
import hashlib
import json
import io
import os
import shutil
import subprocess
import tarfile
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import package_sdk
import package_source
import prepare_release
import publish_release
import assemble_release_run
sys.path.insert(0,str(ROOT/'scripts'))
import qualify_m100


def sdk(path,platform):
    path.mkdir();(path/'packaging').mkdir()
    version=json.loads((ROOT/'engine-version.json').read_text())
    build=dict(**version,channel='Alpha',build_id='fixture-only',source_revision='a'*40,
        source_tree='b'*40,source_dirty=False,source_fingerprint='c'*64,source_untracked_count=0,
        source_untracked_fingerprint=hashlib.sha256(b'').hexdigest(),source_origin='git',
        platform=platform,architecture='x86_64',configuration='Release',compiler='fixture',compiler_version='fixture')
    (path/'engine-version.json').write_text(json.dumps(version));(path/'BUILD_INFO.json').write_text(json.dumps(build))
    (path/('judas.exe' if platform=='Windows' else 'judas')).write_text('Fixture, not a native executable')
    (path/'packaging/sdk-inventory.json').write_text(json.dumps(dict(schema=1,tools=['judas'],windows_tools=[],files=['engine-version.json'],directories=[],current_markdown='docs')))
    records=package_sdk.file_records(path)
    manifest=dict(schema=1,kind='judas-native-sdk',build=build,tools=['judas.exe' if platform=='Windows' else 'judas'],files=records,preset='release')
    (path/package_sdk.MANIFEST).write_text(json.dumps(manifest))
    (path/package_sdk.CHECKSUMS).write_text(''.join(r['sha256']+'  '+r['path']+'\n' for r in records)+package_sdk.digest(path/package_sdk.MANIFEST)+'  '+package_sdk.MANIFEST+'\n')


class ReleaseNotes(unittest.TestCase):
    def test_patch_uses_its_own_curated_notes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);(root/'docs').mkdir()
            first=root/'docs/ALPHA_V1_0_0_NOTES.md';first.write_text('First release')
            patch_notes=root/'docs/ALPHA_V1_0_1_NOTES.md';patch_notes.write_text('Patch release')
            self.assertEqual(assemble_release_run.release_notes(root,{'version':'1.0.1-alpha.1','channel':'Alpha'}),patch_notes)
            self.assertEqual(assemble_release_run.release_notes(root,{'version':'1.0.0-alpha.1','channel':'Alpha'}),first)
            with self.assertRaises(ValueError):
                assemble_release_run.release_notes(root,{'version':'1.0.2-alpha.1','channel':'Alpha'})


class QualificationScope(unittest.TestCase):
    def test_2d_release_scope_has_its_own_checks_and_source_fixtures(self):
        self.assertIn('2d',qualify_m100.GROUPS)
        spec=package_sdk.inventory(ROOT/'packaging/sdk-inventory.json')
        for name in package_source.LEARNING_PROJECTS:
            self.assertIn('projects/'+name,spec['directories'])
            self.assertTrue(package_source.selected('projects/'+name+'/Scenes/main.judas','engine'))
        self.assertFalse(package_source.selected('projects/lunar_island/private.js','engine'))
        self.assertFalse(package_source.selected('projects/beacon_grove/Saves/private.judasstate','engine'))

    def test_qualification_imports_do_not_defeat_strict_source_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            temporary=Path(temporary);source=temporary/'source';source.mkdir()
            for name in ('scripts/qualify_m100.py','scripts/prealpha_validate.py','tools/package_sdk.py','engine-version.json'):
                target=source/name;target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(ROOT/name,target)
            def git(*args):
                subprocess.run(['git','-C',str(source),*args],check=True,capture_output=True)
            git('init');git('add','.')
            git('-c','user.name=Qualification','-c','user.email=fixture@example.invalid','-c','commit.gpgsign=false','commit','-m','fixture')
            environment=dict(os.environ)
            environment.pop('PYTHONDONTWRITEBYTECODE',None)
            environment.pop('PYTHONPYCACHEPREFIX',None)
            command=[sys.executable,'-c',"import sys;sys.path.insert(0,'scripts');import qualify_m100"]
            subprocess.run(command,cwd=source,env=environment,check=True)
            args=argparse.Namespace(source=source,output=temporary/'source.tar.gz',replace=False,
                profile='engine',channel='Alpha',allow_untracked=False)
            with self.assertRaisesRegex(ValueError,'__pycache__'):
                package_source.archive(args)
            self.assertFalse(args.output.exists())
            for cache in source.rglob('__pycache__'):shutil.rmtree(cache)
            subprocess.run(command,cwd=source,env=dict(environment,PYTHONDONTWRITEBYTECODE='1'),check=True)
            self.assertFalse(list(source.rglob('__pycache__')))
            package_source.archive(args)
            with tarfile.open(args.output) as archive:
                self.assertFalse(any('__pycache__' in m.name for m in archive))
            # Local developer bytecode is ignored too; authored unknown inputs
            # must still fail rather than being silently excluded.
            shutil.copyfile(ROOT/'.gitignore',source/'.gitignore');git('add','.gitignore')
            git('-c','user.name=Qualification','-c','user.email=fixture@example.invalid','-c','commit.gpgsign=false','commit','-m','ignore generated bytecode')
            subprocess.run(command,cwd=source,env=environment,check=True)
            self.assertTrue(list(source.rglob('__pycache__')))
            args.output=temporary/'with-cache.tar.gz';package_source.archive(args)
            (source/'authored-input.txt').write_text('intended content, not a generated cache')
            args.output=temporary/'unknown-input.tar.gz'
            with self.assertRaisesRegex(ValueError,'authored-input.txt'):
                package_source.archive(args)

    def test_relative_build_path_survives_isolated_regression_directory(self):
        # The extra native checks deliberately run outside the source checkout.
        # A relative executable must be resolved before changing child cwd.
        with tempfile.TemporaryDirectory(dir=ROOT) as temporary:
            temporary=Path(temporary)
            build=temporary/'build'
            relative=Path(os.path.relpath(build,Path.cwd()))
            output=temporary/'qualification'
            with patch.object(sys,'argv',['qualify_m100.py','--build',str(relative),'--output',str(output)]), \
                 patch.object(qualify_m100,'GROUPS',[]), \
                 patch.object(qualify_m100,'EXTRA',['judas_joint_tests']), \
                 patch.object(qualify_m100.subprocess,'check_output',return_value='{"channel":"Alpha"}'), \
                 patch.object(qualify_m100.subprocess,'run',return_value=argparse.Namespace(returncode=0)) as launch:
                qualify_m100.main()
            executable='judas_joint_tests.exe' if os.name=='nt' else 'judas_joint_tests'
            checks=[c for c in launch.call_args_list if Path(c.args[0][0]).name==executable]
            self.assertEqual(len(checks),1)
            self.assertEqual(Path(checks[0].args[0][0]),build.resolve()/executable)
            self.assertEqual(checks[0].kwargs['cwd'],output.resolve())

    def test_alpha_preserves_but_does_not_certify_unavailable_runtime(self):
        tests=[{'name':'rotation','target':'judas_liquid_rotation_tests'},
               {'name':'surface','target':'judas_liquid_surface_tests'},
               {'name':'origin','target':'judas_origin_environment_tests'},
               {'name':'water-policy','target':'judas_project_water_tests'}]
        original=copy.deepcopy(tests)
        selected,unavailable=qualify_m100.partition_channel_tests(tests,'Alpha')
        self.assertEqual([t['name'] for t in selected],['rotation','water-policy'])
        self.assertEqual([t['name'] for t in unavailable],['surface','origin'])
        self.assertTrue(all(t['status']=='not-exercised' for t in unavailable))
        self.assertEqual(tests,original)
        selected,unavailable=qualify_m100.partition_channel_tests(tests,'Experimental')
        self.assertEqual(selected,original)
        self.assertEqual(unavailable,[])


class Packaging(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
        sdk(self.root/'linux','Linux');sdk(self.root/'windows','Windows')
        source=self.root/'source.tar.gz'
        payload=b'fixture source file'
        inventory=json.dumps({'schema':1,'files':[{'path':'engine-version.json','sha256':hashlib.sha256(payload).hexdigest()}]}).encode()
        info=json.dumps({'source_dirty':False,'revision':'a'*40,'tree':'b'*40,'source_fingerprint':'c'*64,'source_files_sha256':hashlib.sha256(inventory).hexdigest()}).encode()
        with tarfile.open(source,'w:gz') as tar:
            for name,data in [('engine-version.json',payload),('JUDAS_SOURCE_FILES.json',inventory),('JUDAS_SOURCE_INFO.json',info)]:
                entry=tarfile.TarInfo(name);entry.size=len(data);tar.addfile(entry,io.BytesIO(data))
        source.with_name(source.name+'.build.json').write_text(json.dumps({'source':{'revision':'a'*40},'sha256':prepare_release.sha(source)}))
        notes=self.root/'notes.md';notes.write_text('Fixture release only, not published.')
        qualification=self.root/'qualification.json';qualification.write_text(json.dumps({'source_revision':'a'*40,'passed':True,'scope':'synthetic packaging fixture, not an engine qualification'}))
        self.args=argparse.Namespace(qualification=qualification,linux_sdk=self.root/'linux',windows_sdk=self.root/'windows',source_archive=source,
            notes=notes,output=self.root/'candidate',channel='Alpha',epoch=1700000000)

    def test_reproducible_candidate_and_default_dry_run(self):
        first=prepare_release.prepare(self.args)
        self.assertEqual(publish_release.validate_candidate(self.args.output,'a'*40),first)
        original={p.name:p.read_bytes() for p in self.args.output.iterdir()}
        self.args.output=self.root/'repeat';prepare_release.prepare(self.args)
        self.assertEqual(original,{p.name:p.read_bytes() for p in self.args.output.iterdir()})
        with patch.object(publish_release,'gh',side_effect=AssertionError('No GitHub calls in dry-run')):
            with patch.object(sys,'argv',['publish_release.py',str(self.args.output),'--accepted-commit','a'*40]):publish_release.main()

    def test_wrong_commit_and_corruption_never_publish(self):
        prepare_release.prepare(self.args)
        with self.assertRaises(ValueError):publish_release.validate_candidate(self.args.output,'b'*40)
        archive=next(self.args.output.glob('*Linux*.tar.gz'));archive.write_bytes(b'corrupt')
        with self.assertRaises(ValueError):publish_release.validate_candidate(self.args.output,'a'*40)

    def test_missing_windows_and_mismatched_provenance(self):
        (self.root/'windows/judas.exe').unlink()
        with self.assertRaises(ValueError):prepare_release.prepare(self.args)
        self.assertFalse(self.args.output.exists())

    def test_invalid_source_archive_is_rejected(self):
        self.args.source_archive.write_bytes(b'not an archive')
        self.args.source_archive.with_name(self.args.source_archive.name+'.build.json').write_text(json.dumps({'source':{'revision':'a'*40},'sha256':prepare_release.sha(self.args.source_archive)}))
        with self.assertRaises(tarfile.TarError):prepare_release.prepare(self.args)

    def test_unapproved_cli_cannot_reach_github(self):
        prepare_release.prepare(self.args)
        with patch.object(publish_release,'gh',side_effect=AssertionError('Unapproved publication must not contact GitHub')):
            with patch.object(sys,'argv',['publish_release.py',str(self.args.output),'--accepted-commit','a'*40,'--publish']):
                with self.assertRaises(SystemExit):publish_release.main()

    def test_remote_main_and_required_reviewers(self):
        manifest=prepare_release.prepare(self.args)
        with patch.object(publish_release,'gh',return_value=json.dumps({'commit':{'sha':'b'*40}})):
            with self.assertRaises(ValueError):publish_release.check_remote(manifest)
        with patch.object(publish_release,'gh',side_effect=[json.dumps({'commit':{'sha':'a'*40}}),json.dumps({'protection_rules':[]})]):
            with self.assertRaises(ValueError):publish_release.check_remote(manifest)

if __name__=='__main__':unittest.main()
