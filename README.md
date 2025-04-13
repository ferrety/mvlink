# mvlink

A Qt application that allows you to drag and drop files to move them to another directory while creating symbolic links from the new location back to the original location.

## Features

- Drag and drop files to select them for moving
- Browse the filesystem to select a destination directory
- Move files to the destination directory and create symbolic links in their original locations
- User-friendly interface with status updates

## Building and Running with Docker

### Prerequisites

- Docker
- Docker Compose
- X11 server running on host (for Linux GUI applications)

### Running the Application

1. Allow X server connections:
```bash
xhost +local:docker
```

2. Build and run the application:
```bash
docker-compose up --build
```

3. After using the application, you can revoke X server access:
```bash
xhost -local:docker
```

## How to Use

1. Drag and drop files from your file manager into the application window
2. Click "Browse..." to select a destination directory
3. Click "Move Files & Create Symlinks" to move the files and create symbolic links
4. The status at the bottom of the window will show the result of the operation
