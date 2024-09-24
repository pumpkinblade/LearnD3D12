cbuffer ImageCB : register(b0) {
  uint g_image_width;
  uint g_image_height;
  uint g_sample_idx;
  uint g_num_samples;
};

RWStructuredBuffer<float3> g_colors : register(u0);
RWStructuredBuffer<float3> g_pixels : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 dtid : SV_DispatchThreadID) {
  uint idx = dtid.y * g_image_width + dtid.x;
  float alpha = 1.f / float(1 + g_sample_idx);
  float beta = 1.f - alpha;
  g_pixels[idx] = alpha * g_colors[idx] + beta * g_pixels[idx];
}