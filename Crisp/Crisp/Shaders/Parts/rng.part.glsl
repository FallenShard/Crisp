#ifndef RNG_PART_GLSL
#define RNG_PART_GLSL

// From https://redirect.cs.umbc.edu/~olano/papers/GPUTEA.pdf.
uint tea(uint val0, uint val1)
{
  uint v0 = val0;
  uint v1 = val1;
  uint s0 = 0;

  for(uint n = 0; n < 16; n++)
  {
    s0 += 0x9e3779b9;
    v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
    v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
  }

  return v0;
}

uint rndUint(inout uint seed)
{
    return (seed = 1664525 * seed + 1013904223);
}

uint xorshift32(inout uint state)
{
	/* Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" */
	uint x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
    state = x;
	return x;
}

float rndFloat(inout uint seed)
{
    const uint one = 0x3f800000;
    const uint msk = 0x007fffff;
    return uintBitsToFloat(one | (msk & (xorshift32(seed) >> 9))) - 1;
}

// float rndFloat(inout uint state)
// {
//     const uint one = 0x3f800000;
//     const uint msk = 0x007fffff;
//     return uintBitsToFloat(one | (msk & (xorshift32(state) >> 9))) - 1.0f;
// }



// float rndFloat(inout uint state)
// {
//     uint s = xorshift32(state);
//     return uintBitsToFloat(s >> 9) - 1.0f;
// }

#endif