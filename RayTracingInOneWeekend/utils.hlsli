#ifndef __UTILS_HLSLI__
#define __UTILS_HLSLI__

#define PI 3.1415926f

void xorShift(inout uint x) {
  x ^= (x << 13);
  x ^= (x >> 17);
  x ^= (x << 5);
}

float randomFloat(inout uint random) {
  float x = float(random) / float(uint(-1));
  xorShift(random);
  return x;
}

float2 randomDisk(inout uint random) {
  float theta = 2 * PI * randomFloat(random);
  float r = sqrt(randomFloat(random));
  return r * float2(cos(theta), sin(theta));
}

float3 randomSphere(inout uint random) {
  float xi1 = randomFloat(random);
  float xi2 = randomFloat(random);
  float z = 1.f - 2.f * xi1;
  float r = sqrt(1.f - z * z);
  float phi = 2 * PI * xi2;
  float x = r * cos(phi);
  float y = r * sin(phi);
  return float3(x, y, z);
}

float3 randomHemisphere(inout uint random) {
  float3 d = randomSphere(random);
  return float3(d.x, d.y, abs(d.z));
}

float3 toWorld(float3 r, float3 n) {
  float3 w = n;
  float3 v = normalize(abs(w.x) > abs(w.y) ? float3(w.z, 0.f, -w.x) : float3(0.f, w.z, -w.y));
  float3 u = cross(v, w);
  return r.x * u + r.y * v + r.z * w;
}

#endif