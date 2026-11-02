"""Source candidates are immutable and successor recipes bind actual archives."""

import json
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parent))
import create_stage_source as creator
import build_stage_02_cxx as stage02


class Stage02Manifest(unittest.TestCase):
    def test_stage_advance_declares_the_actual_target_loader(self):
        for target_os, arch, loader in (('linux', 'x86_64', 'ld-linux-x86-64.so.2'),
                                       ('linux', 'aarch64', 'ld-linux-aarch64.so.1'),
                                       ('macos', 'aarch64', None), ('windows', 'aarch64', None)):
            with self.subTest(os=target_os, arch=arch), tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                sdk, asset, output = root / 'sdk', root / 'asset', root / 'output'
                sdk.mkdir()
                (sdk / 'manifest.json').write_text(json.dumps({'stage': '01-c',
                    'target': {'os': target_os, 'arch': arch}, 'external_prerequisites': []}))
                for name in ('tools', 'libstdc++', 'sources'):
                    (asset / name).mkdir(parents=True)
                args = ['build_stage_02_cxx', '--sdk-root', str(sdk), '--asset-root', str(asset),
                        '--work-root', str(root / 'work'), '--output', str(output),
                        '--recipe-location', 'file:///candidate.json', '--source-sha256', 'b' * 64]
                with patch.object(sys, 'argv', args), patch.object(stage02, 'run'), \
                        patch.object(stage02, 'rewrite_stale_macho_rpaths'):
                    stage02.main()
                result = json.loads((output / 'manifest.json').read_text())
                self.assertEqual(result['stage'], '02-cxx')
                self.assertEqual(result['target'], {'os': target_os, 'arch': arch})
                loaders = [p['components'] for p in result['external_prerequisites'] if p['id'] == 'linux-elf-loader']
                self.assertEqual(loaders, [[loader]] if loader else [])


class SourceCandidates(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        (self.root / 'input.c').write_text('int source;\n')
        self.spec = {'input_stage': '02-cxx', 'entrypoint': 'tools/build_fixture.py',
                     'project_paths': ('input.c',), 'source_paths': ()}
        self.git = patch.object(creator, 'git', side_effect=lambda root, *args: 'a' * 40 if args[0] == 'rev-parse' else '')
        self.git.start()
        self.addCleanup(self.git.stop)
        self.stages = patch.dict(creator.STAGES, {'03-gfx-simple': self.spec})
        self.stages.start()
        self.addCleanup(self.stages.stop)

    def generate(self, output, stage='03-gfx-simple', os_name='linux', successor=None):
        args = ['create_stage_source', '--root', str(self.root), '--stage', stage, '--target-os', os_name,
                '--source-root', str(self.root / 'sources'), '--output-dir', str(output), '--release-tag', 'v0.4.0-candidate.1']
        if successor:
            args += ['--successor-recipe', str(successor)]
        with patch.object(sys, 'argv', args):
            creator.main()
        return json.loads((output / (stage + '.json')).read_text())

    def test_same_candidate_bytes_and_recipe_on_each_architecture(self):
        for os_name in ('linux', 'macos', 'windows'):
            with self.subTest(os=os_name):
                first = self.generate(self.root / (os_name + '-x64'), os_name=os_name)
                second = self.generate(self.root / (os_name + '-arm64'), os_name=os_name)
                self.assertEqual(first, second)
                self.assertEqual((self.root / (os_name + '-x64') / first['source']['archive']).read_bytes(),
                                 (self.root / (os_name + '-arm64') / second['source']['archive']).read_bytes())

    def test_existing_candidate_never_overwritten(self):
        output = self.root / 'out'
        first = self.generate(output)
        asset = output / first['source']['archive']
        previous = asset.read_bytes()
        (self.root / 'input.c').write_text('changed source;\n')
        with self.assertRaisesRegex(SystemExit, 'already exists'):
            self.generate(output)
        self.assertEqual(asset.read_bytes(), previous)
        self.assertEqual(json.loads((output / '03-gfx-simple.json').read_text()), first)

    def predecessor(self, successor):
        with patch.dict(creator.STAGES, {'02-cxx': dict(self.spec, input_stage='01-c')}), \
                patch.object(creator, 'canonical_runtime_sources', return_value=({}, {'format': 1, 'files': []})):
            return self.generate(self.root / 'predecessor', stage='02-cxx', successor=successor)

    def test_embedded_successor_matches_actual_archive(self):
        output = self.root / 'successor'
        expected = self.generate(output)
        result = self.predecessor(output / '03-gfx-simple.json')
        with tarfile.open(self.root / 'predecessor' / result['source']['archive']) as archive:
            embedded = json.load(archive.extractfile('stages/recipes/03-gfx-simple.json'))
        self.assertEqual(embedded, expected)

    def test_tampered_successor_archive_rejected(self):
        output = self.root / 'successor'
        recipe = self.generate(output)
        (output / recipe['source']['archive']).write_bytes(b'tampered')
        with self.assertRaisesRegex(SystemExit, 'actual local source archive'):
            self.predecessor(output / '03-gfx-simple.json')
        self.assertFalse((self.root / 'predecessor/02-cxx.json').exists())

    def test_wrong_successor_commit_or_version_rejected(self):
        output = self.root / 'successor'
        recipe = self.generate(output)
        path = output / '03-gfx-simple.json'
        for key, value in (('source_commit', 'b' * 40), ('archive', 'other.tar.xz')):
            altered = json.loads(json.dumps(recipe))
            altered['source'][key] = value
            path.write_text(json.dumps(altered))
            with self.assertRaisesRegex(SystemExit, 'candidate version/OS/stage/source commit'):
                self.predecessor(path)


if __name__ == '__main__':
    unittest.main()
