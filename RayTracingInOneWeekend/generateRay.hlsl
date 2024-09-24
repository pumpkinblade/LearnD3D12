#include "geo.hlsli"
#include "utils.hlsli"

cbuffer ImageCB : register(b0) {
  uint g_image_width;
  uint g_image_height;
  uint g_sample_idx;
  uint g_num_samples;
};

cbuffer CameraCB : register(b1) {
  float3 g_camera_origin;
  uint g_camear_pad0;
  float3 g_camera_lower_left_corner;
  uint g_camear_pad1;
  float3 g_camera_horizontal;
  uint g_camear_pad2;
  float3 g_camera_vertical;
  uint g_camear_pad3;
  float3 g_camera_u;
  uint g_camear_pad4;
  float3 g_camera_v;
  uint g_camear_pad5;
  float3 g_camera_w;
  float g_camera_lens_radius;
};

RWStructuredBuffer<Ray> g_rays : register(u0);
RWStructuredBuffer<uint> g_valids : register(u1);
RWStructuredBuffer<uint> g_randoms : register(u2);

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
  uint idx = dtid.y * g_image_width + dtid.x;
  float s = float(dtid.x) / float(g_image_width);
  float t = float(dtid.y) / float(g_image_height);
  uint random = g_randoms[idx];
  Ray ray;
  float2 rd = g_camera_lens_radius * randomDisk(random);
  float3 offset = g_camera_u * rd.x + g_camera_v * rd.y;
  ray.pad0 = ray.pad1 = 0;
  ray.origin = g_camera_origin + offset;
  ray.direction = normalize(g_camera_lower_left_corner + 
                            s * g_camera_horizontal +
                            t * g_camera_vertical - 
                            ray.origin);
  g_rays[idx] = ray;
  g_valids[idx] = true;
  g_randoms[idx] = random;
}