/** Version 1: one unsigned xorshift32 draw per frame; no ambient randomness. */
export function createRandom(seed: number): () => number {
  let state = seed === 0 ? 0x6d2b79f5 : seed >>> 0;
  return () => {
    state ^= state << 13;
    state ^= state >>> 17;
    state ^= state << 5;
    return (state >>> 0) / 4294967296;
  };
}
