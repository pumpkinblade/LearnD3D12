//// Raw Buffer
//Buffer<float3> gInput : register(t0);
//RWBuffer<float> gOutput : register(u0);
//[numthreads(32, 1, 1)]
//void CS(int3 dtid : SV_DispatchThreadID)
//{
//	//float3 v = float3(gInput[3*dtid.x + 0], gInput[3*dtid.x + 1], gInput[3*dtid.x + 2]);
//	gOutput[dtid.x] = length(gInput[dtid.x]);
//	//gOutput[dtid.x] = length(v);
//	//gOutput[dtid.x] = 3.14f;
//}

// Simple
StructuredBuffer<float3> gInput : register(t0);
RWStructuredBuffer<float> gOutput : register(u0);
[numthreads(32, 1, 1)]
void main(int dtid : SV_DispatchThreadID)
{
	gOutput[dtid.x] = length(gInput[dtid.x]);
}
