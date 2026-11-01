# mvlink

Move files to another folder and leave links at their original locations, or create
links in a destination folder while keeping the original files in place.
mvlink provides a Qt interface with drag and drop, a destination picker, and an
operation log.

The project uses semantic versions (`MAJOR.MINOR.PATCH`), starting at `1.0.0`.
Run `mvlink --version` (or `mvlink -v`) to print the version without opening a
window or requiring a display.

## Using mvlink

1. Drag files from your file manager into the window.
2. Click **Browse...**, type a destination folder, or select a previously used folder from the destination dropdown.
3. Leave **Create hard links instead of symbolic links** unchecked for symbolic
   links, or check it for hard links.
4. Click **Move Files** to place the files in the destination and leave links at
   their original paths. Click **Link** to keep the original files and create
   links in the destination.
5. Check the status for the result. Expand **Activity** for operation details; it opens
   automatically on errors. Failed or skipped files remain in the list for retry.

The file panel shows filenames and source folders. Move, Link, and Clear List
become available after files are added. Hover over a failed file to read its error.

Hard links require the source and destination to be on the same filesystem.
Both paths share the same file content, so edits through either path affect the
other. A failed hard-link operation preserves the original file and stays in
the list for retry.

The hard-link checkbox is saved whenever it changes and restored next session.
Preferences and destination history use a per-user INI file selected by Qt:

- Linux: `$XDG_CONFIG_HOME/mvlink/mvlink.ini`, or `~/.config/mvlink/mvlink.ini`.
- Windows: `%APPDATA%\mvlink\mvlink.ini`.
- macOS: `~/Library/Preferences/mvlink/mvlink.ini`.

Existing destination history is copied from native settings when the INI file
has no history; the original settings are preserved.

Destinations are saved to history after a successful move. The 10 most recently
used folders remain available after restarting the app.

If a destination file already exists, mvlink asks before overwriting it. Files
that are already symbolic links are skipped. **Clear List** removes items from
the list without changing the files.

## Building for Linux with Docker

Requirements:

- Docker
- Docker Compose

Run commands from the project directory. Build the application with:

```bash
docker compose run --build --rm build
```

The executable is written to `./mvlink`. Running it directly on the host requires
the matching Qt 6 runtime libraries.

The window icon is embedded in the executable on Linux and Windows; Windows
Explorer also uses the embedded icon. To add mvlink to your Linux application
menu for the current user, install the launcher and icon alongside the executable:

```bash
install -Dm755 mvlink "$HOME/.local/bin/mvlink"
install -Dm644 assets/mvlink.desktop "$HOME/.local/share/applications/mvlink.desktop"
install -Dm644 assets/mvlink.png "$HOME/.local/share/icons/hicolor/256x256/apps/mvlink.png"
```

Ensure `$HOME/.local/bin` is on your `PATH`. Native qmake builds also support
`qmake6 mvlink.pro PREFIX="$HOME/.local"`, followed by `make` and `make install`.
Icon artwork and regeneration instructions are in [assets/README.md](assets/README.md).

### Running the application in Docker

You also need an X11 server and `xhost` on the host. Allow the container to use
the display, then launch the application:

```bash
xhost +local:docker
docker compose run --build --rm launch
```

After closing the application, revoke display access:

```bash
xhost -local:docker
```

The project directory is available inside the container at `/app`. To work with
other files, mount their folder too. For example:

```bash
docker compose run --build --rm -v /path/to/files:/path/to/files launch
```

Both the source and destination folders must be accessible inside the container.
Use the same absolute paths on the host and in the container if you want the
symbolic links to work on the host afterward.

### Running a make target

After building, run a target such as `clean`:

```bash
MAKE_TARGET=clean docker compose run --rm make
```

## Running tests

Build and run the tests in Docker:

```bash
docker compose run --build --rm test
```

The tests run without a display and build in a separate directory inside the
container. The command returns a nonzero exit status if the build or tests fail.

After building the application, run the packaging and headless version checks:

```bash
docker compose run --rm build python3 -m unittest discover -s tests -p 'test_*.py'
docker compose run --rm build python3 tests/test_version.py ./mvlink
```

## Building for Windows 11 (x64) with prebuilt Qt

The recommended Windows build downloads prebuilt Qt libraries and compiles only
mvlink. Use **Qt 6.8.3 for MinGW 64-bit** and its matching **MinGW 13.1.0**
compiler, installed with the Qt Online Installer. Select both components, or
install them from PowerShell with Python and aqtinstall:

```powershell
py -m pip install aqtinstall==3.3.0
py -m aqt install-qt windows desktop 6.8.3 win64_mingw -O C:\Qt
py -m aqt install-tool windows desktop tools_mingw1310 qt.tools.win64_mingw1310 -O C:\Qt
```

From the project directory, build and package using PowerShell:

```powershell
./scripts/build-windows.ps1 -QtRoot C:\Qt\6.8.3\mingw_64 -MingwRoot C:\Qt\Tools\mingw1310_64
```

The script builds in `build/windows-native/` and writes the standalone shared
package to `dist/windows/native-shared/`. It uses Qt's `windeployqt` to include
the required libraries, plugins, and compiler runtime. Keep the entire package
together when distributing or running it.

Add `-Test` to build and run the offscreen Qt tests and verify the staged
application's version, Windows metadata, and GUI startup with the SDK removed
from `PATH`. Symbolic-link tests require Developer Mode or an elevated shell.
The SDK directories may be omitted when `QT_ROOT_DIR` identifies Qt and the
matching compiler's `bin` directory is already on `PATH`.

GitHub Actions uses the same Qt/compiler versions and build script on a
Windows runner, with cached SDK downloads.

## Cross-building for Windows 11 (x64)

This alternative builds on Linux using Docker and compiles Qt from source.
It also supports static builds; the prebuilt Windows workflow above uses shared
libraries.

Requirements:

- Docker with Linux containers (Docker Desktop is supported)
- Docker Compose for the Compose commands below
- Sufficient disk space and time for the first build

Build the shared package (the default):

```bash
docker compose --profile windows run --build --rm windows
```

The package is written to `dist/windows/shared/`. To run or distribute it,
keep `mvlink.exe`, the Qt and MinGW DLLs, `qt.conf`, and the `platforms/`
folder together in their existing layout.

For a static executable, run:

```bash
WINDOWS_LINKAGE=static docker compose --profile windows run --build --rm windows
```

The executable is written to `dist/windows/static/mvlink.exe`.

Without Docker Compose, build the shared package with:

```bash
docker build -f Dockerfile.win --build-arg WINDOWS_LINKAGE=shared -t mvlink-windows-shared .
docker run --rm -v "${PWD}/dist/windows/shared:/out" mvlink-windows-shared
```

For a static build, replace `shared` with `static` in these commands.
The first build takes considerable time; later builds reuse Docker's cache.
The commands use a POSIX shell, such as Bash or a WSL shell.

## Versions and releases

[`version.txt`](version.txt) is the application version source. qmake reads it
through `version.pri` for the runtime version and Windows executable metadata.
Builds from source archives work without Git metadata. Normal release bumps are
managed by Release Please rather than editing the version manually.

Use Conventional Commits for changes merged into `master` (including squash
merge titles):

- `fix: ...` produces a patch release, such as `1.0.0` to `1.0.1`.
- `feat: ...` produces a minor release, such as `1.0.0` to `1.1.0`.
- `feat!: ...` or a `BREAKING CHANGE:` footer produces a major release,
  such as `1.0.0` to `2.0.0`.
- Documentation, test, and maintenance commits alone do not trigger releases.

After Linux and Windows checks pass on `master`, Release Please opens or updates
a release PR containing `version.txt`, `CHANGELOG.md`, and its release manifest.
The first release PR proposes **1.0.0** and collects the existing conventional
commit history. Review and merge that PR to create the `v1.0.0` tag and GitHub
Release. Later release PRs calculate their versions automatically.

In **Settings > Actions > General**, enable **Allow GitHub Actions to create
and approve pull requests**. The release job uses the repository's built-in
`GITHUB_TOKEN`, with contents, pull-request, and issue write permissions; no
additional secret is required. PRs created with that token do not automatically
trigger PR workflows. The merged release commit is checked by the `master` push
workflow before publication. If branch protection requires PR checks on the
generated release PR, a maintainer can close and reopen that PR to trigger them.

Each release is built from its exact tag and receives:

- `mvlink-<version>-linux-x86_64.tar.gz`: executable, launcher, icon, license,
  and instructions. Built on Ubuntu 24.04; requires matching Qt 6 runtime
  libraries (`sudo apt install libqt6widgets6` on Ubuntu 24.04).
- `mvlink-<version>-windows-x86_64.zip`: complete shared Windows package,
  including Qt/compiler DLLs, plugins, license, and version file.
- `SHA256SUMS`: checksums for both archives.

Release notes and GitHub's source archives are available immediately; binary
assets are attached after both packaging jobs succeed. If packaging fails,
open **Actions > Build and release > Run workflow**, select `master`, and enter
the existing release tag (for example `v1.0.0`). This rebuilds both packages
from that tag and replaces their assets and checksums without creating a new
version. The workflow rejects nonexistent releases or tag/version mismatches.
