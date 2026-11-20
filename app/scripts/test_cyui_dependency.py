"""Inert resolver regression checks; never launches an application or native test."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import cyui_dependency


class CyUISourceTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / 'cyui'
        self.source.mkdir()
        self.revision = json.loads((cyui_dependency.APP / 'cyui-source.lock.json').read_text('utf-8'))['revision']

    @patch('cyui_dependency.subprocess.check_output')
    def test_matching_clean_published_revision_is_read_only(self, output):
        output.side_effect = [self.revision, '']
        self.assertEqual(cyui_dependency.resolve(self.root / 'unused', self.source), self.source.resolve())
        self.assertEqual([call.args[0][-2:] for call in output.call_args_list],
                         [['rev-parse', 'HEAD'], ['--porcelain', '--untracked-files=all']])

    @patch('cyui_dependency.subprocess.check_output')
    def test_dirty_checkout_is_never_reset_or_swapped(self, output):
        output.side_effect = [self.revision, ' M src/modules/code/CodeSyntax.cpp']
        with self.assertRaisesRegex(RuntimeError, 'local changes'):
            cyui_dependency.resolve(self.root / 'unused', self.source)
        self.assertEqual(output.call_count, 2)
        self.assertTrue(self.source.is_dir())
        self.assertFalse((self.root / 'unused').exists())

    @patch('cyui_dependency.subprocess.check_output')
    def test_different_revision_is_never_checked_out_over(self, output):
        output.return_value = '0' * 40
        with self.assertRaisesRegex(RuntimeError, 'differs'):
            cyui_dependency.resolve(self.root / 'unused', self.source)
        self.assertEqual(output.call_count, 1)

    @patch('cyui_dependency.subprocess.check_output')
    def test_abbreviated_lock_is_rejected_before_git(self, output):
        (self.root / 'cyui-source.lock.json').write_text(json.dumps({'revision': self.revision[:7]}), encoding='utf-8')
        with patch.object(cyui_dependency, 'APP', self.root):
            with self.assertRaisesRegex(RuntimeError, 'full commit'):
                cyui_dependency.resolve(self.root / 'unused', self.source)
        output.assert_not_called()


if __name__ == '__main__':
    unittest.main()
