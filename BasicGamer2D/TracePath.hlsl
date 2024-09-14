#include "GamerCommon.hlsli"

cbuffer CB : register(b0) {
  int numPins;
  int numTurns;
};
RWStructuredBuffer<float> dist : register(u0);
RWStructuredBuffer<int> allPrev : register(u1);
RWStructuredBuffer<int> pinIndices : register(u2);
RWStructuredBuffer<int> mark : register(u3);
RWStructuredBuffer<int> isRoutedPin : register(u4);
RWStructuredBuffer<int> routes : register(u5);

[numthreads(1, 1, 1)]
void main() {
  float minDist = INFINITY_DISTANCE;
  int pinId = -1, idx = -1;

  // fine the closest un-routed pin
  for (int i = 0; i < numPins; i++) {
    if (!isRoutedPin[i]) {
      int p = pinIndices[i];
      if (dist[p] < minDist) {
        minDist = dist[p];
        idx = p;
        pinId = i;
      }
    }
  }
  if (pinId == -1)
    return;
  isRoutedPin[pinId] = 1;

  // backtracing
  for (int t = numTurns - 1; t >= 0; t--) {
    int prevIdx = allPrev[t * ROW * COL + idx];
    if (prevIdx == idx)
      continue;
    int startIdx = min(idx, prevIdx);
    int endIdx = max(idx, prevIdx);
    routes[++routes[0]] = startIdx;
    routes[++routes[0]] = endIdx;
    if ((uint) idx / ROW == (uint) prevIdx / ROW) { // horizontal
      for (int tmpIdx = startIdx; tmpIdx <= endIdx; tmpIdx++)
        mark[tmpIdx] = 1;
    } else { // vertical
      for (int tmpIdx = startIdx; tmpIdx <= endIdx; tmpIdx += ROW)
        mark[tmpIdx] = 1;
    }
    idx = prevIdx;
  }
}