#define LENGTH 1024

RWStructuredBuffer<int> xs : register(u0);

groupshared int workplace[LENGTH];

[numthreads(LENGTH / 2, 1, 1)]
void main(uint3 gtid : SV_GroupThreadID)
{
  for (uint i = gtid.x; i < LENGTH; i += LENGTH / 2)
    workplace[i] = xs[i];
  GroupMemoryBarrierWithGroupSync();
  
  for (uint d = 0; (1 << d) < LENGTH; d++)
  {
    for (uint offset = (1 << d), dircnt = 1; offset > 0; offset >>= 1, dircnt <<= 1)
    {
      uint lo = gtid.x + (gtid.x / offset) * offset;
      uint hi = lo + offset;
      uint dir = gtid.x / offset / dircnt;
      if ((dir & 1) ? (workplace[lo] < workplace[hi]) : (workplace[lo] > workplace[hi]))
      {
        int tmp = workplace[lo];
        workplace[lo] = workplace[hi];
        workplace[hi] = tmp;
      }
      GroupMemoryBarrierWithGroupSync();
    }
  }

  for (uint i = gtid.x; i < LENGTH; i += LENGTH / 2)
    xs[i] = workplace[i];
}