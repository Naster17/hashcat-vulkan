//
// m22000_aux4: WPA deep-comp kernel, EAPOL hashes (type 1): compute
// HMAC-SHA1 (key = PMK) over pmkid_data and run the standard bitmap +
// digest buffer comparison
//
// bindings: 1 = tmps, 2 = esalt_bufs, 3 = kernel_param,
//           4 = plains_buf, 5 = hashes_shown, 6 = d_return_buf,
//           7..14 = bitmaps s1 a-d / s2 a-d, 15 = digests_buf
//

#define TMP_STRIDE 30u

#define CHECK_BM(bm, mask, shift, d) (((bm)[(((d) >> (shift)) & (mask))] & (1u << ((d) & 0x1fu))) != 0u)

int hash_comp (const uint d[4], const uint base)
{
  // digests are DGST_ELEM=4 words; host sorts them in R3,R2,R1,R0 order

  if (d[3] > raw15[base + 3]) return 1;
  if (d[3] < raw15[base + 3]) return -1;
  if (d[2] > raw15[base + 2]) return 1;
  if (d[2] < raw15[base + 2]) return -1;
  if (d[1] > raw15[base + 1]) return 1;
  if (d[1] < raw15[base + 1]) return -1;
  if (d[0] > raw15[base + 0]) return 1;
  if (d[0] < raw15[base + 0]) return -1;

  return 0;
}

void main()
{
  const uint gid = gl_GlobalInvocationID.x;

  if (!kp_gid_valid (gid)) return;

  const uint base = gid * TMP_STRIDE;

  const uint digest_pos = KP_LOOP_POS;
  const uint digest_cur = KP_DGST_OFF + digest_pos;

  const uint wpa_base = digest_cur * WPA_STRIDE;

  // this can occur on -a 9 because we are ignoring module_deep_comp_kernel()

  if (wpa_get (wpa_base, WPA_TYPE) != 1u) return;

  uint key[16];

  for (int i = 0; i < 8; i++) key[i] = raw1[base + 20u + uint(i)];
  for (int i = 8; i < 16; i++) key[i] = 0u;

  // pmkid_data: 20 bytes, swapped like sha1_hmac_update_global_swap()

  uint pdata[MSGSZ];

  for (int i = 0; i < MSGSZ; i++) pdata[i] = 0u;

  pdata[0] = bswap32 (wpa_get (wpa_base, WPA_PMKID_DATA + 0u));
  pdata[1] = bswap32 (wpa_get (wpa_base, WPA_PMKID_DATA + 1u));
  pdata[2] = bswap32 (wpa_get (wpa_base, WPA_PMKID_DATA + 2u));
  pdata[3] = bswap32 (wpa_get (wpa_base, WPA_PMKID_DATA + 3u));
  pdata[4] = bswap32 (wpa_get (wpa_base, WPA_PMKID_DATA + 4u));

  uint ist0, ist1, ist2, ist3, ist4;
  uint ost0, ost1, ost2, ost3, ost4;

  sha1_hmac_key64 (ist0, ist1, ist2, ist3, ist4, ost0, ost1, ost2, ost3, ost4, key);

  uint istate[5]; uint ostate[5];

  istate[0] = ist0; istate[1] = ist1; istate[2] = ist2; istate[3] = ist3; istate[4] = ist4;
  ostate[0] = ost0; ostate[1] = ost1; ostate[2] = ost2; ostate[3] = ost3; ostate[4] = ost4;

  uint r0, r1, r2, r3, r4;

  sha1_hmac_core (istate, ostate, pdata, 20, r0, r1, r2, r3, r4);

  // direct per-salt compare (mirrors OpenCL/m22000-pure.cl non-static path): the
  // computed HMAC state is byte-swapped before comparing against wpa->pmkid[].
  // This is what aux1..aux3 do; the bespoke bitmap/binary-search over digests_buf
  // that used to be here was order-sensitive and dropped valid salts.

  if ((bswap32 (r0) == wpa_get (wpa_base, WPA_PMKID + 0u))
   && (bswap32 (r1) == wpa_get (wpa_base, WPA_PMKID + 1u))
   && (bswap32 (r2) == wpa_get (wpa_base, WPA_PMKID + 2u))
   && (bswap32 (r3) == wpa_get (wpa_base, WPA_PMKID + 3u)))
  {
    if (claim_hash (digest_cur))
    {
      mark_hash_vk (gid, digest_pos, digest_cur);
    }
  }
}
