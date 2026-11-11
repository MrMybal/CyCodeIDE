"""Inert distribution-boundary tests; no Git, network or packaged EXE is run."""
from pathlib import Path
import json
import tempfile
import unittest
from unittest.mock import patch
import package_windows as package


class UpdateBuildTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)/'ide'
        self.build = self.root/'runtime/build'
        self.core = Path(self.temporary.name)/'core'
        (self.root/'app').mkdir(parents=True)
        self.build.mkdir(parents=True)
        self.core.mkdir()
        (self.root/'app/CMakeLists.txt').write_text('project(CyCodeIDE VERSION 0.2.3)',encoding='utf-8')
        (self.root/'CMakeLists.txt').write_text('project(CyCodeIDE VERSION 0.2.3 LANGUAGES C CXX)', encoding='utf-8')
        (self.root/'app/core-source.lock.json').write_text(json.dumps({'revision': 'a'*40}), encoding='utf-8')
        self.info = {'product':'CyCodeIDE', 'repository':'MrMybal/CyCodeIDE', 'version':'0.2.3',
                     'configuration':'Release', 'platform':'windows-x64', 'updates':'ON',
                     'coreRoot':str(self.core), 'executable':str(self.build/'app/Release/CyCodeIDE.exe'),
                     'helper':str(self.build/'app/Release/CyCodeIDEUpdater.exe')}
        self.addCleanup(patch.stopall)
        patch.object(package, 'ROOT', self.root).start()
        self.git = patch.object(package, 'git', side_effect=lambda root,*args: 'a'*40 if args[0]=='rev-parse' else '').start()

    def write(self):
        (self.build/'ide-distribution-Release.json').write_text(json.dumps(self.info), encoding='utf-8')
        binaries=['CyCodeIDE.exe','CyCodeIDEUpdater.exe','cycode_cli.exe']
        for name in binaries:
            path=self.build/'app/Release'/name;path.parent.mkdir(parents=True,exist_ok=True)
            path.write_bytes(b'inert executable fixture; never launched')
        receipt={'metadata':self.info,'sourceDigest':package.distribution_source_digest(self.root),
                 'binaries':{name:package.digest((self.build/'app/Release'/name).read_bytes()) for name in binaries}}
        (self.build/'ide-distribution-Release.receipt.json').write_text(json.dumps(receipt),encoding='utf-8')


    def test_refuses_foreign_or_disabled_build_before_using_its_core(self):
        cases=[('product','CyCode'),('repository','MrMybal/CyCodeReader'),('updates','OFF'),
               ('configuration','Debug'),('version','0.1.7'),('platform','windows-arm64'),
               ('helper',str(self.build/'app/Release/CyCodeUpdater.exe'))]
        for key,value in cases:
            with self.subTest(key=key):
                previous=self.info[key];self.info[key]=value;self.write()
                with self.assertRaises(ValueError): package.configuration(self.build)
                self.git.assert_not_called()
                self.info[key]=previous

    def test_own_version_is_independent_and_core_must_match_lock(self):
        self.write()
        version,core,stage=package.configuration(self.build)
        self.assertEqual(version,'0.2.3')
        self.assertEqual(core,self.core.resolve())
        self.assertEqual(stage,self.build/'app/Release')
        self.git.side_effect=lambda root,*args: 'b'*40 if args[0]=='rev-parse' else ''
        with self.assertRaises(ValueError): package.configuration(self.build)
        self.git.side_effect=lambda root,*args: 'a'*40 if args[0]=='rev-parse' else ' M app/src/UpdateProduct.cpp'
        with self.assertRaises(ValueError): package.configuration(self.build)


    def test_refuses_stale_binary_or_compiler_inputs(self):
        self.write()
        (self.build/'app/Release/CyCodeIDEUpdater.exe').write_bytes(b'changed helper')
        with self.assertRaises(ValueError): package.configuration(self.build)
        self.write()
        (self.root/'app/CMakeLists.txt').write_text('changed build identity',encoding='utf-8')
        with self.assertRaises(ValueError): package.configuration(self.build)


if __name__=='__main__': unittest.main()
