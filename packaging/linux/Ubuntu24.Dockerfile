# Optional baseline build environment; never used by ordinary native builds.
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build python3 git ca-certificates libsdl2-dev libglm-dev \
    libgl1-mesa-dri xvfb xauth \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /source
