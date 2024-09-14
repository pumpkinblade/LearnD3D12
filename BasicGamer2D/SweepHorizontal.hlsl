#include "GamerCommon.hlsli"

cbuffer CB : register(b0) {
  int turn;
};
RWStructuredBuffer<float> costHorizontal : register(u0);
RWStructuredBuffer<float> dist : register(u1);
RWStructuredBuffer<int> allPrev : register(u2);

groupshared float cL[ROW];
groupshared float cR[ROW];
groupshared float dL[ROW];
groupshared float dR[ROW];
groupshared int pL[ROW];
groupshared int pR[ROW];

[numthreads(ROW / 2, 1, 1)]
void main(uint3 threadIdx : SV_GroupThreadID, uint3 blockIdx : SV_GroupID) {
  uint rowId = blockIdx.x;
  uint cur;
  for (cur = threadIdx.x; cur < ROW; cur += ROW / 2) {
    cL[cur] = cur == 0 ? 0.f : costHorizontal[rowId * ROW + cur];
    cR[cur] = cur == 0 ? 0.f : costHorizontal[rowId * ROW + ROW - cur];
    dL[cur] = dist[rowId * ROW + cur];
    dR[cur] = dist[rowId * ROW + ROW - 1 - cur];
    pL[cur] = cur;
    pR[cur] = ROW - 1 - cur;
  }
  GroupMemoryBarrierWithGroupSync();

  for (uint d = 0; (1 << d) < ROW; d++) {
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
  
  for (cur = threadIdx.x; cur < ROW; cur += ROW / 2) {
    allPrev[turn * ROW * COL + rowId * ROW + cur] = rowId * ROW + cur;
    if (dL[cur] < dist[rowId * ROW + cur]) {
      dist[rowId * ROW + cur] = dL[cur];
      allPrev[turn * ROW * COL + rowId * ROW + cur] = rowId * ROW + pL[cur];
    }
    if (dR[ROW - 1 - cur] < dist[rowId * ROW + cur]) {
      dist[rowId * ROW + cur] = dR[ROW - 1 - cur];
      allPrev[turn * ROW * COL + rowId * ROW + cur] = rowId * ROW + pR[ROW - 1 - cur];
    }
  }
}