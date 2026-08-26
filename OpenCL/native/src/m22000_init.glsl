#define TMP_STRIDE 30u

#define TMP_IPAD(o) raw1[(gid * TMP_STRIDE) + (o)]
#define TMP_OPAD(o) raw1[(gid * TMP_STRIDE) + 5u + (o)]
#define TMP_DGST(o) raw1[(gid * TMP_STRIDE) + 10u + (o)]
#define TMP_OUT(o)  raw1[(gid * TMP_STRIDE) + 20u + (o)]

#define WPA_ESSID_BUF    0u
#define WPA_ESSID_LEN   16u
#define WPA_STRIDE_E   175u

// assemble message words from essid bytes followed by a big-endian u32
// literal (the PBKDF2 INT(i) counter); returns the total message length

int build_msg_essid_counter (out uint msg[MSGSZ], const uint ew[16], const uint essid_len, const uint counter_be)
{
  const int total = int(essid_len) + 4;
  const int nwt = (total + 3) / 4;

  for (int k = 0; k < nwt; k++)
  {
    uint v = 0u;

    for (int t = 0; t < 4; t++)
    {
      const int bytepos = k * 4 + t;

      uint b;

      if (bytepos < int(essid_len))
      {
        b = be_byte (ew, bytepos);
      }
      else
      {
        b = (counter_be >> (8 * (3 - (bytepos - int(essid_len))))) & 0xffu;
      }

      v |= b << (24 - 8 * t);
    }

    msg[k] = v;
  }

  return total;
}

void main()
{
  const uint gid = gl_GlobalInvocationID.x;

  if (!kp_gid_valid (gid)) return;

  // password -> swapped big-endian stream

  const uint pw_base = gid * 65u;

  const uint pw_len = raw0[pw_base + 64u];

  uint ws[MSGSZ];

  for (int i = 0; i < 64; i++) ws[i] = bswap32 (raw0[pw_base + uint(i)]);

  for (int i = 64; i < MSGSZ; i++) ws[i] = 0u;

  // hmac key setup

  uint key[16];

  if (pw_len <= 64u)
  {
    for (int i = 0; i < 16; i++) key[i] = 0u;

    const int nw = int(pw_len) / 4;
    const int rm = int(pw_len) - nw * 4;

    for (int i = 0; i < nw; i++) key[i] = ws[i];

    if (rm != 0)
    {
      uint m = 0u;

      if (rm >= 1) m |= 0xff000000u;
      if (rm >= 2) m |= 0x00ff0000u;
      if (rm >= 3) m |= 0x0000ff00u;

      key[nw] = ws[nw] & m;
    }
  }
  else
  {
    // long keys are reduced with a plain sha1 over the swapped stream
    uint s0, s1, s2, s3, s4;

    s0 = SHA1_INIT0; s1 = SHA1_INIT1; s2 = SHA1_INIT2; s3 = SHA1_INIT3; s4 = SHA1_INIT4;

    sha1 (s0, s1, s2, s3, s4, ws, int(pw_len), 0u);

    key[0] = s0; key[1] = s1; key[2] = s2; key[3] = s3; key[4] = s4;
    for (int i = 5; i < 16; i++) key[i] = 0u;
  }

  uint ist0, ist1, ist2, ist3, ist4;
  uint ost0, ost1, ost2, ost3, ost4;

  sha1_hmac_key64 (ist0, ist1, ist2, ist3, ist4, ost0, ost1, ost2, ost3, ost4, key);

  TMP_IPAD(0) = ist0; TMP_IPAD(1) = ist1; TMP_IPAD(2) = ist2; TMP_IPAD(3) = ist3; TMP_IPAD(4) = ist4;
  TMP_OPAD(0) = ost0; TMP_OPAD(1) = ost1; TMP_OPAD(2) = ost2; TMP_OPAD(3) = ost3; TMP_OPAD(4) = ost4;

  // message: swapped essid || INT(counter)

  const uint esalt_base = KP_DGST_OFF * WPA_STRIDE_E;

  const uint essid_len = raw2[esalt_base + WPA_ESSID_LEN];

  uint ew[16];

  for (int i = 0; i < 16; i++) ew[i] = bswap32 (raw2[esalt_base + WPA_ESSID_BUF + uint(i)]);

  uint istate[5]; uint ostate[5];

  istate[0] = ist0; istate[1] = ist1; istate[2] = ist2; istate[3] = ist3; istate[4] = ist4;
  ostate[0] = ost0; ostate[1] = ost1; ostate[2] = ost2; ostate[3] = ost3; ostate[4] = ost4;

  uint msg[MSGSZ];

  // block A: INT(1)

  int total = build_msg_essid_counter (msg, ew, essid_len, 0x00000001u);

  uint d0, d1, d2, d3, d4;

  sha1_hmac_core (istate, ostate, msg, total, d0, d1, d2, d3, d4);

  TMP_DGST(0) = d0; TMP_DGST(1) = d1; TMP_DGST(2) = d2; TMP_DGST(3) = d3; TMP_DGST(4) = d4;


  TMP_OUT(0)  = d0; TMP_OUT(1)  = d1; TMP_OUT(2)  = d2; TMP_OUT(3)  = d3; TMP_OUT(4)  = d4;

  // block B: INT(2)

  total = build_msg_essid_counter (msg, ew, essid_len, 0x00000002u);

  sha1_hmac_core (istate, ostate, msg, total, d0, d1, d2, d3, d4);

  TMP_DGST(5) = d0; TMP_DGST(6) = d1; TMP_DGST(7) = d2; TMP_DGST(8) = d3; TMP_DGST(9) = d4;
  TMP_OUT(5)  = d0; TMP_OUT(6)  = d1; TMP_OUT(7)  = d2; TMP_OUT(8)  = d3; TMP_OUT(9)  = d4;
}
