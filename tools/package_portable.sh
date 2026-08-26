#!/usr/bin/env sh

##
## Assemble a portable Linux release tree out of a "make linux" build.
##
## Unlike tools/package_bin.sh this is Linux-only and does the packing itself,
## so it can run unattended inside a container build. The layout matches what
## hashcat expects at runtime: the frontend and the core library side by side,
## plugins one directory below them, kernels and data directories beside.
##
## Usage: tools/package_portable.sh <outdir> [tag]
##

set -e

VERSION=${PORTABLE_VERSION:-7.1.2}
MAJOR=${VERSION%%.*}

IN=.
OUT=$1

if [ -z "$OUT" ]; then
  echo "! usage: $0 <outdir>" >&2
  exit 1
fi

TAG=$2
[ -n "$TAG" ] || TAG="portable"

# artifacts a usable tree cannot do without; say which one is missing rather
# than pack something that looks fine and cannot run

for artifact in hashcat.bin "libhashcat.so.$MAJOR"; do

  if [ -f "$IN/$artifact" ]; then
    continue
  fi

  echo "! $artifact was not built. Run 'make linux' first." >&2
  exit 1

done

rm -rf "$OUT"
mkdir -p "$OUT"

cp    "$IN/hashcat.bin"            "$OUT/"
cp    "$IN/libhashcat.so.$MAJOR"   "$OUT/"
cp    "$IN/hashcat.hcstat2"        "$OUT/"

# the runtime search path of every artifact points one level up from its own
# directory for the core, so modules/, bridges/ and feeds/ must stay where
# they are relative to hashcat.bin

cp -r "$IN/modules"                "$OUT/modules"
cp -r "$IN/bridges"                "$OUT/bridges"
cp -r "$IN/feeds"                  "$OUT/feeds"

cp -r "$IN/OpenCL"                 "$OUT/OpenCL"

cp -r "$IN/charsets"               "$OUT/"
cp -r "$IN/layouts"                "$OUT/"
cp -r "$IN/masks"                  "$OUT/"
cp -r "$IN/rules"                  "$OUT/"
cp -r "$IN/extra"                  "$OUT/"
cp -r "$IN/tunings"                "$OUT/"

if [ -d "$IN/pcfg" ]; then
  cp -r "$IN/pcfg"                 "$OUT/"
fi

cp -r "$IN/docs"                   "$OUT/docs"

cp    "$IN/examples/example.dict"           "$OUT/" 2>/dev/null || true
cp    "$IN/examples/example"[0123456789]*.hash "$OUT/" 2>/dev/null || true
cp    "$IN/examples/example"[0123456789]*.cmd  "$OUT/" 2>/dev/null || true

mkdir -p "$OUT/tools"
cp    "$IN/tools"/*hashcat.pl      "$OUT/tools/" 2>/dev/null || true
cp    "$IN/tools"/*hashcat.py      "$OUT/tools/" 2>/dev/null || true

# the example commands call ./hashcat, which does not exist in this tree

for example in "$IN"/example[0123456789]*.sh; do
  [ -f "$example" ] || continue
  sed 's!./hashcat !./hashcat.bin !' "$example" > "$OUT/${example##*/}"
done

# strip what the container built but the archive does not ship

find "$OUT/modules" "$OUT/bridges" "$OUT/feeds" -name '*.mk' -delete 2>/dev/null || true
find "$OUT/modules" "$OUT/bridges" "$OUT/feeds" -name '*.c'  -delete 2>/dev/null || true

# note what the tree needs from the host it runs on

CLSPV_NOTE="A clspv compiler is NOT included; install it or point HASHCAT_CLSPV at one."

if [ -f /usr/local/bin/clspv ] && cp /usr/local/bin/clspv "$OUT/clspv" 2>/dev/null; then
  CLSPV_NOTE="The OpenCL C -> SPIR-V compiler (clspv) needed by the Vulkan backend is included as ./clspv and is picked up automatically."
fi

cat > "$OUT/README-portable.txt" <<EOF
hashcat $VERSION ($TAG portable build)

Everything here is linked against $( [ "$TAG" = "musl" ] && echo musl || echo "the C library of the build image" ).
The OpenCL ICD loader (libOpenCL.so) and the Vulkan loader (libvulkan.so) are
loaded at runtime through dlopen and are NOT bundled:

  Alpine / musl : apk add vulkan-loader mesa-vulkan-ati   # plus an ICD
  Debian/glibc  : apt install libvulkan1 ocl-icd-libopencl2

$CLSPV_NOTE

Unpack and run ./hashcat.bin from the top directory.
EOF

echo "== packaged into $OUT =="
