#include "GamerCommon.hlsli"

cbuffer CB : register(b0) {
  int numPins;
};
RWStructuredBuffer<int> pinIndices : register(u0);
RWStructuredBuffer<int> mark : register(u1);
RWStructuredBuffer<int> isRoutedPin : register(u2);
RWStructuredBuffer<int> routes : register(u3);

[numthreads(1, 1, 1)]
void main() {
  for (int i = 0; i < numPins; i++)
    isRoutedPin[i] = 0;
  isRoutedPin[0] = 1;
  mark[pinIndices[0]] = 1;
  routes[0] = 0;
}