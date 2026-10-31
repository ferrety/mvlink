# mvlink

Move files to another folder and leave symbolic links at their original locations.
mvlink provides a Qt interface with drag and drop, a destination picker, and an
operation log.

## Using mvlink

1. Drag files from your file manager into the window.
2. Click **Browse...** to choose an existing destination folder.
3. Click **Move Files**.
4. Check the status and log for the result. Failed or skipped files remain in the list.

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
