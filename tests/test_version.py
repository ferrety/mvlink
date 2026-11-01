"""Check the real executable's version without any display or QPA plugin."""

import os
from pathlib import Path
import subprocess
import sys
import unittest


class VersionTests(unittest.TestCase):
    def test_headless_version(self):
        executable = os.environ.get('MVLINK_TEST_EXECUTABLE')
        if not executable:
            self.skipTest('pass an executable as the script argument')
        version = (Path(__file__).resolve().parents[1] / 'version.txt').read_text().strip()
        environment = dict(os.environ, QT_QPA_PLATFORM='nonexistent-platform')
        environment.pop('DISPLAY', None)
        environment.pop('WAYLAND_DISPLAY', None)
        for argument in ('--version', '-v'):
            with self.subTest(argument=argument):
                result = subprocess.run(
                    [str(Path(executable).resolve()), argument], env=environment,
                    capture_output=True, text=True, timeout=15, check=True,
                )
                self.assertEqual(result.stdout, f'mvlink {version}\n')
                self.assertEqual(result.stderr, '')


if __name__ == '__main__':
    if len(sys.argv) > 1:
        os.environ['MVLINK_TEST_EXECUTABLE'] = sys.argv.pop(1)
    unittest.main()
