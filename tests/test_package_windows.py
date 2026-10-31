"""Packaging checks runnable on Linux without Qt or a Windows toolchain."""

import importlib.util
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    'package_windows', Path(__file__).parents[1] / 'docker/package-windows.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)


class WindowsPackagingTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.qt = self.root / 'qt'
        self.exe = self.root / 'mvlink.exe'
        self.exe.touch()
        for name in ('bin/Qt6Core.dll', 'bin/Qt6Gui.dll', 'bin/Qt6Widgets.dll',
                     'plugins/platforms/qwindows.dll'):
            path = self.qt / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.touch()
        self.output = self.root / 'out'
        patcher = patch.object(packager, 'QT', self.qt)
        patcher.start()
        self.addCleanup(patcher.stop)

    def test_shared_walks_plugin_and_transitive_imports_once(self):
        runtime = self.root / 'libwinpthread-1.dll'
        runtime.touch()
        graph = {
            'mvlink.exe': ['Qt6Widgets.dll', 'KERNEL32.dll'],
            'Qt6Widgets.dll': ['Qt6Gui.dll', 'Qt6Core.dll'],
            'Qt6Gui.dll': ['Qt6Core.dll'],
            'Qt6Core.dll': ['libwinpthread-1.dll', 'api-ms-win-core-file-l1-1-0.dll'],
            'qwindows.dll': ['Qt6Gui.dll', 'ole32.dll'],
            'libwinpthread-1.dll': ['msvcrt.dll', 'KERNEL32.dll'],
        }
        with patch.object(packager, 'imports', side_effect=lambda p: graph[p.name]) as scan:
            with patch.object(packager.subprocess, 'check_output', return_value=str(runtime)):
                packager.package('shared', self.exe, self.output)
        self.assertEqual(scan.call_count, 6)
        self.assertEqual({str(p.relative_to(self.output)) for p in self.output.rglob('*')
                          if p.is_file()}, {
            'mvlink.exe', 'Qt6Widgets.dll', 'Qt6Gui.dll', 'Qt6Core.dll',
            'libwinpthread-1.dll', 'platforms/qwindows.dll', 'qt.conf', 'mvlink-imports.txt'})
        self.assertIn('[platforms/qwindows.dll]', (self.output / 'mvlink-imports.txt').read_text())

    def test_static_packages_only_executable_and_report(self):
        with patch.object(packager, 'imports', return_value=['USER32.dll']):
            packager.package('static', self.exe, self.output)
        self.assertEqual({p.name for p in self.output.iterdir()}, {'mvlink.exe', 'mvlink-imports.txt'})

    def test_static_rejects_runtime_dll(self):
        with patch.object(packager, 'imports', return_value=['libgcc_s_seh-1.dll']):
            with self.assertRaisesRegex(RuntimeError, 'non-system DLL'):
                packager.package('static', self.exe, self.output)

    def test_shared_rejects_unexpected_qt_module(self):
        with patch.object(packager, 'imports', return_value=['Qt6Network.dll']):
            with self.assertRaisesRegex(RuntimeError, 'unexpected DLL'):
                packager.package('shared', self.exe, self.output)

    def test_shared_rejects_missing_required_dll(self):
        (self.qt / 'bin/Qt6Core.dll').unlink()
        with patch.object(packager, 'imports', return_value=['Qt6Core.dll']):
            with self.assertRaisesRegex(RuntimeError, 'cannot locate'):
                packager.package('shared', self.exe, self.output)


if __name__ == '__main__':
    unittest.main()
