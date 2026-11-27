"""Metadata fixture tests, not a substitute for actual archive/signature CI."""
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from audit_windows_redistribution import source_identity, notice_path


class TestSourceMapping(unittest.TestCase):
    def test_split_packages_use_source_base(self):
        self.assertEqual(source_identity({"name": ["mingw-w64-ucrt-x86_64-libgcc"],
                         "base": ["mingw-w64-gcc"], "version": ["16.2.0-7"]}),
                         "mingw-w64-gcc-16.2.0-7.src.tar.zst")

    def test_epoch(self):
        self.assertEqual(source_identity({"base": ["mingw-w64-pkgconf"],
                         "version": ["1:3.0.11-2"]}),
                         "mingw-w64-pkgconf-1~3.0.11-2.src.tar.zst")

    def test_missing_or_unsafe_base(self):
        for data in ({"version": ["1"]}, {"base": ["../wrong"], "version": ["1"]},
                     {"base": ["mingw-w64-a"], "version": ["../wrong"]}):
            with self.subTest(data=data), self.assertRaises(ValueError):
                source_identity(data)

    def test_notice_path(self):
        self.assertTrue(notice_path("ucrt64/share/licenses/gcc/COPYING.RUNTIME"))
        self.assertTrue(notice_path("ucrt64/share/doc/python/LICENSE.txt"))
        self.assertFalse(notice_path("ucrt64/include/license_parser.h"))
        self.assertFalse(notice_path("ucrt64/lib/license_helper.dll"))
        self.assertFalse(notice_path("ucrt64/include/COPYRIGHT_parser.hpp"))
        self.assertTrue(notice_path("ucrt64/share/doc/gcc/COPYING.RUNTIME"))
        self.assertTrue(notice_path("ucrt64/share/doc/example/LICENSE-MIT"))
        self.assertTrue(notice_path("ucrt64/share/licenses/example/custom-license"))
        self.assertFalse(notice_path("ucrt64/bin/g++.exe"))


if __name__ == "__main__":
    unittest.main()
