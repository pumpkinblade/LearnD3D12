#include "geo.hlsli"
#include "material.hlsli"

#define MAX_NUM_OBJS 128

cbuffer ImageCB : register(b0) {
  uint g_image_width;
  uint g_image_height;
  uint g_sample_idx;
  uint g_num_samples;
};

cbuffer ObjectCB : register(b1) {
  uint g_num_objs;
  Sphere g_spheres[MAX_NUM_OBJS];
  Material g_materials[MAX_NUM_OBJS];
};

RWStructuredBuffer<float3> g_colors : register(u0);
RWStructuredBuffer<Ray> g_rays : register(u1);
RWStructuredBuffer<bool> g_valids : register(u2);
RWStructuredBuffer<uint> g_randoms : register(u3);

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
  uint idx = dtid.y * g_image_width + dtid.x;
  if (!g_valids[idx]) {
    g_colors[idx] = float3(1.f, 1.f, 1.f);
    return;
  }
  
  HitRecord rec;
  uint hit_obj_id = g_num_objs;
  Ray ray = g_rays[idx];
  for (uint obj_id = 0; obj_id < g_num_objs; obj_id++) {
    HitRecord rec1 = hit(ray, g_spheres[obj_id]);
    if ((rec1.is_hit && !rec.is_hit) ||
          (rec1.is_hit && rec.is_hit && rec1.t < rec.t)) {
      rec = rec1;
      hit_obj_id = obj_id;
    }
  }
  
  if (hit_obj_id < g_num_objs) {
    Material mat = g_materials[hit_obj_id];
    uint random = g_randoms[idx];
    Ray new_ray;
    new_ray.pad0 = new_ray.pad1 = 0;
    new_ray.origin = rec.pos;
    new_ray.direction = scatter(mat, ray.direction, rec.normal, random);
    g_colors[idx] = attenuation(mat);
    g_rays[idx] = new_ray;
    g_valids[idx] = true;
    g_randoms[idx] = random;
  } else {
    float t = 0.5f * (ray.direction.y + 1.f);
    g_colors[idx] =
           (1.f - t) * float3(1.f, 1.f, 1.f) + t * float3(0.5f, 0.7f, 1.f);
    g_valids[idx] = false;
  }
}