
# Hashcat – Build Documentation

**Revision**: 1.7  
**Author**: See `docs/credits.txt`

---

##  Requirements

- **Python 3.12** or higher

Check your Python version:

```bash
$ python3 --version
# Expected output: Python 3.13.9
```

If you can't install Python ≥ 3.12 globally, you can use **pyenv**.

> If you're using `pyenv`, follow **all steps** below. Otherwise, follow only **steps 3 and 5**.

---

##  Building Hashcat – Step-by-Step

###  Step 1: Install dependencies and pyenv

#### On Linux

Install required libraries to build Python:

```bash
$ sudo apt install libbz2-dev libssl-dev libncurses5-dev libffi-dev libreadline-dev libsqlite3-dev liblzma-dev
```

Install `pyenv`:

```bash
$ curl https://pyenv.run | bash
```

> Follow the instructions shown after installation to set up your shell correctly.

#### On macOS

Install `pyenv` via Homebrew:

```bash
$ brew install pyenv
```

---

###  Step 2: Install Python using pyenv

Install Python 3.12 (or newer):

```bash
$ pyenv install 3.12
```

Check installed versions:

```bash
$ pyenv versions
# Example:
# * system
#   3.12.11
```

---

###  Step 3: Clone the Hashcat repository

```bash
$ git clone https://github.com/hashcat/hashcat.git
$ cd hashcat
```

---

###  Step 4: Set the local Python version

```bash
$ pyenv local 3.12.11
```

---

###  Step 5: Build Hashcat

```bash
$ make clean && make
```

The build produces `libhashcat.so.7` next to the `hashcat` binary. It holds the core, and the binary,
the modules, the bridges and the feeds all link against it. It has to travel with them when the tree
is copied somewhere else, and it is found beside the binary without anything set in the environment.
`make SHARED=0` builds the older arrangement instead, where every plugin carries its own copy of the
core.

---

###  Step 6 (Optional): Check the build

```bash
$ tools/test_package.sh
```

The binary is started, made to load every module, made to produce candidates through a feed, and
made to compile a kernel and crack a hash. It reads the directory you give it, the current one by
default, so it checks an unpacked archive the same way it checks a build tree. The last group of
checks needs one OpenCL device and a CPU is enough for all of them, `--no-device` leaves that group
out.

---

###  Step 7 (Optional): Install Hashcat (Linux only)

```bash
$ make install
```

Hashcat will use the following locations depending on your environment:

| Condition                                   | Session Files                          | Kernel Cache                          | Potfiles                              |
|--------------------------------------------|----------------------------------------|---------------------------------------|----------------------------------------|
| `$XDG_DATA_HOME` and `$XDG_CACHE_HOME` set | `$XDG_DATA_HOME/hashcat/sessions/`     | `$XDG_CACHE_HOME/hashcat/kernels/`    | `$XDG_DATA_HOME/hashcat/`              |
| Only `$XDG_DATA_HOME` set                  | `$XDG_DATA_HOME/hashcat/sessions/`     | `$HOME/.cache/hashcat/`               | `$XDG_DATA_HOME/hashcat/`              |
| Only `$XDG_CACHE_HOME` set                 | `$HOME/.local/share/hashcat/sessions/` | `$XDG_CACHE_HOME/hashcat/kernels/`    | `$HOME/.local/share/hashcat/`          |
| None of the above                          | `$HOME/.local/share/hashcat/sessions/` | `$HOME/.cache/hashcat/`               | `$HOME/.local/share/hashcat/`          |

---
##  Building Hashcat for Android

See: [BUILD_Android.md](BUILD_Android.md)

---

##  Building Hashcat with Docker

See: [BUILD_Docker.md](BUILD_Docker.md)

---

##  Building Hashcat for Windows

| Method                                 | Documentation                        |
|----------------------------------------|--------------------------------------|
| From macOS                             | [BUILD_macOS.md](BUILD_macOS.md)     |
| Using Windows Subsystem for Linux (WSL)| [BUILD_WSL.md](BUILD_WSL.md)         |
| Using Cygwin                           | [BUILD_CYGWIN.md](BUILD_CYGWIN.md)   |
| Using MSYS2                            | [BUILD_MSYS2.md](BUILD_MSYS2.md)     |
| From Linux                             | Run: `make win`                      |

The Windows build produces `hashcat.dll` beside `hashcat.exe`. It holds the core, and the executable,
the modules, the bridges and the feeds all import from it, so it has to travel with them. Windows
searches the directory of the executable, so nothing has to be set in the environment.
`make win SHARED=0` builds the older arrangement instead, where every plugin carries its own copy of
the core.

---

## Building hashcat with the Vulkan backend

This fork adds a native Vulkan compute backend. The short version:

1. Build the binary exactly as upstream (`make`) — no Vulkan SDK is required to
   compile, because `ext_vulkan.c` loads `libvulkan.so.1` at runtime via `dlopen`.
2. At runtime you need a Vulkan driver/loader and (for the default pipeline) the
   `clspv` compiler. The optional native shaders are already checked in under
   `OpenCL/native/`.

### What to download / install

| Component | Needed for | How to get it |
|-----------|------------|---------------|
| Vulkan loader + ICD | Running on any Vulkan GPU | Distro package, e.g. `apt install vulkan-tools libvulkan1 mesa-vulkan-drivers` (verify with `vulkaninfo`). |
| `clspv` | Default OpenCL-C -> SPIR-V translation | Build from https://github.com/google/clspv (needs LLVM), or drop a prebuilt `clspv` next to the `hashcat` binary. Point hashcat at it with `HASHCAT_CLSPV=/path/clspv`. |
| glslang + SPIRV-Tools | Rebuilding the native `.vksprv` shaders only | Distro packages `glslang-tools spirv-tools` (provides `glslangValidator`, `spirv-dis`, `spirv-as`, `spirv-link`, `spirv-val`), or the Vulkan SDK. |
| Python 3 | Rebuilding native shaders (`tools/vknative-build.py`) | Already required to build hashcat. |

### Build

```bash
$ make clean && make
```

The Vulkan code is compiled into `libhashcat`/`hashcat`; nothing else changes
compared to the generic build. To *run* on Vulkan you only need `libvulkan.so.1`
present; for the default (non-native) pipeline also have `clspv` reachable as
described above.

### Run

```bash
# pure-Vulkan on device #1, native shaders (mode 22000)
$ ./hashcat -d 1 --native-vulkan -m 22000 -a 3 ...

# auto-detected Vulkan, default clspv translation, ignore OpenCL
$ ./hashcat --backend-ignore-opencl -m 0 ...
```

Use `-I` to list the detected Vulkan devices. Native shaders currently exist for
mode 22000; every other mode transparently uses the clspv-translated path (see
[NATIVE_SUPPORT.md](NATIVE_SUPPORT.md)).

### Rebuild the native shaders (optional)

```bash
$ tools/vknative-build.py      # regenerates OpenCL/native/m22000-pure.vksprv
```

Full design, all environment variables and the Docker setup are in
[hashcat-vulkan.md](hashcat-vulkan.md).

---

## Done

Enjoy your fresh **Hashcat** binaries!
