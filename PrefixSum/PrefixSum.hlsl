#ifndef VECTOR_LENGTH
#define VECTOR_LENGTH 16
#endif

RWStructuredBuffer<int> xs : register(u0);

groupshared int workplace[VECTOR_LENGTH];

[numthreads(VECTOR_LENGTH / 2, 1, 1)]
void main(uint3 threadIdx : SV_GroupThreadID) {
  [unroll]
  for (uint i = threadIdx.x; i < VECTOR_LENGTH; i += VECTOR_LENGTH / 2)
    workplace[i] = xs[i];
  GroupMemoryBarrierWithGroupSync();
 
  [unroll]
  for (uint d = 0; (1 << d) < VECTOR_LENGTH; d++) {
    uint dst = threadIdx.x >> d << (d + 1) | (1 << d) | (threadIdx.x & ((1 << d) - 1));
    uint src = (dst >> d << d) - 1;
    workplace[dst] += workplace[src];
    GroupMemoryBarrierWithGroupSync();
  }

  [unroll]
  for (uint i = threadIdx.x; i < VECTOR_LENGTH; i += VECTOR_LENGTH / 2)
    xs[i] = workplace[i];
}
