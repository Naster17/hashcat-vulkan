# Native Vulkan Shader Support Matrix

This table shows, for a selection of hash modes, whether a **hand-written native
Vulkan shader** is available, or whether the mode runs on Vulkan only through the
**clspv** OpenCL-C → SPIR-V translator.

Key:

* **Native** — a hand-written GLSL compute shader ships in `OpenCL/native/` and is
  used with `--native-vulkan`. Runs on pure Vulkan with no OpenCL-C translation.
* **clspv (translated)** — no native shader yet; hashcat translates the OpenCL C
  kernel to SPIR-V at kernel-load time via `clspv`. Still runs on Vulkan, just
  translated, and `--native-vulkan` is ignored for these modes.

## Support matrix

| Hash mode | Algorithm | Native Vulkan shader | Runtime path |
|-----------|-----------|----------------------|--------------|
| 22000 | WPA-PBKDF2-PMKID / WPA-PBKDF2-PMKID + EAPOL | Yes | Native (`--native-vulkan`) or clspv fallback |
| 0 | MD5 | No | clspv (translated) |
| 100 | SHA1 | No | clspv (translated) |
| 1000 | NTLM | No | clspv (translated) |
| 1800 | sha512crypt | No | clspv (translated) |
| 3200 | bcrypt | No | clspv (translated) |
| 5500 | NetNTLMv1 | No | clspv (translated) |
| 5600 | NetNTLMv2 | No | clspv (translated) |
| 2500 | WPA/WPA2 (legacy) | No | clspv (translated) |
| 16800 | WPA-PMKID-PBKDF2 (legacy) | No | clspv (translated) |
| 2500/22000 family (all others) | — | No | clspv (translated) |

## Notes

* Native shaders are opt-in: pass `--native-vulkan`. When a native shader is not
  available for a kernel, hashcat silently falls back to the clspv-translated
  module, so a mixed attack (e.g. one mode native, another translated) works
  without extra flags.
* The native path currently covers **mode 22000** end-to-end: `m22000_init`,
  `m22000_loop`, `m22000_comp`, `m22000_aux1`…`m22000_aux4` and a debug kernel,
  all linked into `OpenCL/native/m22000-pure.vksprv` by
  `tools/vknative-build.py`.
* To request a new mode get native support, add its GLSL kernel under
  `OpenCL/native/src/` and register it in the `KERNELS` map in
  `tools/vknative-build.py`, then rebuild the `.vksprv` module.

See [hashcat-vulkan.md](hashcat-vulkan.md) for the build steps, runtime
requirements and environment variables.
