# Hashcat – Vulkan Compute Backend

**Revision**: 1.0
**Branch**: `vulkan-native`

---

## Overview

This fork adds a **native Vulkan compute backend** to hashcat. In addition to the
classic OpenCL and CUDA/HIP paths, hashcat can now run its attack kernels on any
Vulkan-capable GPU (AMD, Intel, NVIDIA, …) through the same dispatch, transfer and
status machinery used by the rest of the engine. Vulkan devices are discovered
automatically at startup and show up as ordinary GPU devices — you select them
with `-d` exactly like OpenCL devices, and you can hide the other backends with
`--backend-ignore-opencl` / `--backend-ignore-cuda` / … if you want a pure-Vulkan run.

There are **two shader pipelines**, and the difference matters:

| Pipeline | How the SPIR-V is produced | When it is used |
|----------|----------------------------|-----------------|
| **clspv (default)** | hashcat translates the existing OpenCL C kernel to SPIR-V **at kernel-load time** by invoking the external [`clspv`](https://github.com/google/clspv) compiler as a subprocess. | Always, unless a native shader is available *and* requested. |
| **`--native-vulkan`** | A hand-written native Vulkan GLSL compute shader (shipped pre-compiled as a `.vksprv` module) is loaded **instead of** the clspv-translated one for that kernel. | Only when `--native-vulkan` is passed *and* a matching native shader exists for the kernel in `OpenCL/native/`. |

The native path is not a separate dialect — the pre-built module carries the exact
same `NonSemantic.ClspvReflection` metadata (descriptor model, specialization
constants, entry-point naming) that the clspv path produces, so the runtime binds
buffers and launches dispatches identically. The native shaders just skip the
OpenCL-C → SPIR-V translation step and use GPU-side buffer copies/fills
(`vkCmdCopyBuffer` / `vkCmdFillBuffer`) plus pipelined command-buffer dispatch,
which removes the host↔device staging stalls that the generic path can hit.

> Native shaders are currently provided for **mode 22000** (WPA-PBKDF2-PMKID /
> WPA-PBKDF2-PMKID + EAPOL). For every other hash mode the backend transparently
> falls back to the clspv translation pipeline.

---

## Quick start

```bash
$ make                                    # no Vulkan SDK needed to build

# pure-Vulkan, native shaders, mode 22000
$ ./hashcat -d 1 --native-vulkan -m 22000 -a 3 ...

# Vulkan via the clspv translator, ignoring OpenCL
$ ./hashcat --backend-ignore-opencl -m 0 ...
```

List devices with `-I`. Which modes have native shaders: [NATIVE_SUPPORT.md](NATIVE_SUPPORT.md).

---

## Requirements

### At runtime — default (clspv) pipeline

* A Vulkan 1.1+ capable GPU and loader: `libvulkan.so.1` (resolved via `dlopen` at
  runtime, so **no Vulkan SDK is needed to *build* hashcat**).
* The `clspv` binary, found in this order:
  1. `$HASHCAT_CLSPV` (if set),
  2. a `clspv` shipped next to the `hashcat` executable (portable builds),
  3. `/usr/local/bin/clspv`, `/opt/clspv/build/bin/clspv`,
     `$HOME/opt/clspv-stable/build/bin/clspv`, `/usr/bin/clspv`.

If `clspv` is missing, the affected Vulkan kernel fails to build and hashcat logs
the compiler output.

### At runtime — `--native-vulkan` pipeline

* `libvulkan.so.1` (same as above).
* The pre-built module `OpenCL/native/<stem>.vksprv` shipped with the tree (e.g.
  `OpenCL/native/m22000-pure.vksprv`).
* **`clspv` is not required** for this pipeline — the shader is already SPIR-V.

### To rebuild / regenerate the native shaders

* Python 3.
* The Vulkan SDK (or the standalone tools) providing:
  * `glslangValidator`
  * `spirv-dis`, `spirv-as`, `spirv-link`, `spirv-val` (SPIRV-Tools)
* Then run:

  ```bash
  $ tools/vknative-build.py
  ```

  This compiles every GLSL kernel in `OpenCL/native/src/*.glsl`, links them into a
  single SPIR-V module and injects the `NonSemantic.ClspvReflection` metadata,
  writing `OpenCL/native/m22000-pure.vksprv`.

---

## Building hashcat (with the Vulkan backend)

The Vulkan backend is compiled into `libhashcat` / `hashcat` directly, so building
this fork is the same as upstream:

```bash
$ make clean && make
```

Notes:

* **No Vulkan SDK is needed to compile the binary.** `ext_vulkan.c` loads
  `libvulkan.so.1` with `hc_dlopen()` and resolves every function pointer at
  runtime, so the toolchain that builds hashcat is unchanged from upstream.
* The native `.vksprv` module is *not* rebuilt by `make`; it is checked in under
  `OpenCL/native/`. Regenerate it only when you edit the GLSL sources (see above).
* To actually *run* on Vulkan you still need `libvulkan.so.1` and — for the default
  pipeline — a `clspv` binary on the target machine (see Requirements).

Optional checks and install are identical to the generic build, see
[BUILD.md](BUILD.md) (step 6 `tools/test_package.sh`, step 7 `make install`).

---

## Usage

### Selecting the Vulkan backend

Vulkan devices are listed automatically. Pick them with `-d` and, optionally, hide
the other APIs:

```bash
# pure-Vulkan run on device #1 (WPA-PMKID, native shaders)
$ ./hashcat -d 1 --native-vulkan -m 22000 -a 3 ...

# let hashcat use whatever it finds, but never touch OpenCL
$ ./hashcat --backend-ignore-opencl -m 0 ...
```

Relevant options (also in `--help`):

| Option | Effect |
|--------|--------|
| `--native-vulkan` | Use hand-written native Vulkan shaders where available (falls back to clspv otherwise). |
| `--backend-ignore-vulkan` | Do not open the Vulkan interface on startup. |
| `-d`, `--backend-devices` | Restrict to the listed Vulkan/OpenCL device indices. |
| `-I`, `--backend-info` | Shows detected APIs and devices, including Vulkan. |

### Environment variables

| Variable | Meaning |
|----------|---------|
| `HASHCAT_CLSPV` | Explicit path to the `clspv` binary for the default pipeline. |
| `HASHCAT_VK_CLSPV_OPTS` | Extra flags passed to `clspv` (e.g. `-O 0`); tokenized on whitespace. |
| `HASHCAT_VK_DEBUG` | Prints the `clspv` command line and keeps the temporary `.cl`/`.log` files on compile failure. |
| `HASHCAT_VK_DEBUG_PWS` | After the decompress kernel, dumps `pws[0]` (password length + bytes) to stderr. |
| `HASHCAT_VK_DUMP` | Dumps selected kernel argument buffers to stderr for `m01800`, `m00400`, `m00500` (and others where added) after each dispatch. |
| `HASHCAT_VK_SYNC` | Restores fully synchronous (blocking) dispatch semantics; useful for A/B comparisons against the old path. |

---

## How it works

Both pipelines share one backend; `--native-vulkan` only swaps the SPIR-V source
(clspv output vs the prebuilt `.vksprv`). The runtime then binds buffers and
launches dispatches identically.

### Transfers are GPU-side

Buffer copies (`hc_vkCopyBuffer`) and fills (`hc_vkFillBuffer8`/`32`) run via
`vkCmdCopyBuffer` / `vkCmdFillBuffer` on the device. Previously these were CPU
`memcpy`/`memset` through `HOST_VISIBLE` memory, which starved the GPU between
launches and caused the 0%↔100% load cycling. Covered hot paths: `pws_amp →
pws_buf`, rules/combs/bfs staging, and `bzero`.

### Pipelined dispatch

Each kernel keeps two rotating command-buffer slots. A launch waits only on the
fence of the slot it reuses, so host preparation of launch *i+1* overlaps GPU
execution of launch *i*. Outside the attack loop (autotune, selftest, bridges,
hooks) dispatch stays blocking, via a scoped async window around `choose_kernel()`.
Every host readback of a device-written buffer is guarded by `hc_vkQueueIdle()`.

### Synchronous escape hatch

`HASHCAT_VK_SYNC=1` restores the old fully-synchronous dispatch and CPU-copy
paths, for A/B comparisons and debugging.

### Kernel cache

Native modules are cached as `<name>.nvk`, separate from clspv's `<name>.kernel`,
so toggling the flag never mixes translated and native builds. The cache is
rebuilt when the `.vksprv` source is newer than the cached file.

### Device discovery

Vulkan physical devices are enumerated as GPUs automatically. Software
rasterizers (`llvmpipe`, `lavapipe`, `swiftshader`) are skipped.

---

## Docker

`tools/docker/` provides reproducible container builds that compile `clspv` inside
the image and ship it next to `hashcat` (the backend picks it up automatically):

* `tools/docker/Dockerfile.debian` — Debian/glibc build.
* `tools/docker/Dockerfile.musl` — Alpine/musl build (matches the tested RADV/Mesa setup).
* `tools/docker/clspv-hashcat-m22000-fix.patch` — clspv fix required for the m22000 native shader.

Build via the Makefile:

```bash
$ make docker-musl      # -> dist/musl/hashcat-<version>-musl-<arch>.tar.gz
$ make docker-debian    # -> dist/glibc/hashcat-<version>-glibc-<arch>.tar.gz
$ make docker-musl-shell # interactive Alpine build environment
```

The produced tree already contains `clspv` beside the binary, so the default
pipeline works without extra setup.

---

## Portable package

`tools/package_portable.sh` builds a self-contained tree and, if `clspv` is found
at `/usr/local/bin/clspv` (or via `HASHCAT_CLSPV`), copies it in as `./clspv`:

```bash
$ tools/package_portable.sh <outdir> [tag]
```

On the target machine install only a Vulkan loader + ICD, e.g. `apt install
libvulkan1 …` (Debian/glibc) or `apk add vulkan-loader mesa-vulkan-ati`
(Alpine/musl). No OpenCL stack is required for the Vulkan backend.

---

## Performance notes

On the reference setup (AMD BC-250, RADV GFX1013, Mesa 25.2.7, Alpine musl) the
native Vulkan pipeline measured:

```
baseline  -> fixed   native-vulkan   126 kH/s -> 306 kH/s  (2.4x)
baseline  -> fixed   clspv           131 kH/s -> 326 kH/s  (2.5x)
GPU busy  steady-state   95-100% (was ~33%)
```

i.e. the GPU stays saturated instead of cycling 0%↔100% between launches. Self-test,
MD5 cracking output, `-w3` and `-a3` mode 22000 were all validated. Numbers depend
on hardware, driver and kernel; treat them as a pointer, not a guarantee.

---

## Troubleshooting

* **No Vulkan devices listed (`-I`).** Install a Vulkan loader + ICD
  (`libvulkan1` + `mesa-vulkan-drivers`, or the vendor ICD) and verify with
  `vulkaninfo`. Software rasterizers are skipped on purpose.
* **Kernel build fails / `clspv` not found.** The default pipeline needs `clspv`.
  Set `HASHCAT_CLSPV=/path/clspv`, drop `clspv` next to the binary, or install it
  to one of the searched paths. With `--native-vulkan` on mode 22000, `clspv` is
  not required.
* **`--native-vulkan` seems ignored.** Native shaders exist only for mode 22000;
  all other modes fall back to clspv transparently. See
  [NATIVE_SUPPORT.md](NATIVE_SUPPORT.md).
* **Performance lower than expected.** Compare with `HASHCAT_VK_SYNC=1` to isolate
  pipelining effects. Make sure you are not on a software rasterizer.
* **Self-test failure on a native kernel.** Rebuild the module with
  `tools/vknative-build.py` and confirm the GLSL sources match the tree; set
  `HASHCAT_VK_DEBUG` to inspect the dispatch/argument state.

---

## Implementation map

| Area | File |
|------|------|
| Runtime Vulkan interface (loaders, dispatch, transfers) | `src/ext_vulkan.c`, `include/ext_vulkan.h` |
| Backend integration, clspv invocation, native-shader selection | `src/backend.c` (`vk_clspv_compile`, `load_kernel`) |
| Device-type plumbing, options, usage text | `src/types.h`, `src/user_options.c`, `src/usage.c` |
| Native GLSL sources | `OpenCL/native/src/*.glsl` |
| Native module builder | `tools/vknative-build.py` |
| Container build | `tools/docker/` |
