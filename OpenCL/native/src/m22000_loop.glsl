//
// m22000_loop: PBKDF2-HMAC-SHA1 iterations for WPA
//
// The two PBKDF2 counter chains are independent, so their SHA1 rounds run
// interleaved two-by-two through sha1_block_dual () for twice the
// instruction-level parallelism.
//
// bindings: 1 = tmps, 3 = kernel_param
//

#define TMP_STRIDE 30u

void main()
{
  const uint gid = gl_GlobalInvocationID.x;

  if (!kp_gid_valid (gid)) return;

  const uint base = gid * TMP_STRIDE;

  // hmac midstates (identical for both chains)

  uint i0 = raw1[base + 0u];
  uint i1 = raw1[base + 1u];
  uint i2 = raw1[base + 2u];
  uint i3 = raw1[base + 3u];
  uint i4 = raw1[base + 4u];

  uint o0 = raw1[base + 5u];
  uint o1 = raw1[base + 6u];
  uint o2 = raw1[base + 7u];
  uint o3 = raw1[base + 8u];
  uint o4 = raw1[base + 9u];

  // chain A: dgst[0..4] / out[0..4]
  // chain B: dgst[5..9] / out[5..9]

  uint da0 = raw1[base + 10u];
  uint da1 = raw1[base + 11u];
  uint da2 = raw1[base + 12u];
  uint da3 = raw1[base + 13u];
  uint da4 = raw1[base + 14u];

  uint db0 = raw1[base + 15u];
  uint db1 = raw1[base + 16u];
  uint db2 = raw1[base + 17u];
  uint db3 = raw1[base + 18u];
  uint db4 = raw1[base + 19u];

  uint xa0 = raw1[base + 20u];
  uint xa1 = raw1[base + 21u];
  uint xa2 = raw1[base + 22u];
  uint xa3 = raw1[base + 23u];
  uint xa4 = raw1[base + 24u];

  uint xb0 = raw1[base + 25u];
  uint xb1 = raw1[base + 26u];
  uint xb2 = raw1[base + 27u];
  uint xb3 = raw1[base + 28u];
  uint xb4 = raw1[base + 29u];

  const uint loops = KP_LOOP_CNT;

  for (uint j = 0u; j < loops; j++)
  {
    // message words: chain A digest and chain B digest, padded

    uint wa[16];
    uint wb[16];

    wa[0] = da0; wa[1] = da1; wa[2] = da2; wa[3] = da3; wa[4] = da4;
    wb[0] = db0; wb[1] = db1; wb[2] = db2; wb[3] = db3; wb[4] = db4;

    wa[5] = 0x80000000u; wb[5] = 0x80000000u;

    for (int i = 6; i < 15; i++) { wa[i] = 0u; wb[i] = 0u; }

    wa[15] = (64 + 20) * 8;
    wb[15] = (64 + 20) * 8;

    // inner blocks share the ipad midstate

    uint ia0 = i0, ia1 = i1, ia2 = i2, ia3 = i3, ia4 = i4;
    uint ib0 = i0, ib1 = i1, ib2 = i2, ib3 = i3, ib4 = i4;

    sha1_block_dual (ia0, ia1, ia2, ia3, ia4,
                     ib0, ib1, ib2, ib3, ib4,
                     wa, wb);

    // outer blocks share the opad midstate

    uint oa0 = o0, oa1 = o1, oa2 = o2, oa3 = o3, oa4 = o4;
    uint ob0 = o0, ob1 = o1, ob2 = o2, ob3 = o3, ob4 = o4;

    // messages: inner digests

    wa[0] = ia0; wa[1] = ia1; wa[2] = ia2; wa[3] = ia3; wa[4] = ia4;
    wb[0] = ib0; wb[1] = ib1; wb[2] = ib2; wb[3] = ib3; wb[4] = ib4;

    wa[5] = 0x80000000u; wb[5] = 0x80000000u;

    for (int i = 6; i < 15; i++) { wa[i] = 0u; wb[i] = 0u; }

    wa[15] = (64 + 20) * 8;
    wb[15] = (64 + 20) * 8;

    sha1_block_dual (oa0, oa1, oa2, oa3, oa4,
                     ob0, ob1, ob2, ob3, ob4,
                     wa, wb);

    da0 = oa0; da1 = oa1; da2 = oa2; da3 = oa3; da4 = oa4;
    db0 = ob0; db1 = ob1; db2 = ob2; db3 = ob3; db4 = ob4;

    xa0 ^= oa0; xa1 ^= oa1; xa2 ^= oa2; xa3 ^= oa3; xa4 ^= oa4;
    xb0 ^= ob0; xb1 ^= ob1; xb2 ^= ob2; xb3 ^= ob3; xb4 ^= ob4;
  }

  raw1[base + 10u] = da0; raw1[base + 11u] = da1; raw1[base + 12u] = da2;
  raw1[base + 13u] = da3; raw1[base + 14u] = da4;

  raw1[base + 15u] = db0; raw1[base + 16u] = db1; raw1[base + 17u] = db2;
  raw1[base + 18u] = db3; raw1[base + 19u] = db4;

  raw1[base + 20u] = xa0; raw1[base + 21u] = xa1; raw1[base + 22u] = xa2;
  raw1[base + 23u] = xa3; raw1[base + 24u] = xa4;

  raw1[base + 25u] = xb0; raw1[base + 26u] = xb1; raw1[base + 27u] = xb2;
  raw1[base + 28u] = xb3; raw1[base + 29u] = xb4;
}
