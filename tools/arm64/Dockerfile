# syntax=docker/dockerfile:1
# OpenUG2 aarch64 + OpenGL ES builder for ArkOS/dArkOS handhelds (R36S).
#
# Ubuntu 20.04 is deliberate: its glibc 2.31 is the oldest runtime current
# ArkOS images provide, so a binary linked here also starts on newer images.
# (Debian bullseye has the same glibc but its security pool is no longer
# fetchable now that Debian 11 is past LTS.) The target device supplies SDL2
# and the Mali GLES driver; libpng/libcrypto are linked statically by
# build-in-container.sh.
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        file \
        libgles2-mesa-dev \
        libpng-dev \
        libsdl2-dev \
        libssl-dev \
        pkg-config \
        vim-common \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# No WORKDIR: build-in-container.sh recreates /work, and a container started
# inside a directory it deletes loses getcwd() for the rest of the build.
