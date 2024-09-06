#ifndef THREAD_NUM
#define THREAD_NUM 16
#endif

StructuredBuffer<float> xs : register(t0);
StructuredBuffer<float> ys : register(t1);
RWStructuredBuffer<float> zs: register(u0);
cbuffer ConstantBuffer : register(b0)
{
  float alpha;
  uint n;
}

[numthreads(THREAD_NUM, 1, 1)]
void main(uint3 dtid : SV_DispatchThreadID)
{
  if (dtid.x < n)
    zs[dtid.x] = alpha * xs[dtid.x] + ys[dtid.x];
}
