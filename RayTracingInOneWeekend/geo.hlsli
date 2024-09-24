#ifndef __GEO_HLSLI__
#define __GEO_HLSLI__

#define MAX_T 1e3f
#define MIN_T 1e-3f

struct Ray {
  float3 origin;
  float pad0;
  float3 direction;
  float pad1;
};

struct Sphere {
  float3 center;
  float radius;
};

struct HitRecord {
  float3 pos;
  int is_hit;
  float3 normal;
  float t;
};

HitRecord hit(Ray ray, Sphere sphere) {
  HitRecord rec;
  rec.pos = float3(0.f, 0.f, 0.f);
  rec.is_hit = false;
  rec.normal = float3(0.f, 0.f, 1.f);
  rec.t = MAX_T;
  
  float3 oc = ray.origin - sphere.center;
  float a = dot(ray.direction, ray.direction);
  float half_b = dot(oc, ray.direction);
  float c = dot(oc, oc) - sphere.radius * sphere.radius;

  float discriminant = half_b * half_b - a * c;
  if (discriminant < 0) {
    return rec;
  }
  float sqrtd = sqrt(discriminant);

  // Find the nearest root that lies in the acceptable range.
  float t = (-half_b - sqrtd) / a;
  if (t < MIN_T || MAX_T < t) {
    t = (-half_b + sqrtd) / a;
    if (t < MIN_T || MAX_T < t) {
      return rec;
    }
  }

  rec.pos = ray.origin + t * ray.direction;
  rec.is_hit = true;
  rec.normal = (rec.pos - sphere.center) / sphere.radius;
  rec.t = t;
  return rec;
}

#endif