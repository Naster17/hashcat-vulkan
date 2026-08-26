//
// m22000_aux3: WPA deep-comp kernel, PMKID + keyver 3 (sha256 + aes-cmac mic)
//
// bindings: 1 = tmps, 2 = esalt_bufs, 3 = kernel_param,
//           4 = plains_buf, 5 = hashes_shown, 6 = d_return_buf
//

#define TMP_STRIDE 30u

void main()
{
  const uint gid = gl_GlobalInvocationID.x;

  if (!kp_gid_valid (gid)) return;

  const uint base = gid * TMP_STRIDE;

  const uint digest_pos = KP_LOOP_POS;
  const uint digest_cur = KP_DGST_OFF + digest_pos;

  const uint wpa_base = digest_cur * WPA_STRIDE;

  const uint type   = wpa_get (wpa_base, WPA_TYPE);
  const uint keyver = wpa_get (wpa_base, WPA_KEYVER);

  if ((type != 2u) && (keyver != 3u)) return;

  uint pke[32];

  load_pke (pke, wpa_base);

  uint to;
  uint m0;
  uint m1;

  const int nonce_compare = int(wpa_get (wpa_base, WPA_NONCE_CMP));

  if (nonce_compare < 0)
  {
    m0 = pke[15] & ~0x000000ffu;
    m1 = pke[16] & ~0xffffff00u;

    to = (pke[15] << 24) | (pke[16] >> 8);
  }
  else
  {
    m0 = pke[23] & ~0x000000ffu;
    m1 = pke[24] & ~0xffffff00u;

    to = (pke[23] << 24) | (pke[24] >> 8);
  }

  uint bo_loops = wpa_get (wpa_base, WPA_DET_LE) + wpa_get (wpa_base, WPA_DET_BE);

  bo_loops = (bo_loops == 0u) ? 2u : bo_loops;

  const uint nonce_error_corrections = wpa_get (wpa_base, WPA_NONCE_ERR);

  for (uint nec = 0u; nec <= nonce_error_corrections; nec++)
  {
    for (uint bo_pos = 0u; bo_pos < bo_loops; bo_pos++)
    {
      uint t = to;

      if (bo_loops == 1u)
      {
        if (wpa_get (wpa_base, WPA_DET_LE) == 1u)
        {
          t -= nonce_error_corrections / 2u;
          t += nec;
        }
        else if (wpa_get (wpa_base, WPA_DET_BE) == 1u)
        {
          t = bswap32 (t);

          t -= nonce_error_corrections / 2u;
          t += nec;

          t = bswap32 (t);
        }
      }
      else
      {
        if (bo_pos == 0u)
        {
          t -= nonce_error_corrections / 2u;
          t += nec;
        }
        else if (bo_pos == 1u)
        {
          t = bswap32 (t);

          t -= nonce_error_corrections / 2u;
          t += nec;

          t = bswap32 (t);
        }
      }

      if (nonce_compare < 0)
      {
        pke[15] = m0 | (t >> 24);
        pke[16] = m1 | (t << 8);
      }
      else
      {
        pke[23] = m0 | (t >> 24);
        pke[24] = m1 | (t << 8);
      }

      // kck = hmac-sha256 (key = tmps.out, msg = pke)

      uint key[16];

      key[0] = raw1[base + 20u]; key[1] = raw1[base + 21u];
      key[2] = raw1[base + 22u]; key[3] = raw1[base + 23u];
      key[4] = raw1[base + 24u]; key[5] = raw1[base + 25u];
      key[6] = raw1[base + 26u]; key[7] = raw1[base + 27u];

      for (int i = 8; i < 16; i++) key[i] = 0u;

      uint ist0, ist1, ist2, ist3, ist4, ist5, ist6, ist7;
      uint ost0, ost1, ost2, ost3, ost4, ost5, ost6, ost7;

      sha256_hmac_key64 (ist0, ist1, ist2, ist3, ist4, ist5, ist6, ist7,
                         ost0, ost1, ost2, ost3, ost4, ost5, ost6, ost7, key);

      uint istate[8]; uint ostate[8];

      istate[0] = ist0; istate[1] = ist1; istate[2] = ist2; istate[3] = ist3;
      istate[4] = ist4; istate[5] = ist5; istate[6] = ist6; istate[7] = ist7;
      ostate[0] = ost0; ostate[1] = ost1; ostate[2] = ost2; ostate[3] = ost3;
      ostate[4] = ost4; ostate[5] = ost5; ostate[6] = ost6; ostate[7] = ost7;

      uint msg[MSGSZ];

      for (int i = 0; i < 32; i++) msg[i] = pke[i];
      for (int i = 32; i < MSGSZ; i++) msg[i] = 0u;

      uint kck0, kck1, kck2, kck3, kck4, kck5, kck6, kck7;

      sha256_hmac_core (istate, ostate, msg, 102,
                        kck0, kck1, kck2, kck3, kck4, kck5, kck6, kck7);

      // note: the clspv kernel swaps only the first four words here because
      // aes128_set_encrypt_key() swaps them back internally

      kck0 = bswap32 (kck0); kck1 = bswap32 (kck1);
      kck2 = bswap32 (kck2); kck3 = bswap32 (kck3);

      // aes-128 cmac over eapol

      uint ukey_s[4];

      ukey_s[0] = kck0; ukey_s[1] = kck1; ukey_s[2] = kck2; ukey_s[3] = kck3;

      uint ks[44];

      aes128_expand_key (ks, ukey_s);

      const int eapol_len = int(wpa_get (wpa_base, WPA_EAPOL_LEN));

      for (int i = 0; i < MSGSZ; i++) msg[i] = 0u;

      for (int i = 0; i < 80; i++) msg[i] = wpa_get (wpa_base, WPA_EAPOL + uint(i));

      uint m[4] = { 0u, 0u, 0u, 0u };
      uint iv[4] = { 0u, 0u, 0u, 0u };

      int eapol_left = eapol_len;
      int eapol_idx = 0;

      while (eapol_left > 16)
      {
        uint blk[4];

        blk[0] = msg[eapol_idx + 0] ^ iv[0];
        blk[1] = msg[eapol_idx + 1] ^ iv[1];
        blk[2] = msg[eapol_idx + 2] ^ iv[2];
        blk[3] = msg[eapol_idx + 3] ^ iv[3];

        uint cip[4];

        aes128_encrypt_vk (ks, blk, cip);

        iv[0] = cip[0]; iv[1] = cip[1]; iv[2] = cip[2]; iv[3] = cip[3];

        eapol_left -= 16;
        eapol_idx += 4;
      }

      m[0] = msg[eapol_idx + 0];
      m[1] = msg[eapol_idx + 1];
      m[2] = msg[eapol_idx + 2];
      m[3] = msg[eapol_idx + 3];

      // subkey: k = make_kn(aes(zero))

      uint zero[4] = { 0u, 0u, 0u, 0u };
      uint k[4];

      aes128_encrypt_vk (ks, zero, k);

      make_kn (k[0], k[1], k[2], k[3]);

      if (eapol_left < 16)
      {
        make_kn (k[0], k[1], k[2], k[3]);
      }

      m[0] ^= k[0]; m[1] ^= k[1]; m[2] ^= k[2]; m[3] ^= k[3];
      m[0] ^= iv[0]; m[1] ^= iv[1]; m[2] ^= iv[2]; m[3] ^= iv[3];

      uint keymic[4];

      aes128_encrypt_vk (ks, m, keymic);

      keymic[0] = bswap32 (keymic[0]); keymic[1] = bswap32 (keymic[1]);
      keymic[2] = bswap32 (keymic[2]); keymic[3] = bswap32 (keymic[3]);

      if ((keymic[0] == wpa_get (wpa_base, WPA_KEYMIC + 0u))
       && (keymic[1] == wpa_get (wpa_base, WPA_KEYMIC + 1u))
       && (keymic[2] == wpa_get (wpa_base, WPA_KEYMIC + 2u))
       && (keymic[3] == wpa_get (wpa_base, WPA_KEYMIC + 3u)))
      {
        if (claim_hash (digest_cur))
        {
          mark_hash_vk (gid, digest_pos, digest_cur);
        }
      }
    }
  }
}
