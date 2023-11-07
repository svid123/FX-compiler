typedef int T;

struct SS
{
	

	T a;
}TStr;


typedef float3 TArrayFloats0[2];
typedef TArrayFloats0 TArrayFloats[3];
typedef volatile float3 vec3;




TArrayFloats arrr={{vec3(1+1,0,0),float3(1,0,0)},
				{float3(0,1,0),float3(1,1,0)},
				{float3(0,2,0),float3(1,2,9)}};



typedef int TTT,ZZ[2];
#define def(a,...) a=__VA_ARGS__


static const float F[2] = {10, 20};
float4 vec=int4(1,0.3,1,1),vec1,vec2=0;




[root_param]
cbuffer bbb
{
	double3x3 mtx=float3x3(1,1,1,
					2,2,2,
					3,3,3);
}

unsigned int aaaa;

#define assert(a,b)	if (a) b

#define AA 2
#define BB(x) x
#define TEXCOORD_(num) TEXCOORD##num
#define TEXCOORD__(num) TEXCOORD_(num)

enum DEFERRED_MATERIAL_DATA	//Stored in alpha channel
{
	DMD_MAT_MODEL=0,	//Model render
	DMD_MAT_GRASS,		//Grass render, No reflections
	DMD_MAT_TERRAIN,	//Terrain (Reflections are enabled only on non-rough surface)
	DMD_MAT_MASK,

	DMD_FLAG_SHADOWS=		4,	//Recieve shadows
	DMD_FLAG_SHADOWS_CLOSE=	8,	//Recieve shadows from cockpit depth map
	DMD_FLAG_FORESTCONTRAST=16,	//Low contrast lighting (from forest settings)
	DMD_FLAG_CUBEIBL=		32,
	DMD_FLAG_COCKPITREF=	64,

	DMD_REGULAR_TERRAIN=(DMD_MAT_TERRAIN | DMD_FLAG_SHADOWS),
	DMD_REGULAR_GRASS=(DMD_MAT_GRASS | DMD_FLAG_SHADOWS),
	DMD_REGULAR_MODEL=(DMD_MAT_MODEL | DMD_FLAG_SHADOWS | /*DMD_FLAG_REFLECTIONS | */DMD_FLAG_CUBEIBL),
	DMD_REGULAR_COCKPIT=(DMD_MAT_MODEL | DMD_FLAG_SHADOWS | DMD_FLAG_COCKPITREF | DMD_FLAG_CUBEIBL),
};

sampler LerpSampler
{
	Filter=MIN_MAG_MIP_LINEAR;

	AddressU = CLAMP;
    AddressV = CLAMP;

	BorderColor=0xff;
}

sampler aSamplers[3][2]={
							{
								LerpSampler,LerpSampler
							},
							{								
								sampler {
									Filter=MIN_MAG_MIP_LINEAR;

									AddressU = CLAMP;
									AddressV = CLAMP;
								},
								sampler {
									Filter=MIN_MAG_MIP_POINT;

									AddressU = CLAMP;
									AddressV = CLAMP;
								}								
							}
						};




int TEXCOORD__(BB(1));

Texture2D g_tDiffTexture[2];

float4 g_vColor,g_vTexTransform;
float4 g_vTransform;
float4 g_vSettings;	//bBufferEnabled,fGamma,0,0
Buffer<float4> g_bufColor;

#include "CommStates.inc"



#include "CommStates.inc"
//Buffer<float4> g_Buf;

DEFERRED_MATERIAL_DATA testvar;

struct VS_IN
{
	float2 vPos:POSITION0;
};


struct VS_OUT
{
	float4 vPos : POSITION;
	float2 vT0 : TEXCOORD0;
	float4 vColor:COLOR0;
};



groupshared float4 arr[16*10];



unsigned int a(const line VS_OUT a[3]):COLOR0
{
	[unroll]
	for (int n=0;n<2;++n)	//due to summetrical shape only 2 samples needed (instead of 4)
	{		
	}
	return (unsigned int)10;
}

float fn(uniform int a)
{
	return pow(a,2);
}

void a()
{
}





sampler PointSampler
{
	Filter=MIN_MAG_MIP_POINT;

	AddressU = CLAMP;
    AddressV = CLAMP;
};

struct PS_OUTPUT
{
    float4 cOut : SV_Target0;  // Pixel color
};


PS_OUTPUT PixOutRestore(VS_OUT In)
{ 
    PS_OUTPUT Out;

	Out.cOut=g_tDiffTexture[1].Sample(LerpSampler, In.vT0);

	if (g_vSettings.y!=1.0f)
		Out.cOut.rgb=pow(Out.cOut.rgb,g_vSettings.y);

	Out.cOut*=In.vColor;

	if (Out.cOut.a==0.0f)
		discard;
	else
		Out.cOut.rgb/=Out.cOut.a;

    return Out;
}

PS_OUTPUT PixOut(VS_OUT In)
{ 
    PS_OUTPUT Out;

	bool bReplaceAlpha=false;
	bool bPointSample=false;
	bool bDiscardPixels=false;

	if (bPointSample)
		Out.cOut=g_tDiffTexture[1].Sample(PointSampler, In.vT0);
	else
		Out.cOut=g_tDiffTexture[1].Sample(LerpSampler, In.vT0);

	if (g_vSettings.y!=1.0f)
		Out.cOut.rgb=pow(Out.cOut.rgb,g_vSettings.y);

	Out.cOut*=In.vColor;
	Out.cOut+=g_tDiffTexture[1].Sample(PointSampler, In.vT0);

	if (bDiscardPixels)
		if (Out.cOut.a==0.0f)
			discard;


	if (bReplaceAlpha)
		Out.cOut.a=In.vColor.a;

//	Out.cOut.rgb*=g_Buf.Load(0).rgb;
	
    return Out;
}


DepthStencilState DSS_NoZWriteGreater
{
	DepthWriteMask=0;
	DepthFunc=Greater;
};
DepthStencilState DSS_NoZWriteLess
{
	DepthWriteMask=0;
	DepthFunc=Less;
};

DepthStencilState DSS_NoZWriteGreaterEqual
{
	DepthWriteMask=0;
	DepthFunc=Greater_equal;
};

DepthStencilState DSS_ZWriteGreater
{
	//DepthWriteMask=0;
	DepthFunc=true;
};

DepthStencilState DSS_ZWriteLess
{
	//DepthWriteMask=0;
	DepthFunc=Less;
};


technique T0
{
    pass P0
    {
		SetBlendState(BS_AlphaBlend,-1,0xFFFFFFFF);
		SetDepthStencilState(DSS_NoZNoWrite,0);
		SetRasterizerState(RS_NoCull);

		aa=true;
		bb="asd"
				"aaaasd";
		//DefRange(zzz,-2,1);
		//DefRange(range,1,3);


		//SetVertexShader(50, VertOut);
		SetPixelShader(50, PixOut);
    }
/*
    pass P1	//Draw less, don't change Z
    {
		SetDepthStencilState(DSS_NoZWriteLess,0);
		SetRasterizerState(RS_NoCull);
		

		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(false,false,false)));
    }

    pass P2	//No blending
    {
		//SetBlendState(BS_AlphaBlend,float4(1,1,1,1),0xFFFFFFFF);
		SetDepthStencilState(DSS_NoZNoWrite,0);
		SetRasterizerState(RS_NoCull);
		

		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(true,false,false)));
    }

    pass P3	//Replace Z with Less magnitude
    {
		SetDepthStencilState(DSS_ZWriteLess,0);
		SetRasterizerState(RS_NoCull);
		

		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(false,false,false)));
    }

    pass P4	//Output with Z enabled, write 1 in Z
    {
		SetBlendState(BS_AlphaBlend,float4(1,1,1,1),0xFFFFFFFF);
		SetDepthStencilState(DSS_Default,0);
		SetRasterizerState(RS_NoCull);

      
		SetVertexShader(CompileShader(vs_4_0, VertOut(1)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(false,false,false)));
    }

    pass P5	//Point sampled P0
    {
		SetBlendState(BS_AlphaBlend,float4(1,1,1,1),0xFFFFFFFF);
		SetDepthStencilState(DSS_NoZNoWrite,0);
		SetRasterizerState(RS_NoCull);

      
		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(false,true,false)));
    }

    pass P6	//Draw greater equal, don't change Z
    {
		SetDepthStencilState(DSS_NoZWriteGreaterEqual,0);
		SetRasterizerState(RS_NoCull);
		

		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOut(false,false,false)));
    }

    pass P7	//Same as Pass0 with color by alpha restoration
    {
		SetBlendState(BS_AlphaBlend,float4(1,1,1,1),0xFFFFFFFF);
		SetDepthStencilState(DSS_NoZNoWrite,0);
		SetRasterizerState(RS_NoCull);

      
		SetVertexShader(CompileShader(vs_4_0, VertOut(0)));
		SetPixelShader(CompileShader(ps_4_0, PixOutRestore()));
    }*/
}


