# mvlink

Move files to another folder and leave links at their original locations, or create
links in a destination folder while keeping the original files in place.
mvlink provides a Qt interface with drag and drop, a destination picker, and an
operation log.

## Using mvlink

1. Drag files from your file manager into the window.
2. Click **Browse...**, type a destination folder, or select a previously used folder from the destination dropdown.
3. Leave **Create hard links instead of symbolic links** unchecked for symbolic
   links, or check it for hard links.
4. Click **Move Files** to place the files in the destination and leave links at
   their original paths. Click **Link** to keep the original files and create
   links in the destination.
5. Check the status and log for the result. Failed or skipped files remain in the list.

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

## Cross-building for Windows 11 (x64)

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
