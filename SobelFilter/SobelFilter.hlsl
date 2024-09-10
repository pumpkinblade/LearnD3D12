Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);

// ����RGB���������
float CalcLuminance(float3 color)
{
	return dot(color, float3(0.299f, 0.578f, 0.114f));
}

[numthreads(16, 16, 1)]
void SobelCS(uint3 dtid : SV_DispatchThreadID)
{
	// �ɼ��뵱ǰ�������������ڵ�������
	float4 c[3][3];
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			int2 xy = dtid.xy + int2(-1 + j, -1 + i);
			c[i][j] = gInput[xy];
		}
	}

	// ���ÿ����ɫͨ����������������ʽ�����ƫ�����Ľ���ֵ
	float4 Gx = -1.0f * c[0][0] - 2.0f * c[1][0] - 1.0f * c[2][0] + 1.0f * c[0][2] + 2.0f * c[1][2] + 1.0f * c[2][2];
	float4 Gy = -1.0f * c[2][0] - 2.0f * c[2][1] - 1.0f * c[2][2] + 1.0f * c[0][0] + 2.0f * c[0][1] + 1.0f * c[0][2];

	// ����ÿ���������ݶȵĳ���
	float4 mag = sqrt(Gx * Gx + Gy * Gy);

	// ���ݶȶ��͵ĵط�����Ϊ��ɫ���ݶ�ƽ̹�ĵط�����Ϊ��ɫ
	mag = 1.0f - saturate(CalcLuminance(mag.rgb));

	gOutput[dtid.xy] = mag;
}