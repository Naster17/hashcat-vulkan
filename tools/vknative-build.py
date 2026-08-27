#!/usr/bin/env python3
##
## Author......: See docs/credits.txt
## License.....: MIT
##
## Builds hand-written native Vulkan compute shaders (GLSL) into a single
## SPIR-V module carrying NonSemantic.ClspvReflection metadata, so that
## hashcat's Vulkan runtime consumes them exactly like clspv-generated
## modules (same descriptor model, same specialization constants, same
## entry point naming).
##
## Usage: tools/vknative-build.py [output.spv]
##

import math
import os
import struct
import subprocess
import sys
import tempfile

TOOL_DIR = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOL_DIR)
SRC_DIR = os.path.join(ROOT, "OpenCL", "native", "src")

KERNELS = {
    "m22000_init":  [(0, 0), (4, 1), (18, 2), (24, 3)],
    "m22000_loop":  [(4, 1), (24, 3)],
    "m22000_comp":  [],
    "m22000_aux1":  [(4, 1), (18, 2), (24, 3), (14, 4), (16, 5), (19, 6)],
    "m22000_aux2":  [(4, 1), (18, 2), (24, 3), (14, 4), (16, 5), (19, 6)],
    "m22000_aux3":  [(4, 1), (18, 2), (24, 3), (14, 4), (16, 5), (19, 6)],
    "m22000_aux4":  [(4, 1), (18, 2), (24, 3), (14, 4), (16, 5), (19, 6),
                     (6, 7), (7, 8), (8, 9), (9, 10), (10, 11), (11, 12),
                     (12, 13), (13, 14), (15, 15)],
}

ARG_NAMES = {
    0: "pws", 4: "tmps", 6: "bitmaps_buf_s1_a", 7: "bitmaps_buf_s1_b",
    8: "bitmaps_buf_s1_c", 9: "bitmaps_buf_s1_d", 10: "bitmaps_buf_s2_a",
    11: "bitmaps_buf_s2_b", 12: "bitmaps_buf_s2_c", 13: "bitmaps_buf_s2_d",
    14: "plains_buf", 15: "digests_buf", 16: "hashes_shown", 18: "esalt_bufs",
    19: "d_return_buf", 24: "kernel_param",
}


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write("command failed: %s\n%s\n%s\n" % (" ".join(cmd), r.stdout, r.stderr))
        sys.exit(1)
    return r


# ---------------------------------------------------------------------------
# generated crypto primitives (fully unrolled)
# ---------------------------------------------------------------------------

def gen_sha1_block():
    sched = ["w0","w1","w2","w3","w4","w5","w6","w7","w8","w9","wa","wb","wc","wd","we","wf"]
    L = []
    L.append("void sha1_block (inout uint st0, inout uint st1, inout uint st2, inout uint st3, inout uint st4, const uint wi[16])")
    L.append("{")
    L.append("  uint a = st0, b = st1, c = st2, d = st3, e = st4;")
    L.append("  uint w0  = wi[ 0]; uint w1  = wi[ 1]; uint w2  = wi[ 2]; uint w3  = wi[ 3];")
    L.append("  uint w4  = wi[ 4]; uint w5  = wi[ 5]; uint w6  = wi[ 6]; uint w7  = wi[ 7];")
    L.append("  uint w8  = wi[ 8]; uint w9  = wi[ 9]; uint wa  = wi[10]; uint wb  = wi[11];")
    L.append("  uint wc  = wi[12]; uint wd  = wi[13]; uint we  = wi[14]; uint wf  = wi[15];")
    for i in range(16, 80):
        s = f"w{i}"
        L.append(f"  uint {s} = ROTL ({sched[i-3]} ^ {sched[i-8]} ^ {sched[i-14]} ^ {sched[i-16]}, 1);")
        sched.append(s)
    for i in range(80):
        if i < 20:
            k, f = "0x5a827999u", "((b & c) | (~b & d))"
        elif i < 40:
            k, f = "0x6ed9eba1u", "(b ^ c ^ d)"
        elif i < 60:
            k, f = "0x8f1bbcdcu", "((b & c) | (b & d) | (c & d))"
        else:
            k, f = "0xca62c1d6u", "(b ^ c ^ d)"
        L.append(f"  {{ uint tt = ROTL (a, 5) + ({f}) + e + {k} + {sched[i]}; e = d; d = c; c = ROTL (b, 30); b = a; a = tt; }}")
    L.append("  st0 += a; st1 += b; st2 += c; st3 += d; st4 += e;")
    L.append("}")
    return "\n".join(L)


def gen_md5_block():
    # Emitted as a runtime loop (not fully unrolled): the glslang/RADV toolchain
    # miscompiles the 64-round fully-unrolled expansion (empty-block MD5 comes
    # out wrong on the GPU, though the same source is correct in analysis). The
    # loop form with table constants compiles correctly.
    S = [7,12,17,22]*4 + [5,9,14,20]*4 + [4,11,16,23]*4 + [6,10,15,21]*4
    K = [int(abs(math.sin(i + 1)) * (1 << 32)) & 0xffffffff for i in range(64)]
    L = []
    L.append("const uint MD5_S[64] = uint[](%s);" % ", ".join("%du" % v for v in S))
    L.append("const uint MD5_K[64] = uint[](%s);" % ", ".join("0x%08xu" % v for v in K))
    L.append("void md5_block (inout uint st0, inout uint st1, inout uint st2, inout uint st3, const uint wi[16])")
    L.append("{")
    L.append("  uint a = st0, b = st1, c = st2, d = st3;")
    L.append("  for (int i = 0; i < 64; i++)")
    L.append("  {")
    L.append("    uint f; int gi;")
    L.append("    if (i < 16)      { f = d ^ (b & (c ^ d)); gi = i; }")
    L.append("    else if (i < 32) { f = c ^ (d & (b ^ c)); gi = (5 * i + 1) % 16; }")
    L.append("    else if (i < 48) { f = b ^ c ^ d;         gi = (3 * i + 5) % 16; }")
    L.append("    else             { f = c ^ (b | ~d);      gi = (7 * i) % 16; }")
    L.append("    uint tmp = d; d = c; c = b;")
    L.append("    b = b + ROTL (a + f + MD5_K[i] + wi[gi], MD5_S[i]);")
    L.append("    a = tmp;")
    L.append("  }")
    L.append("  st0 += a; st1 += b; st2 += c; st3 += d;")
    L.append("}")
    return "\n".join(L)


SHA256_K = [
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
]


def gen_sha256_block():
    # textbook SHA-256. GF(2) rotations are written with the SHA-256 ROTR
    # amounts converted to ROTL (ROTR n == ROTL 32-n).
    # NOTE: kept as a fully expanded schedule. A rotating 16-word window was
    # tried for lower register pressure, but the self-referential ring pattern
    # is miscompiled by the glslang/RADV toolchain (WPA3/keyver3 breaks), the
    # same class of issue documented in gen_md5_block(). sha256_block is only
    # used by the WPA3 aux kernel (once per candidate), not the hot PBKDF2
    # loop, so the expansion cost here is negligible.
    sched = [f"wi[{i}]" for i in range(16)]
    L = []
    L.append("void sha256_block (inout uint st0, inout uint st1, inout uint st2, inout uint st3, inout uint st4, inout uint st5, inout uint st6, inout uint st7, const uint wi[16])")
    L.append("{")
    L.append("  uint a = st0, b = st1, c = st2, d = st3, e = st4, f = st5, g = st6, h = st7;")
    for i in range(16, 64):
        w15 = sched[i - 15]; w2 = sched[i - 2]; w7 = sched[i - 7]; w16 = sched[i - 16]
        # sigma0(x) = ROTR 7 ^ ROTR 18 ^ >> 3   (= ROTL 25 ^ ROTL 14 ^ >> 3)
        # sigma1(x) = ROTR 17 ^ ROTR 19 ^ >> 10 (= ROTL 15 ^ ROTL 13 ^ >> 10)
        L.append(f"  uint w{i} = {w16} + (ROTL ({w15}, 25) ^ ROTL ({w15}, 14) ^ ({w15} >> 3)) + {w7} + (ROTL ({w2}, 15) ^ ROTL ({w2}, 13) ^ ({w2} >> 10));")
        sched.append(f"w{i}")
    for i in range(64):
        # Sigma1(e) = ROTR 6  ^ ROTR 11 ^ ROTR 25 (= ROTL 26 ^ ROTL 21 ^ ROTL 7)
        # Sigma0(a) = ROTR 2  ^ ROTR 13 ^ ROTR 22 (= ROTL 30 ^ ROTL 19 ^ ROTL 10)
        t1 = (f"h + (ROTL (e, 26) ^ ROTL (e, 21) ^ ROTL (e, 7))"
              f" + ((e & f) ^ (~e & g)) + {SHA256_K[i]:#010x}u + {sched[i]}")
        t2 = f"(ROTL (a, 30) ^ ROTL (a, 19) ^ ROTL (a, 10)) + ((a & b) ^ (a & c) ^ (b & c))"
        L.append(f"  {{ uint tt1 = {t1}; uint tt2 = {t2}; h = g; g = f; f = e; e = d + tt1; d = c; c = b; b = a; a = tt1 + tt2; }}")
    L.append("  st0 += a; st1 += b; st2 += c; st3 += d; st4 += e; st5 += f; st6 += g; st7 += h;")
    L.append("}")
    return "\n".join(L)


def gen_sha1_block_dual():
    """interleaved dual SHA1 blocks: two independent compressions per call,
    rounds interleaved for 2x instruction-level parallelism"""
    aw = ["aw%d" % i for i in range(80)]
    bw = ["bw%d" % i for i in range(80)]
    L = []
    L.append("void sha1_block_dual (inout uint a0, inout uint a1, inout uint a2, inout uint a3, inout uint a4,")
    L.append("                     inout uint b0, inout uint b1, inout uint b2, inout uint b3, inout uint b4,")
    L.append("                     const uint wi0[16], const uint wi1[16])")
    L.append("{")
    L.append("  uint aa = a0, ab = a1, ac = a2, ad = a3, ae = a4;")
    L.append("  uint ba = b0, bb = b1, bc = b2, bd = b3, be = b4;")
    for i in range(16):
        L.append("  uint aw%d = wi0[%d];" % (i, i))
        L.append("  uint bw%d = wi1[%d];" % (i, i))
    for i in range(16, 80):
        L.append("  uint aw%d = ROTL (aw%d ^ aw%d ^ aw%d ^ aw%d, 1);" % ((i, i-3, i-8, i-14, i-16)))
        L.append("  uint bw%d = ROTL (bw%d ^ bw%d ^ bw%d ^ bw%d, 1);" % ((i, i-3, i-8, i-14, i-16)))
    def fexp(p, q, r, s, i):
        # p,q,r,s = a,b,c,d variables of that chain
        if i < 20:
            return "((%s & %s) | (~%s & %s))" % (q, r, q, s)
        if i < 40:
            return "(%s ^ %s ^ %s)" % (q, r, s)
        if i < 60:
            return "((%s & %s) | (%s & %s) | (%s & %s))" % (q, r, q, s, r, s)
        return "(%s ^ %s ^ %s)" % (q, r, s)
    Ks = [0x5a827999]*20 + [0x6ed9eba1]*20 + [0x8f1bbcdc]*20 + [0xca62c1d6]*20
    AV = ["aa", "ab", "ac", "ad", "ae"]
    BV = ["ba", "bb", "bc", "bd", "be"]
    for i in range(80):
        k = "0x%08xu" % Ks[i]
        wa = ("aw%d" % i) if i >= 16 else ("aw%d" % i)
        wb = ("bw%d" % i) if i >= 16 else ("bw%d" % i)
        fa = fexp("aa", "ab", "ac", "ad", i)
        fb = fexp("ba", "bb", "bc", "bd", i)
        L.append("  { uint ta = ROTL (aa, 5) + (%s) + ae + %s + %s;" % (fa, k, wa))
        L.append("    uint tb = ROTL (ba, 5) + (%s) + be + %s + %s;" % (fb, k, wb))
        L.append("    ae = ad; ad = ac; ac = ROTL (ab, 30); ab = aa; aa = ta;")
        L.append("    be = bd; bd = bc; bc = ROTL (bb, 30); bb = ba; ba = tb; }")
    L.append("  a0 += aa; a1 += ab; a2 += ac; a3 += ad; a4 += ae;")
    L.append("  b0 += ba; b1 += bb; b2 += bc; b3 += bd; b4 += be;")
    L.append("}")
    return "\n".join(L)


def extract_aes_tables():
    src = open(os.path.join(ROOT, "OpenCL", "inc_cipher_aes.cl")).read()
    out = []
    for name in ("te0", "te1", "te2", "te3", "te4"):
        i = src.index("%s[256]" % name)
        j = src.index("{", i)
        k = src.index("};", j)
        vals = "".join(src[j:k].split()).lstrip("{").rstrip(",").rstrip("}")
        parts = [p.strip() + ("u" if not p.strip().endswith("u") else "") for p in vals.split(",")]
        if len(parts) != 256:
            raise RuntimeError("%s has %d entries" % (name, len(parts)))
        out.append("const uint %s[256] = {%s};" % (name, ",".join(parts)))
    return "\n".join(out)


# ---------------------------------------------------------------------------
# static helper GLSL (no buffer references; fixed-size array parameters,
# messages are always passed as uint[MSGSZ])
# ---------------------------------------------------------------------------

MSGSZ = 96

COMMON_HELPERS = r"""
uint bswap32 (const uint v)
{
  return ((v & 0x000000ffu) << 24)
       | ((v & 0x0000ff00u) <<  8)
       | ((v & 0x00ff0000u) >>  8)
       | ((v & 0xff000000u) >> 24);
}

uint be_byte (const uint words[16], const int pos)
{
  return (words[pos >> 2] >> (24 - 8 * (pos & 3))) & 0xffu;
}
"""


def gen_streaming(name, nstate, iv_expr, block_fn, le):
    """message streaming after a midstate; le selects md5 padding rules"""
    args = ", ".join("inout uint s%d" % i for i in range(nstate))
    L = []
    L.append("void %s (%s, const uint words[%d], const int len_bytes, const uint prefix_len)" % (name, args, MSGSZ))
    L.append("{")
    L.append("  int full = len_bytes / 64;")
    L.append("")
    L.append("  for (int b = 0; b < full; b++)")
    L.append("  {")
    L.append("    uint w[16];")
    L.append("")
    L.append("    for (int i = 0; i < 16; i++) w[i] = words[b * 16 + i];")
    L.append("")
    L.append("    %s (%s);" % (block_fn, ", ".join("s%d" % i for i in range(nstate)) + ", w"))
    L.append("  }")
    L.append("")
    L.append("  int rem = len_bytes - full * 64;")
    L.append("")
    L.append("  uint tail[32];")
    L.append("")
    L.append("  for (int i = 0; i < 32; i++) tail[i] = 0;")
    L.append("")
    L.append("  int nw = rem / 4;")
    L.append("  int rmod = rem - nw * 4;")
    L.append("")
    L.append("  for (int i = 0; i < nw; i++) tail[i] = words[full * 16 + i];")
    L.append("")
    L.append("  if (rmod != 0)")
    L.append("  {")
    L.append("    uint v = words[full * 16 + nw];")
    L.append("    uint m = 0u;")
    L.append("")
    if le:
        L.append("    if (rmod >= 1) m |= 0x000000ffu;")
        L.append("    if (rmod >= 2) m |= 0x0000ffffu;")
        L.append("    if (rmod >= 3) m |= 0x00ffffffu;")
        L.append("")
        L.append("    tail[nw] = v & m;")
        L.append("    tail[nw] |= 0x80u << (8 * rmod);")
    else:
        L.append("    if (rmod >= 1) m |= 0xff000000u;")
        L.append("    if (rmod >= 2) m |= 0x00ff0000u;")
        L.append("    if (rmod >= 3) m |= 0x0000ff00u;")
        L.append("")
        L.append("    tail[nw] = v & m;")
        L.append("    tail[nw] |= 0x80u << (8 * (3 - rmod));")
    L.append("  }")
    L.append("  else")
    L.append("  {")
    if le:
        L.append("    tail[nw] = 0x00000080u;")
    else:
        L.append("    tail[nw] = 0x80000000u;")
    L.append("  }")
    L.append("")
    L.append("  uint total = prefix_len + uint(len_bytes);")
    L.append("")
    L.append("  if (rem >= 56)")
    L.append("  {")
    L.append("    uint first[16];")
    L.append("")
    L.append("    for (int i = 0; i < 16; i++) first[i] = tail[i];")
    L.append("")
    L.append("    %s (%s, first);" % (block_fn, ", ".join("s%d" % i for i in range(nstate))))
    L.append("")
    if le:
        L.append("    tail[30] = (total << 3);")
        L.append("    tail[31] = (total >> 29);")
    else:
        L.append("    tail[30] = (total >> 29);")
        L.append("    tail[31] = (total << 3);")
    L.append("")
    L.append("    uint second[16];")
    L.append("")
    L.append("    for (int i = 0; i < 16; i++) second[i] = tail[16 + i];")
    L.append("")
    L.append("    %s (%s, second);" % (block_fn, ", ".join("s%d" % i for i in range(nstate))))
    L.append("")
    L.append("    return;")
    L.append("  }")
    L.append("")
    if le:
        L.append("  tail[14] = (total << 3);")
        L.append("  tail[15] = (total >> 29);")
    else:
        L.append("  tail[14] = (total >> 29);")
        L.append("  tail[15] = (total << 3);")
    L.append("")
    L.append("  uint fin[16];")
    L.append("")
    L.append("  for (int i = 0; i < 16; i++) fin[i] = tail[i];")
    L.append("")
    L.append("  %s (%s, fin);" % (block_fn, ", ".join("s%d" % i for i in range(nstate))))
    L.append("}")
    return "\n".join(L)


def gen_hmac_key64(name, nstate, block_fn):
    ins = ", ".join("out uint ist%d" % i for i in range(nstate))
    outs = ", ".join("out uint ost%d" % i for i in range(nstate))
    L = []
    L.append("void %s (%s, %s, const uint key[16])" % (name, ins, outs))
    L.append("{")
    L.append("  uint a[16];")
    L.append("  uint b[16];")
    L.append("")
    L.append("  for (int i = 0; i < 16; i++)")
    L.append("  {")
    L.append("    a[i] = key[i] ^ 0x36363636u;")
    L.append("    b[i] = key[i] ^ 0x5c5c5c5cu;")
    L.append("  }")
    L.append("")
    iv = ["%s_INIT%d" % (name.split("_")[0].upper(), i) for i in range(nstate)]
    for pre, arr in (("ist", "a"), ("ost", "b")):
        for i in range(nstate):
            L.append("  %s%d = %s;" % (pre, i, iv[i]))
        L.append("  %s (%s, %s);" % (block_fn, ", ".join("%s%d" % (pre, i) for i in range(nstate)), arr))
        L.append("")
    L.append("}")
    return "\n".join(L)


def gen_hmac_core(name, nstate, digest_words, bitlen_base):
    ins = ", ".join("const uint istate[%d]" % nstate for _ in range(1))
    outs = ", ".join("out uint d%d" % i for i in range(nstate))
    L = []
    L.append("void %s (const uint istate[%d], const uint ostate[%d]," % (name, nstate, nstate))
    L.append("            const uint msg_words[%d], const int msg_len," % MSGSZ)
    L.append("            %s)" % outs)
    L.append("{")
    L.append("  uint ist[%d]; uint ost[%d];" % (nstate, nstate))
    L.append("")
    for i in range(nstate):
        L.append("  ist[%d] = istate[%d]; ost[%d] = ostate[%d];" % (i, i, i, i))
    L.append("")
    scalars = ", ".join("ist%d" % i for i in range(nstate))
    L.append("  uint %s;" % scalars)
    for i in range(nstate):
        L.append("  ist%d = ist[%d];" % (i, i))
    states = ", ".join("ist%d" % i for i in range(nstate))
    L.append("  %s (%s, msg_words, msg_len, 64);" % (name.split("_")[0], states))
    for i in range(nstate):
        L.append("  ist[%d] = ist%d;" % (i, i))
    L.append("")
    L.append("  uint inner[16];")
    L.append("")
    for i in range(nstate):
        L.append("  inner[%d] = ist[%d];" % (i, i))
    # 0x80 padding for the outer-HMAC block sits right after the inner digest
    # (byte offset nstate*4). BE hashes store it in the high byte of that word;
    # little-endian MD5 stores it in the low byte.
    if name.startswith("md5"):
        L.append("  inner[%d] = 0x00000080u;" % nstate)
    else:
        L.append("  inner[%d] = 0x80000000u;" % nstate)
    L.append("")
    L.append("  for (int i = %d; i < 14; i++) inner[i] = 0;" % (nstate + 1))
    L.append("")
    # sha1/sha256 use a big-endian 64-bit length (high word first);
    # md5 is little-endian (low word first)
    if name.startswith("md5"):
        L.append("  inner[14] = (%d + %d) * 8;" % (64, digest_words))
        L.append("  inner[15] = 0;")
    else:
        L.append("  inner[14] = 0;")
        L.append("  inner[15] = (%d + %d) * 8;" % (64, digest_words))
    L.append("")
    oscalars = ", ".join("ost%d" % i for i in range(nstate))
    L.append("  uint %s;" % oscalars)
    for i in range(nstate):
        L.append("  ost%d = ost[%d];" % (i, i))
    ostates = ", ".join("ost%d" % i for i in range(nstate))
    L.append("  %s_block (%s, inner);" % (name.split("_")[0], ostates))
    for i in range(nstate):
        L.append("  ost[%d] = ost%d;" % (i, i))
    L.append("")
    for i in range(nstate):
        L.append("  d%d = ost[%d];" % (i, i))
    L.append("}")
    return "\n".join(L)


def gen_aes():
    body = r"""
void aes128_expand_key (out uint ks[44], const uint ukey_s[4])
{
  ks[ 0] = ukey_s[0];
  ks[ 1] = ukey_s[1];
  ks[ 2] = ukey_s[2];
  ks[ 3] = ukey_s[3];

  for (int i = 4; i < 44; i++)
  {
    if ((i % 4) == 0)
    {
      const uint t = ks[i - 1];

      uint rcon;

      if (i ==  4) rcon = 0x01000000u;
      else if (i ==  8) rcon = 0x02000000u;
      else if (i == 12) rcon = 0x04000000u;
      else if (i == 16) rcon = 0x08000000u;
      else if (i == 20) rcon = 0x10000000u;
      else if (i == 24) rcon = 0x20000000u;
      else if (i == 28) rcon = 0x40000000u;
      else if (i == 32) rcon = 0x80000000u;
      else if (i == 36) rcon = 0x1b000000u;
      else              rcon = 0x36000000u;

      ks[i] = ks[i - 4] ^ rcon
            ^ (te2[(t >> 16) & 0xff] & 0xff000000u)
            ^ (te3[(t >>  8) & 0xff] & 0x00ff0000u)
            ^ (te0[(t      ) & 0xff] & 0x0000ff00u)
            ^ (te1[(t >> 24) & 0xff] & 0x000000ffu);
    }
    else
    {
      ks[i] = ks[i - 4] ^ ks[i - 1];
    }
  }
}
"""
    # round function emitted as statements via function
    enc = r"""
void aes128_encrypt_vk (const uint ks[44], const uint inw[4], out uint outw[4])
{
  uint in_s[4];

  in_s[0] = bswap32 (inw[0]);
  in_s[1] = bswap32 (inw[1]);
  in_s[2] = bswap32 (inw[2]);
  in_s[3] = bswap32 (inw[3]);

  uint s0 = in_s[0] ^ ks[0];
  uint s1 = in_s[1] ^ ks[1];
  uint s2 = in_s[2] ^ ks[2];
  uint s3 = in_s[3] ^ ks[3];

  uint t0, t1, t2, t3;
"""

    def rnd(src, dst, k):
        a0, a1, a2, a3 = src
        b0, b1, b2, b3 = dst
        return (
            "  { uint o0 = te0[%s >> 24] ^ te1[(%s >> 16) & 0xff] ^ te2[(%s >> 8) & 0xff] ^ te3[%s & 0xff] ^ ks[%d];\n"
            "    uint o1 = te0[%s >> 24] ^ te1[(%s >> 16) & 0xff] ^ te2[(%s >> 8) & 0xff] ^ te3[%s & 0xff] ^ ks[%d];\n"
            "    uint o2 = te0[%s >> 24] ^ te1[(%s >> 16) & 0xff] ^ te2[(%s >> 8) & 0xff] ^ te3[%s & 0xff] ^ ks[%d];\n"
            "    uint o3 = te0[%s >> 24] ^ te1[(%s >> 16) & 0xff] ^ te2[(%s >> 8) & 0xff] ^ te3[%s & 0xff] ^ ks[%d];\n"
            "    %s = o0; %s = o1; %s = o2; %s = o3; }\n"
            % (a0, a1, a2, a3, k,
               a1, a2, a3, a0, k + 1,
               a2, a3, a0, a1, k + 2,
               a3, a0, a1, a2, k + 3,
               b0, b1, b2, b3))

    k = 4
    src, dst = ["s0", "s1", "s2", "s3"], ["t0", "t1", "t2", "t3"]
    for r in range(9):
        enc += rnd(src, dst, k) + "\n"
        src, dst = dst, src
        k += 4

    fin = (
        "  outw[0] = bswap32 (((te4[(%s >> 24) & 0xff] & 0xff000000u)\n"
        "                   | (te4[(%s >> 16) & 0xff] & 0x00ff0000u)\n"
        "                   | (te4[(%s >>  8) & 0xff] & 0x0000ff00u)\n"
        "                   | (te4[(%s      ) & 0xff] & 0x000000ffu)) ^ ks[40]);\n"
        "  outw[1] = bswap32 (((te4[(%s >> 24) & 0xff] & 0xff000000u)\n"
        "                   | (te4[(%s >> 16) & 0xff] & 0x00ff0000u)\n"
        "                   | (te4[(%s >>  8) & 0xff] & 0x0000ff00u)\n"
        "                   | (te4[(%s      ) & 0xff] & 0x000000ffu)) ^ ks[41]);\n"
        "  outw[2] = bswap32 (((te4[(%s >> 24) & 0xff] & 0xff000000u)\n"
        "                   | (te4[(%s >> 16) & 0xff] & 0x00ff0000u)\n"
        "                   | (te4[(%s >>  8) & 0xff] & 0x0000ff00u)\n"
        "                   | (te4[(%s      ) & 0xff] & 0x000000ffu)) ^ ks[42]);\n"
        "  outw[3] = bswap32 (((te4[(%s >> 24) & 0xff] & 0xff000000u)\n"
        "                   | (te4[(%s >> 16) & 0xff] & 0x00ff0000u)\n"
        "                   | (te4[(%s >>  8) & 0xff] & 0x0000ff00u)\n"
        "                   | (te4[(%s      ) & 0xff] & 0x000000ffu)) ^ ks[43]);\n"
        "}\n"
        % (src[0], src[1], src[2], src[3],
           src[1], src[2], src[3], src[0],
           src[2], src[3], src[0], src[1],
           src[3], src[0], src[1], src[2]))

    return body + enc + fin


AUX_HELPERS = r"""
// wpa_t esalt layout in words (all u32 members)

#define WPA_ESSID_BUF    0u
#define WPA_ESSID_LEN   16u
#define WPA_TYPE        21u
#define WPA_PMKID       22u
#define WPA_PMKID_DATA  26u
#define WPA_KEYMIC      42u
#define WPA_KEYVER      54u
#define WPA_EAPOL       55u
#define WPA_EAPOL_LEN  135u
#define WPA_PKE        136u
#define WPA_NONCE_ERR  171u
#define WPA_NONCE_CMP  172u
#define WPA_DET_LE     173u
#define WPA_DET_BE     174u

#define WPA_STRIDE   175u

uint wpa_get (const uint base, const uint off)
{
  return raw2[base + off];
}

// plain_t is 8 words: gidvid u64, il_pos u64, salt_pos, digest_pos,
// hash_pos, extra1, extra2

void mark_hash_vk (const uint gid, const uint digest_pos, const uint final_hash_pos)
{
  const uint salt_pos     = KP_SALT_POS;
  const uint digests_cnt  = KP_DGST_CNT;

  const uint idx = atomicAdd (raw6[0], 1u);

  if (idx >= digests_cnt)
  {
    // mirror mark_hash(): undo the increment when out of range

    atomicAdd (raw6[0], ~0u);

    return;
  }

  const uint pbase = idx * 10u;   // plain_t: 2+2 u64 pad words + 5 u32 + pad -> 10 words (40 bytes)

  raw4[pbase + 0u] = gid;
  raw4[pbase + 1u] = 0u;
  raw4[pbase + 2u] = 0u;
  raw4[pbase + 3u] = 0u;
  raw4[pbase + 4u] = salt_pos;
  raw4[pbase + 5u] = digest_pos;      // relative
  raw4[pbase + 6u] = final_hash_pos;  // absolute
  raw4[pbase + 7u] = 0u;
}

bool claim_hash (const uint digest_cur)
{
  return atomicAdd (raw5[digest_cur], 1u) == 0u;
}

void load_pke (out uint pke[32], const uint base)
{
  for (int i = 0; i < 32; i++) pke[i] = wpa_get (base, WPA_PKE + uint(i));
}

void make_kn (inout uint k0, inout uint k1, inout uint k2, inout uint k3)
{
  uint kl0 = (k0 << 1) & 0xfefefefeu;
  uint kl1 = (k1 << 1) & 0xfefefefeu;
  uint kl2 = (k2 << 1) & 0xfefefefeu;
  uint kl3 = (k3 << 1) & 0xfefefefeu;

  uint kr0 = (k0 >> 7) & 0x01010101u;
  uint kr1 = (k1 >> 7) & 0x01010101u;
  uint kr2 = (k2 >> 7) & 0x01010101u;
  uint kr3 = (k3 >> 7) & 0x01010101u;

  const uint c = kr0 & 1u;

  kr0 = (kr0 >> 8) | (kr1 << 24);
  kr1 = (kr1 >> 8) | (kr2 << 24);
  kr2 = (kr2 >> 8) | (kr3 << 24);
  kr3 = kr3 >> 8;

  k0 = kl0 | kr0;
  k1 = kl1 | kr1;
  k2 = kl2 | kr2;
  k3 = kl3 | kr3;

  k3 ^= c * 0x87000000u;
}
"""


def build_common(kern, kparam_binding):
    """assemble the shared helper text for one kernel"""

    kp = ""
    if kparam_binding is None:
        kp = "// this kernel does not take a kernel_param buffer\n"
    else:
        kp = f"""
// kernel_param interface (binding {kparam_binding})

#define KB raw{kparam_binding}

#define KP_MASK      (KB[ 0])
#define KP_SHIFT1    (KB[ 1])
#define KP_SHIFT2    (KB[ 2])
#define KP_SALT_POS  (KB[ 3])
#define KP_LOOP_POS  (KB[ 4])
#define KP_LOOP_CNT  (KB[ 6])
#define KP_DGST_CNT (KB[10])
#define KP_DGST_OFF (KB[11])
#define KP_GID_LO   (KB[16])
#define KP_GID_HI   (KB[17])

bool kp_gid_valid (const uint gid)
{{
  if (KP_GID_HI != 0u) return true;

  return (gid < KP_GID_LO);
}}
"""

    helpers = f"""
// message streaming after a midstate (messages passed as uint[{MSGSZ}])

{gen_streaming('sha1', 5, None, 'sha1_block', False)}

{gen_hmac_key64('sha1_hmac_key64', 5, 'sha1_block')}

{gen_hmac_core('sha1_hmac_core', 5, 20, 84)}

{gen_streaming('md5', 4, None, 'md5_block', True)}

{gen_hmac_key64('md5_hmac_key64', 4, 'md5_block')}

{gen_hmac_core('md5_hmac_core', 4, 16, 80)}

{gen_streaming('sha256', 8, None, 'sha256_block', False)}

{gen_hmac_key64('sha256_hmac_key64', 8, 'sha256_block')}

{gen_hmac_core('sha256_hmac_core', 8, 32, 96)}
"""

    # NOTE: gen_hmac_* emit *_INIT constants; provide them here
    init_consts = """
#define SHA1_INIT0 0x67452301u
#define SHA1_INIT1 0xefcdab89u
#define SHA1_INIT2 0x98badcfeu
#define SHA1_INIT3 0x10325476u
#define SHA1_INIT4 0xc3d2e1f0u

#define MD5_INIT0 0x67452301u
#define MD5_INIT1 0xefcdab89u
#define MD5_INIT2 0x98badcfeu
#define MD5_INIT3 0x10325476u

#define SHA256_INIT0 0x6a09e667u
#define SHA256_INIT1 0xbb67ae85u
#define SHA256_INIT2 0x3c6ef372u
#define SHA256_INIT3 0xa54ff53au
#define SHA256_INIT4 0x510e527fu
#define SHA256_INIT5 0x9b05688cu
#define SHA256_INIT6 0x1f83d9abu
#define SHA256_INIT7 0x5be0cd19u

// md5_init()/sha256_init() aliases used by the generic generators

void md5_iv (out uint s0, out uint s1, out uint s2, out uint s3)
{
  s0 = MD5_INIT0; s1 = MD5_INIT1; s2 = MD5_INIT2; s3 = MD5_INIT3;
}
"""

    aes = ""
    if kern.startswith("m22000_aux"):
        aes = AUX_HELPERS + "\n" + gen_aes()

    return init_consts + kp + COMMON_HELPERS + helpers + aes


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "OpenCL", "native", "m22000-pure.vksprv")

    tmpdir = tempfile.mkdtemp(prefix="vknative")

    crypto = gen_sha1_block() + "\n\n" + gen_md5_block() + "\n\n" + gen_sha256_block() + "\n\n" + gen_sha1_block_dual()
    crypto += "\n\n" + extract_aes_tables() + "\n"

    spvs = []

    for kern, args in KERNELS.items():
        body = open(os.path.join(SRC_DIR, "%s.glsl" % kern)).read()

        prelude = "#version 450\n"
        prelude += "#define MSGSZ 96\n"
        prelude += "layout(local_size_x = 64, local_size_x_id = 0, local_size_y = 1, local_size_y_id = 1, local_size_z = 1, local_size_z_id = 2) in;\n"
        prelude += "#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))\n\n"

        decls = []
        kparam_binding = None
        for ordinal, binding in args:
            decls.append('layout(set = 0, binding = %d) restrict buffer B%d { uint raw%d[]; }; // ordinal %d (%s)'
                         % (binding, binding, binding, ordinal, ARG_NAMES.get(ordinal, "?")))
            if ordinal == 24:
                kparam_binding = binding
        if not decls:
            decls.append("// this kernel has no buffer arguments")
        decls.append("")

        common = build_common(kern, kparam_binding)

        src_text = prelude + "\n".join(decls) + "\n" + crypto + "\n" + common + "\n" + body

        srcfile = os.path.join(tmpdir, "%s.comp" % kern)
        open(srcfile, "w").write(src_text)

        spvfile = os.path.join(tmpdir, "%s.spv" % kern)
        run(["glslangValidator", "-V", "--target-env", "vulkan1.1", "-o", spvfile, srcfile])

        dis = run(["spirv-dis", spvfile]).stdout
        dis = dis.replace('OpEntryPoint GLCompute %main "main"', 'OpEntryPoint GLCompute %%main "%s"' % kern)
        dis = dis.replace('OpName %main "main"', 'OpName %%main "%s"' % kern)
        asmfile = os.path.join(tmpdir, "%s-renamed.spvasm" % kern)
        open(asmfile, "w").write(dis)
        run(["spirv-as", "--target-env", "vulkan1.1", asmfile, "-o", spvfile])

        spvs.append(spvfile)

    merged = os.path.join(tmpdir, "merged.spv")
    run(["spirv-link", "--target-env", "vulkan1.1"] + spvs + ["-o", merged])

    injected = inject_reflection(merged, tmpdir)

    run(["spirv-val", "--target-env", "vulkan1.1", injected])

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    data = open(injected, "rb").read()
    open(out_path, "wb").write(data)

    print("built %s (%d bytes, %d kernels)" % (out_path, len(data), len(KERNELS)))


def inject_reflection(merged, tmpdir):
    """append NonSemantic.ClspvReflection metadata describing every kernel"""

    code = open(merged, "rb").read()
    words = list(struct.unpack("<%dI" % (len(code) // 4), code))

    n = len(words)

    def op(i):
        return words[i] & 0xffff

    def wc(i):
        return words[i] >> 16

    bound_idx = 3
    bound = words[bound_idx]

    memory_model_idx = None
    first_func_idx = None
    first_type_idx = None
    entrypoints = []
    uint_type = None
    void_type = None

    TYPE_OPS = set(list(range(19, 33)) + [43, 44])

    i = 5
    while i < n:
        o = op(i)
        w = wc(i)
        if w == 0:
            break
        if o == 14:
            memory_model_idx = i
        elif o == 15:
            fn = words[i + 2]
            raw = words[i + 3:i + w]
            b = b"".join(struct.pack("<I", x) for x in raw)
            name = b.split(b"\x00")[0].decode()
            entrypoints.append((name, fn))
        elif o == 19:
            void_type = words[i + 1]
        elif o == 21:
            if words[i + 2] == 32 and words[i + 3] == 0:
                uint_type = words[i + 1]
        elif o == 54:
            if first_func_idx is None:
                first_func_idx = i
        elif o in TYPE_OPS:
            if first_type_idx is None and o not in (43, 44):
                first_type_idx = i
        i += w

    assert uint_type is not None, "missing uint type"
    assert void_type is not None, "missing void type"
    assert memory_model_idx is not None and first_func_idx is not None

    nid = bound

    def new_id():
        nonlocal nid
        nid += 1
        return nid

    def insn(opc, operands):
        return [(len(operands) + 1) << 16 | opc] + operands

    def str_words(s):
        b = s.encode() + b"\x00"
        while len(b) % 4:
            b += b"\x00"
        return list(struct.unpack("<%dI" % (len(b) // 4), b))

    str_insns = []
    str_ids = {}

    def get_str(s):
        if s not in str_ids:
            rid = new_id()
            str_ids[s] = rid
            str_insns.append(insn(7, [rid] + str_words(s)))
        return str_ids[s]

    kernel_entries = [(name, fn, KERNELS[name]) for (name, fn) in entrypoints if name in KERNELS]

    needed = set([len(a) for _, _, a in kernel_entries])
    for _, _, args in kernel_entries:
        for ordinal, binding in args:
            needed.add(ordinal)
            needed.add(binding)
        needed.add(0)

    const_map = {}
    const_insns = []
    for v in sorted(needed):
        cid = new_id()
        const_map[v] = cid
        const_insns.append(insn(43, [uint_type, cid, v]))

    import_id = new_id()
    ext_insn = insn(10, str_words("SPV_KHR_non_semantic_info"))  # OpExtension
    imp_insn = insn(11, [import_id] + str_words("NonSemantic.ClspvReflection.5"))

    refl_insns = []

    for name, fn, args in kernel_entries:
        kid = new_id()
        name_str = get_str(name)
        attr_str = get_str("")
        # %kid = OpExtInst %void %imp Kernel(fn, num_args, flags, attributes)
        refl_insns.append(insn(12, [void_type, kid, import_id, 1, fn, name_str,
                                    const_map[len(args)], const_map[0], attr_str]))
        for ordinal, binding in args:
            aname = get_str(ARG_NAMES.get(ordinal, "arg%d" % ordinal))
            ai = new_id()
            # %ai = OpExtInst %void %imp ArgumentInfo(name)
            refl_insns.append(insn(12, [void_type, ai, import_id, 2, aname]))
            sid = new_id()
            # %sid = OpExtInst %void %imp ArgumentStorageBuffer(kernel, ordinal, set, binding, arg_info)
            refl_insns.append(insn(12, [void_type, sid, import_id, 3, kid, const_map[ordinal],
                                        const_map[0], const_map[binding], ai]))

    # OpString belongs to the debug section: place it right before the first
    # debug-section instruction (OpSource/OpName/OpString/...) so that no
    # other instruction kind sits between

    DEBUG_OPS = (3, 4, 5, 6, 7, 330)

    strings_at = None
    i2 = 5
    while i2 < n:
        o2 = words[i2] & 0xffff
        w2 = words[i2] >> 16
        if o2 in DEBUG_OPS:
            strings_at = i2
            break
        i2 += w2
    if strings_at is None:
        i2 = 5
        while i2 < n:
            o2 = words[i2] & 0xffff
            w2 = words[i2] >> 16
            if o2 in (71, 72):
                strings_at = i2
                break
            i2 += w2
    if strings_at is None:
        strings_at = first_type_idx if first_type_idx is not None else first_func_idx

    def flat(insts):
        r = []
        for ins in insts:
            r += ins
        return r

    # find the first existing import so that the extension can precede it

    first_import_idx = None
    i3 = 5
    while i3 < n:
        o3 = words[i3] & 0xffff
        w3 = words[i3] >> 16
        if o3 == 11:
            first_import_idx = i3
            break
        i3 += w3

    ext_pos = first_import_idx if first_import_idx is not None else memory_model_idx

    out = []
    out += words[:ext_pos]
    out += ext_insn
    out += words[ext_pos:memory_model_idx]
    out += imp_insn
    out += words[memory_model_idx:strings_at]
    out += flat(str_insns)
    out += words[strings_at:first_func_idx]
    out += flat(const_insns)
    out += words[first_func_idx:]
    out += flat(refl_insns)

    out[bound_idx] = nid + 1

    outpath = os.path.join(tmpdir, "injected.spv")
    open(outpath, "wb").write(struct.pack("<%dI" % len(out), *out))
    return outpath


if __name__ == "__main__":
    main()
