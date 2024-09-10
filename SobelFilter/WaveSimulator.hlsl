cbuffer cbSettings
{
	float gK1;
	float gK2;
	float gK3;

	float gDisturbMag;
	int2 gDisturbIndex;
};

RWTexture2D<float> gPrevSolInput : register(u0);
RWTexture2D<float> gCurrSolInput : register(u1);
RWTexture2D<float> gOutput       : register(u2);

[numthreads(16, 16, 1)]
void UpdateCS(int3 dtid : SV_DispatchThreadID)
{
	gOutput[dtid.xy] =
		gK1 * gPrevSolInput[dtid.xy].r +
		gK2 * gCurrSolInput[dtid.xy].r +
		gK3 * (gCurrSolInput[int2(dtid.x + 1, dtid.y)].r +
			gCurrSolInput[int2(dtid.x - 1, dtid.y)].r +
			gCurrSolInput[int2(dtid.x, dtid.y - 1)].r +
			gCurrSolInput[int2(dtid.x, dtid.y + 1)].r);
}

[numthreads(1, 1, 1)]
void DisturbCS()
{
	int x = gDisturbIndex.x;
	int y = gDisturbIndex.y;

	float halfMag = 0.5f * gDisturbMag;

	// Buffer is RW so operator += is well defined.
	gOutput[int2(x, y)] += gDisturbMag;
	gOutput[int2(x + 1, y)] += halfMag;
	gOutput[int2(x - 1, y)] += halfMag;
	gOutput[int2(x, y + 1)] += halfMag;
	gOutput[int2(x, y - 1)] += halfMag;
}