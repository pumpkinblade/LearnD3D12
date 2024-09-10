Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// 根据RGB计算出亮度
float CalcLuminance(float3 color)
{
	return dot(color, float3(0.299f, 0.578f, 0.114f));
}

[numthreads(16, 16, 1)]
void SobelCS(uint3 dtid : SV_DispatchThreadID)
{
	// 采集与当前欲处理像素相邻的众像素
	float4 c[3][3];
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			int2 xy = dtid.xy + int2(-1 + j, -1 + i);
			c[i][j] = gInput[xy];
		}
	}

	// 针对每个颜色通道，运用索贝尔公式估算出偏导数的近似值
	float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
	float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][2] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];

	// 计算每个分量的梯度的长度
	float4 mag = sqrt(Gx * Gx + Gy * Gy);

	// 将梯度陡峭的地方绘制为黑色，梯度平坦的地方绘制为白色
	mag = 1.0f - saturate(CalcLuminance(mag.rgb));

	gOutput[dtid.xy] = mag;
}