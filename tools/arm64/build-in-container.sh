#!/bin/sh
# Builds the aarch64 + OpenGL ES binary inside the image from ./Dockerfile
# (Ubuntu 20.04, glibc 2.31). Invoked by build.ps1; all paths are container paths.
set -eu

SRC=${SRC:-/src}
WORK=${WORK:-/work}
OUT=${OUT:-/out}

echo "== copying sources (CRLF -> LF on the throwaway copy) =="
# Leave any directory we are about to delete: the container may start in $WORK.
cd /
rm -rf "$WORK"
mkdir -p "$WORK" "$OUT"
tar -C "$SRC" --exclude=.git --exclude=dist --exclude=build -cf - . | tar -C "$WORK" -xf -

# The Windows checkout uses core.autocrlf; make, sed, xxd and the preprocessor
# all expect LF. Strip it only on this disposable copy.
find "$WORK" -type f \( -name '*.c' -o -name '*.h' -o -name '*.sh' -o -name 'Makefile' \
    -o -name '*.vert' -o -name '*.frag' -o -name '*.txt' -o -name '*.md' \) \
    -print0 | xargs -0 -r sed -i 's/\r$//'

echo "== toolchain =="
gcc --version | head -n 1
sdl2-config --version
pkg-config --modversion libpng libcrypto

echo "== build (make gles) =="
cd "$WORK"
make gles \
    CC=gcc \
    UGUI_IMAGE_LIBS='-Wl,-Bstatic -lpng16 -lcrypto -Wl,-Bdynamic -ldl -lpthread'

echo "== verify =="
file nfsu2
echo "-- NEEDED --"
readelf -d nfsu2 | grep NEEDED || true
echo "-- highest GLIBC symbol versions --"
objdump -T nfsu2 | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -n 8

install -m 0755 nfsu2 "$OUT/nfsu2"
sha256sum "$OUT/nfsu2"
echo "== done =="
