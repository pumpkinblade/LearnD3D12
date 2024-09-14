#include "GamerCommon.hlsli"

cbuffer CB : register(b0) {
  int turn;
};
RWStructuredBuffer<float> costVertical : register(u0);
RWStructuredBuffer<float> dist : register(u1);
RWStructuredBuffer<int> allPrev : register(u2);

groupshared float cL[ROW];
groupshared float cR[ROW];
groupshared float dL[ROW];
groupshared float dR[ROW];
groupshared int pL[ROW];
groupshared int pR[ROW];

[numthreads(COL / 2, 1, 1)]
void main(uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID) {
  uint cur;
  uint colId = blockIdx.x;
  for (cur = threadIdx.x; cur < COL; cur += COL / 2) {
    cL[cur] = cur == 0 ? 0.f : costVertical[cur * ROW + colId];
    cR[cur] = cur == 0 ? 0.f : costVertical[(COL - cur) * ROW + colId];
    dL[cur] = dist[cur * ROW + colId];
    dR[cur] = dist[(COL - 1 - cur) * ROW + colId];
    pL[cur] = cur;
    pR[cur] = ROW - 1 - cur;
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint d = 0; (1 << d) < COL; d++) {
    uint dst = (threadIdx.x >> d << (d + 1) | (1 << d)) | (threadIdx.x & ((1 << d) - 1));
    uint src = (dst >> d << d) - 1;
    if (dL[dst] > dL[src] + cL[dst]) {
      dL[dst] = dL[src] + cL[dst];
      pL[dst] = pL[src];
    }
    if (dR[dst] > dR[src] + cR[dst]) {
      dR[dst] = dR[src] + cR[dst];
      pR[dst] = pR[src];
    }
    cL[dst] += cL[src];
    cR[dst] += cR[src];
    GroupMemoryBarrierWithGroupSync();
  }
  
  for (cur = threadIdx.x; cur < COL; cur += COL / 2) {
    allPrev[turn * ROW * COL + cur * ROW + colId] = cur * ROW + colId;
    if (dL[cur] < dist[cur * ROW + colId]) {
      dist[cur * ROW + colId] = dL[cur];
      allPrev[turn * ROW * COL + cur * ROW + colId] = pL[cur] * ROW + colId;
    }
    if (dR[COL - 1 - cur] < dist[cur * ROW + colId]) {
      dist[cur * ROW + colId] = dR[COL - 1 - cur];
      allPrev[turn * ROW * COL + cur * ROW + colId] = pR[COL - 1 - cur] * ROW + colId;
    }
  }
}