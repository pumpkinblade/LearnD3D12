#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__

#include "utils.hlsli"

#define MATERIAL_LAMBERTIAN 0
#define MATERIAL_METAL 1
#define MATERIAL_GLASS 2

struct Material {
  int type;
  float ir;
  float fuzz;
  uint pad0;
  float3 color;
  uint pad1;
};

float3 attenuation(Material mat) {
  return mat.color;
}

inline float reflectance(float cosine, float ref_idx) {
  // Use Schlick's approximation for reflectance.
  float r0 = (1.f - ref_idx) / (1.f + ref_idx);
  r0 = r0 * r0;
  return r0 + (1.f - r0) * pow((1.f - cosine), 5.f);
}

float3 scatter(Material mat, float3 r, float3 n, inout uint random) {
  switch (mat.type) {
    case MATERIAL_METAL:
      return normalize(reflect(r, n) +
                       mat.fuzz * toWorld(randomHemisphere(random), n));
      break;
    case MATERIAL_GLASS:{
        bool front_face = dot(r, n) < 0;
        n = front_face ? n : -n;
        float refraction_ratio = front_face ? (1.f / mat.ir) : mat.ir;
        float cos_theta = min(dot(-r, n), 1.f);
        float sin_theta = sqrt(1.f - cos_theta * cos_theta);
        bool cannot_refract = refraction_ratio * sin_theta > 1.f;
        if (cannot_refract ||
            reflectance(cos_theta, refraction_ratio) > randomFloat(random)) {
          return reflect(r, n);
        } else {
          return refract(r, n, refraction_ratio);
        }
      }
      break;
    case MATERIAL_LAMBERTIAN:
    default:
      return toWorld(randomHemisphere(random), n);
      break;
  }
}

#endif