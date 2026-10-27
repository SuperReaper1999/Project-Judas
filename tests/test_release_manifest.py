import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import release_manifest as release


def fixture(version='1.0.1-alpha.1', channel='Alpha'):
    name = 'Judas-' + version + '-Linux-x86_64.tar.gz'
    return {'schema': 1, 'channel': channel, 'version': version, 'tag': 'v' + version,
            'source_revision': 'a' * 40, 'compatibility': {'update_protocol': 1, 'engine_major': 1},
            'notes': 'Test fixture only. Not a published release.',
            'notes_url': release.BASE + 'tag/v' + version,
            'artifacts': [{'name': name, 'platform': 'Linux', 'architecture': 'x86_64',
                           'bytes': 4, 'sha256': hashlib.sha256(b'test').hexdigest(),
                           'url': release.BASE + 'download/v' + version + '/' + name}]}


def cases():
    good = fixture()
    rows = [{'name': 'newer same-channel compatible', 'manifest': good, 'valid': True, 'offer': True},
            {'name': 'same version', 'manifest': fixture('1.0.0-alpha.1'), 'valid': True, 'offer': False},
            {'name': 'older version', 'manifest': fixture('0.9.0-alpha.1'), 'valid': False, 'offer': False},
            {'name': 'no target platform', 'manifest': good, 'platform': 'Windows', 'valid': True, 'offer': False},
            {'name': 'explicit Experimental opt in', 'manifest': fixture(channel='Experimental'), 'channel': 'Experimental', 'valid': True, 'offer': True}]
    different = fixture('2.0.0-alpha.1'); different['compatibility']['engine_major'] = 2
    rows.append({'name': 'different engine major is not an offered update', 'manifest': different, 'valid': True, 'offer': False})
    mutations = [
        ('wrong channel', lambda j: j.update(channel='Experimental')),
        ('wrong schema', lambda j: j.update(schema=2)),
        ('boolean schema', lambda j: j.update(schema=True)),
        ('invalid version', lambda j: j.update(version='01.0.1-alpha.1')),
        ('wrong tag', lambda j: j.update(tag='main')),
        ('missing provenance', lambda j: j.pop('source_revision')),
        ('invalid commit', lambda j: j.update(source_revision='dirty')),
        ('unsupported protocol', lambda j: j['compatibility'].update(update_protocol=2)),
        ('major mismatch', lambda j: j['compatibility'].update(engine_major=2)),
        ('untrusted notes', lambda j: j.update(notes_url='https://evil.invalid/')),
        ('empty notes', lambda j: j.update(notes='')),
        ('unbounded notes', lambda j: j.update(notes='x' * 8193)),
        ('no artifacts', lambda j: j.update(artifacts=[])),
        ('too many artifacts', lambda j: j.update(artifacts=j['artifacts'] * 33)),
        ('duplicate artifact', lambda j: j['artifacts'].append(copy.deepcopy(j['artifacts'][0]))),
        ('untrusted download', lambda j: j['artifacts'][0].update(url='file:///tmp/bad')),
        ('traversal filename', lambda j: j['artifacts'][0].update(name='../bad')),
        ('bad checksum', lambda j: j['artifacts'][0].update(sha256='0')),
        ('zero size', lambda j: j['artifacts'][0].update(bytes=0)),
        ('negative size', lambda j: j['artifacts'][0].update(bytes=-1)),
        ('boolean size', lambda j: j['artifacts'][0].update(bytes=True)),
        ('excessive size', lambda j: j['artifacts'][0].update(bytes=100*1024**3+1)),
        ('unknown platform', lambda j: j['artifacts'][0].update(platform='Android')),
        ('wrong architecture', lambda j: j['artifacts'][0].update(architecture='unknown')),
    ]
    for name, change in mutations:
        altered = copy.deepcopy(good); change(altered)
        rows.append({'name': name, 'manifest': altered, 'valid': False, 'offer': False})
    for version, ch, valid in [('1.0.1','Stable',True),('1.0.1-alpha.1','Stable',False),('1.0.1-beta.1','Beta',True),('1.0.1-alpha.1','Beta',False)]:
        rows.append({'name': ch + ' maturity ' + version, 'manifest': fixture(version,ch), 'channel': ch, 'valid':valid,'offer':valid})
    return rows


class ReleaseTests(unittest.TestCase):
    def test_shared_manifest_controls(self):
        for row in cases():
            with self.subTest(row['name']):
                try:
                    doc = release.validate(row['manifest'], row.get('channel','Alpha'))
                except (ValueError, TypeError, KeyError):
                    self.assertFalse(row['valid']); continue
                self.assertTrue(row['valid'])
                self.assertEqual(release.result(doc,'1.0.0-alpha.1',row.get('platform','Linux'),'x86_64')['offer'],row['offer'])

    def test_semver_order(self):
        order=['1.0.0-alpha','1.0.0-alpha.1','1.0.0-alpha.2','1.0.0-alpha.10','1.0.0-beta.1','1.0.0-rc.1','1.0.0','1.0.1','2.0.0']
        for a,b in zip(order,order[1:]):
            self.assertEqual(release.compare(a,b),-1);self.assertEqual(release.compare(b,a),1)
        self.assertEqual(release.compare('1.0.0+local','1.0.0+other'),0)
        for bad in ['1','v1.0.0','01.0.0','1.0.0-alpha.01','1.0.0-','1.0.0+','1.0.0-x..a']:
            with self.assertRaises(ValueError): release.semver(bad)

    def test_integrity_and_negative_controls(self):
        with tempfile.TemporaryDirectory() as tmp:
            doc=fixture(); p=Path(tmp)/doc['artifacts'][0]['name'];p.write_bytes(b'test')
            self.assertEqual(release.verify_download(doc,p),doc['artifacts'][0]['sha256'])
            p.write_bytes(b'evil')
            with self.assertRaises(ValueError):release.verify_download(doc,p)
            p.write_bytes(b'tes')
            with self.assertRaises(ValueError):release.verify_download(doc,p)
            p.unlink()
            with self.assertRaises(ValueError):release.verify_download(doc,p)

    def test_excessive_nesting_is_a_controlled_failure(self):
        with self.assertRaises(ValueError):release.decode('['*2000+'0'+']'*2000)

    def test_offline_is_a_controlled_failure(self):
        with patch.object(release.urllib.request,'urlopen',side_effect=OSError('offline')):
            with patch.object(sys,'argv',['release_manifest.py','check','--installed','1.0.0-alpha.1','--platform','Linux']):
                self.assertEqual(release.main(),1)
        with self.assertRaises(ValueError):release.endpoint('main')

if __name__=='__main__':
    if '--write-fixtures' in sys.argv:
        (ROOT/'tests/fixtures/releases/update-cases.json').write_text(json.dumps(cases(),indent=2)+'\n')
    else:unittest.main()
