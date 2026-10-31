"""Stage only the application's imported DLLs and the runtime-loaded QPA plugin."""

import re
import shutil
import subprocess
import sys
from pathlib import Path


QT = Path('/opt/qt-windows')
COMPILER = 'x86_64-w64-mingw32-g++-posix'
OBJDUMP = 'x86_64-w64-mingw32-objdump'
QT_DLLS = {'qt6core.dll', 'qt6gui.dll', 'qt6widgets.dll'}
RUNTIME_DLLS = {'libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'}
# These are provided by Windows 11. Unknown imports fail packaging rather than
# producing an incomplete bundle. API sets are also supplied by Windows.
SYSTEM_DLLS = set('''
advapi32 authz avrt bcrypt combase comctl32 comdlg32 crypt32 d2d1 d3d9 d3d11 d3d12
dcomp dnsapi dwrite dwmapi dxgi gdi32 imm32 iphlpapi kernel32 mpr msimg32 msvcrt
netapi32 ntdll ole32 oleacc oleaut32 powrprof propsys psapi rpcrt4 runtimeobject secur32
setupapi shcore shell32 shlwapi user32 userenv usp10 uxtheme version
windowscodecs winhttp winmm winspool wintrust ws2_32 wtsapi32 ucrtbase
'''.split())


def imports(binary):
    result = subprocess.check_output([OBJDUMP, '-p', str(binary)], text=True)
    return re.findall(r'DLL Name:\s*(\S+)', result)


def package(mode, executable, output):
    if mode not in {'static', 'shared'}:
        raise ValueError('linkage must be shared or static')
    output.mkdir(parents=True)
    shutil.copy2(executable, output / 'mvlink.exe')
    pending = [output / 'mvlink.exe']
    if mode == 'shared':
        plugin = output / 'platforms' / 'qwindows.dll'
        plugin.parent.mkdir()
        shutil.copy2(QT / 'plugins/platforms/qwindows.dll', plugin)
        pending.append(plugin)
        (output / 'qt.conf').write_text('[Paths]\nPlugins = .\n')
    seen = set()
    report = []
    while pending:
        binary = pending.pop(0)
        report.append(f'[{binary.relative_to(output)}]')
        for name in imports(binary):
            report.append(f'  DLL Name: {name}')
            lower = name.lower()
            if lower.endswith('.dll') and (
                lower[:-4] in SYSTEM_DLLS or lower.startswith(('api-ms-', 'ext-ms-'))
            ):
                continue
            if mode == 'static':
                raise RuntimeError(f'static build imports a non-system DLL: {name}')
            if lower in seen:
                continue
            if lower in QT_DLLS:
                source = QT / 'bin' / name
            elif lower in RUNTIME_DLLS:
                source = Path(subprocess.check_output(
                    [COMPILER, f'-print-file-name={name}'], text=True).strip())
            else:
                raise RuntimeError(f'unexpected DLL dependency: {name}')
            if not source.is_file():
                raise RuntimeError(f'cannot locate required DLL: {name}')
            target = output / name
            shutil.copy2(source, target)
            seen.add(lower)
            pending.append(target)
        report.append('')
    (output / 'mvlink-imports.txt').write_text('\n'.join(report))


if __name__ == '__main__':
    package(sys.argv[1], Path(sys.argv[2]), Path(sys.argv[3]))
