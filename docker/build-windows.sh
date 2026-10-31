#!/bin/sh
set -eu

# The Qt SDK and application must use the same linkage.
case "${WINDOWS_LINKAGE:-shared}" in
    shared) set -- 'CONFIG+=shared' 'CONFIG-=static' ;;
    static) set -- 'CONFIG+=static' 'CONFIG-=shared' \
        'QMAKE_LFLAGS+=-static -static-libgcc -static-libstdc++' ;;
    *) echo 'WINDOWS_LINKAGE must be shared or static' >&2; exit 1 ;;
esac

# Never reuse Linux Makefiles, moc output, or object files from the source tree.
build_dir=$(mktemp -d)
trap 'rm -rf "$build_dir"' EXIT HUP INT TERM
cd "$build_dir"
# A custom spec selects MinGW before qmake probes the target compiler.
mkdir mkspec
cat > mkspec/qmake.conf <<'SPEC'
include(/opt/qt-windows/mkspecs/win32-g++/qmake.conf)
QMAKE_CC = x86_64-w64-mingw32-gcc-posix
QMAKE_CXX = x86_64-w64-mingw32-g++-posix
QMAKE_LINK = x86_64-w64-mingw32-g++-posix
QMAKE_LINK_C = x86_64-w64-mingw32-gcc-posix
QMAKE_AR = x86_64-w64-mingw32-ar cqs
QMAKE_RC = x86_64-w64-mingw32-windres
SPEC
printf '#include "/opt/qt-windows/mkspecs/win32-g++/qplatformdefs.h"\n' > mkspec/qplatformdefs.h
# Bypass the generated wrapper: its unquoted $* splits qmake assignments.
/opt/qt-host/bin/qmake6 -qtconf /opt/qt-windows/bin/target_qt.conf \
    -spec "$build_dir/mkspec" /src/mvlink.pro \
    'CONFIG+=release' 'CONFIG-=debug debug_and_release import_plugins' \
    'QTPLUGIN=qwindows' "$@"
make -j"${BUILD_JOBS:-2}"

# Stage a complete package before copying it into the mode-specific output.
python3 /usr/local/bin/package-windows.py "$WINDOWS_LINKAGE" mvlink.exe package
mkdir -p /out
cp -R package/. /out/
