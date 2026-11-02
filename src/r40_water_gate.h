/* Conservative atlas source test. Include the preceding slots because the
 * original reference UV mode can reach a right-edge neighboring atlas cell. */
static int r40_texture_may_water(int texture){unsigned v=(unsigned)texture-16u;return v<=8u&&((0x183u>>v)&1u);}
