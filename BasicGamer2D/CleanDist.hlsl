#include "GamerCommon.hlsli"

RWStructuredBuffer<int> mark : register(u0);
RWStructuredBuffer<float> dist : register(u1);

[numthreads(ROW / 2, 1, 1)]
void main(uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID) {
  dist[blockIdx.x * ROW + threadIdx.x] = mark[blockIdx.x * ROW + threadIdx.x] ? 0.f : INFINITY_DISTANCE;
  dist[blockIdx.x * ROW + threadIdx.x + ROW / 2] = mark[blockIdx.x * ROW + threadIdx.x + ROW / 2] ? 0.f : INFINITY_DISTANCE;
}
