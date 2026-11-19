"""Dependency selection contracts; no Git, network or SDK executable is run."""
from pathlib import Path
import json,tempfile,unittest
from unittest.mock import patch
from cymcp_dependency import resolve

class SDKSourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name);self.core=self.root/'core';self.sdk=self.root/'sdk'
        (self.core/'app').mkdir(parents=True);self.sdk.mkdir()
        self.revision='1'*40
        (self.core/'app/cymcp-source.lock.json').write_text(json.dumps({'revision':self.revision}),encoding='utf-8')

    @patch('cymcp_dependency.subprocess.run')
    @patch('cymcp_dependency.subprocess.check_output')
    def test_exact_clean_local_source_needs_no_network(self,output,run):
        output.side_effect=[self.revision+'\n','']
        self.assertEqual(resolve(self.core,self.sdk),self.sdk.resolve());run.assert_not_called()

    @patch('cymcp_dependency.subprocess.run')
    @patch('cymcp_dependency.subprocess.check_output')
    def test_dirty_local_source_is_preserved_without_fallback(self,output,run):
        output.side_effect=[self.revision,' M src/core.cpp']
        with self.assertRaises(ValueError):resolve(self.core,self.sdk)
        run.assert_not_called();self.assertTrue(self.sdk.is_dir())

    @patch('cymcp_dependency.subprocess.run')
    def test_missing_explicit_local_source_is_not_downloaded(self,run):
        with self.assertRaises(FileNotFoundError):resolve(self.core,self.root/'absent')
        run.assert_not_called()

if __name__=='__main__':unittest.main()
