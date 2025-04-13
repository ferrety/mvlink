FROM ubuntu:latest

# Set non-interactive mode for apt
ENV DEBIAN_FRONTEND=noninteractive

# Install necessary and more packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    python3-pip \
    libgl1-mesa-dev \
    wget \
    qt6-base-dev \
    qt6-declarative-dev \
    qt6-tools-dev \
    qt6-tools-dev-tools \
    libqt6widgets6 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy project files
COPY . .
# Set environment variables for Qt


# Build the project
RUN qmake6 FileMover.pro && make

# Set the entry point
CMD ["./mvlink"]