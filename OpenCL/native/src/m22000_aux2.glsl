//
// m22000_aux2: WPA deep-comp kernel, PMKID + keyver 2 (HMAC-SHA1 eapol mic)
//
// bindings: 1 = tmps, 2 = esalt_bufs, 3 = kernel_param,
//           4 = plains_buf, 5 = hashes_shown, 6 = d_return_buf
//

#define TMP_STRIDE 30u

void main()
{
  const uint gid = gl_GlobalInvocationID.x;

  if (!kp_gid_valid (gid)) return;

  raw6[20] = 0x12345678u;
  raw6[21] = KP_GID_LO;
  raw6[22] = wpa_get (0, WPA_TYPE);

  const uint base = gid * TMP_STRIDE;

  const uint digest_pos = KP_LOOP_POS;
  const uint digest_cur = KP_DGST_OFF + digest_pos;

  const uint wpa_base = digest_cur * WPA_STRIDE;

  const uint type   = wpa_get (wpa_base, WPA_TYPE);
  const uint keyver = wpa_get (wpa_base, WPA_KEYVER);

  if ((type != 2u) && (keyver != 2u)) return;

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

      // kck = hmac-sha1 (key = tmps.out, msg = pke)

      uint key[16];

      key[0] = raw1[base + 20u]; key[1] = raw1[base + 21u];
      key[2] = raw1[base + 22u]; key[3] = raw1[base + 23u];
      key[4] = raw1[base + 24u]; key[5] = raw1[base + 25u];
      key[6] = raw1[base + 26u]; key[7] = raw1[base + 27u];

      for (int i = 8; i < 16; i++) key[i] = 0u;

      uint ist0, ist1, ist2, ist3, ist4;
      uint ost0, ost1, ost2, ost3, ost4;

      sha1_hmac_key64 (ist0, ist1, ist2, ist3, ist4, ost0, ost1, ost2, ost3, ost4, key);

      uint istate[5]; uint ostate[5];

      istate[0] = ist0; istate[1] = ist1; istate[2] = ist2; istate[3] = ist3; istate[4] = ist4;
      ostate[0] = ost0; ostate[1] = ost1; ostate[2] = ost2; ostate[3] = ost3; ostate[4] = ost4;

      uint msg[MSGSZ];

      for (int i = 0; i < MSGSZ; i++) msg[i] = 0u;

      for (int i = 0; i < 32; i++) msg[i] = pke[i];

      uint kck0, kck1, kck2, kck3, kck4;

      sha1_hmac_core (istate, ostate, msg, 100, kck0, kck1, kck2, kck3, kck4);

      // mic = hmac-sha1 (key = kck unswapped, msg = eapol)

      uint key2[16];

      // the MIC key is the KCK: first 16 bytes of the PTK only

      key2[0] = kck0; key2[1] = kck1; key2[2] = kck2; key2[3] = kck3;

      for (int i = 4; i < 16; i++) key2[i] = 0u;

      uint ist20, ist21, ist22, ist23, ist24;
      uint ost20, ost21, ost22, ost23, ost24;

      sha1_hmac_key64 (ist20, ist21, ist22, ist23, ist24, ost20, ost21, ost22, ost23, ost24, key2);

      uint istate2[5]; uint ostate2[5];

      istate2[0] = ist20; istate2[1] = ist21; istate2[2] = ist22; istate2[3] = ist23; istate2[4] = ist24;
      ostate2[0] = ost20; ostate2[1] = ost21; ostate2[2] = ost22; ostate2[3] = ost23; ostate2[4] = ost24;

      const int eapol_len = int(wpa_get (wpa_base, WPA_EAPOL_LEN));

      for (int i = 0; i < MSGSZ; i++) msg[i] = 0u;

      for (int i = 0; i < 80; i++) msg[i] = wpa_get (wpa_base, WPA_EAPOL + uint(i));

      uint mic0, mic1, mic2, mic3, mic4;

      sha1_hmac_core (istate2, ostate2, msg, eapol_len, mic0, mic1, mic2, mic3, mic4);

      if (gid == 0u)
      {
        raw1[4096u] = mic0;
        raw1[4097u] = kck0;
        raw1[4098u] = digest_cur;
        raw1[4099u] = wpa_get (wpa_base, WPA_KEYMIC);
      }

      if (gid == 0u)
      {
        uint db = 4096u + gid * 8u;
        raw1[db + 0u] = mic0;
        raw1[db + 1u] = wpa_get (wpa_base, WPA_KEYMIC);
        raw1[db + 2u] = digest_cur;
        raw1[db + 3u] = KP_LOOP_POS;
      }

      if ((mic0 == wpa_get (wpa_base, WPA_KEYMIC + 0u))
       && (mic1 == wpa_get (wpa_base, WPA_KEYMIC + 1u))
       && (mic2 == wpa_get (wpa_base, WPA_KEYMIC + 2u))
       && (mic3 == wpa_get (wpa_base, WPA_KEYMIC + 3u)))
      {
        if (claim_hash (digest_cur))
        {
          mark_hash_vk (gid, digest_pos, digest_cur);
        }
      }
    }
  }
}
