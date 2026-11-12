"""Inert checks of contextual-help aliases; no app or native fixture is run."""
from pathlib import Path
import tempfile
import unittest
from verify_ui_help import verify


class PanelHelpTests(unittest.TestCase):
    def setUp(self):
        self.temporary=tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)
        self.ide=self.root/'ide';self.core=self.root/'core'
        (self.ide/'app/src').mkdir(parents=True)
        (self.core/'app/src').mkdir(parents=True)
        (self.core/'app/src/HelpTopics.h').write_text('{"vcs", "version-control", "providers"}',encoding='utf-8')

    def test_explicit_alias_uses_the_registered_native_destination(self):
        (self.ide/'app/src/View.cpp').write_text('beginPanel("Changes", "vcs");\nbeginPanel("Commits", "gitCommits", "vcs");',encoding='utf-8')
        self.assertEqual(verify(self.ide,self.core),2)

    def test_missing_panel_and_alias_are_rejected(self):
        for call in ['beginPanel("Commits", "gitCommits");','beginPanel("Commits", "gitCommits", "missing");']:
            with self.subTest(call=call):
                (self.ide/'app/src/View.cpp').write_text(call,encoding='utf-8')
                with self.assertRaises(AssertionError):verify(self.ide,self.core)


if __name__=='__main__':unittest.main()
