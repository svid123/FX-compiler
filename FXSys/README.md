# FXSys
Lightweight HLSL Effect precompiler based on EBNF and DXC for DX12

Currently provided sources and project are intended for MSVS2022. Target platform Win64<br />
Basically it translates FX HLSL into raw HLSL sources with removed parts of non-standard HLSL code and adds macro definitions for HLSL DXC compiler (not included headers and libs)<br />
Compiler provides object SFXCode with compiled DX bytecode and additional render state settings such as: BlendState, RasterState, DepthstencilState, sampler states, global initializers as well as definitions in Microsoft FX-like manner.<br />
Currently render state objects reflects DX11-like render state objects (i.e. no conservative rasterizer in RasterDesc etc.)<br />
Additionaly it provides global definitions per pass, definition-ranges per pass with automatically built variants of passes for each value of definition range<br />

### What it does:
* Parses all the sources of translation unit into single translation unit (i.e. all the includes are emplaced into final translation unit for DXC)
* Removes redunant code for given pass to speedup DXC compiling as well as remove possible conflicts due to pass definitions
* Collects into SFXCode structure and removes global constants initializers from HLSL (like 'float4 g_vSetting=float4(1,1,1,0)' => 'float4 g_vSetting'). Supported array initializers for global, as well as character string initializers for local variables
* Automatically build separate passes for pass group based on DefRange-s
* Packs all the stuff into SFXCode

### Extensions of standard HLSL are
1) Support for 'enum' keyword (translates values into macro definitins, enum tag in HLSL treated as 'int' type)
2) Converts character string of initializers into array of characters (i.e. uint chars[]="text" => uint chars[]={'t','e','x','t'})
3) Precompiler collects global initializers of constants and sampelers (and removes from HLSL for DXC).
4) Precompiler keywords<br/>
   **enum**<br/>
   **technique**<br/>
   **pass**<br/>
   **DefRange**<br/>
   **RasterizerState**<br/>
   **BlendState**<br/>
   **DepthStencilState**<br/>
   **SetBlendState**<br/>
   **SetRasterizerState**<br/>
   **SetDepthStencilState**<br/>
   **SetVertexShader**<br/>
   **SetPixelShader**<br/>
   **SetGeometryShader**<br/>
   **SetHullShader**<br/>
   **SetDomainShader**<br/>
   **SetComputeShader**<br/>
5) Has it's own C preprocessor with supported directives:<br /> 
	**#include**<br />
	**#define**<br />
	**#if**<br />
	**#ifdef**<br />
	**#else**<br />
	**#elif**<br />
	**#endif**<br />
	**#pragma** (once, far_extern)<br />
	<p>Supported built-in macros:<br/>
	__LINE__<br />
	__FILE__<br />
	__DATE__<br />
	__TIME__<br />
	__VA_ARG__<br />
   </p>
   Allows bypass of preprocessor directives for DXC using ## for keyword (like ##ifdef)<br />
   Supports standard C macro expansion for macro arguments 
6) Has internal constant-expressions parser for preprocessor #if directive and all the FX entities (i.e. DepthBias=1+0.5*2)
7) Provides bypass ## for preprocessor keywords (such directives are not processed by FXSys and kept for DXC)
8) Custom attributes<br/>
**[root_param]**<br/>
**[root_const]**<br/>
These attributes are collected into separate dictionary with belonging ID's as helpers for inline Root Signature constants and parameters



### Example syntax for technique definition:
```
technique T0	//tech name
{
    pass P0		//pass name
    {
		SetBlendState(BS_AlphaBlend,0xFFFFFFFF,0xFFFFFFFF);
		SetDepthStencilState(DSS_NoZNoWrite,0);
		SetRasterizerState(RS_NoCull);

		g_fZ=0;			//Global macro definitions
		g_bReplaceAlpha=false;	//..
		g_bPointSample=!g_bReplaceAlpha;	//..
		g_bDiscardPixels=true;	//..
		DefRange(g_nShadingMode,0,5)	//Definition range for auto-generated versions of P0 pass, in range [0..5]

		SetVertexShader(40, VertOut);	//Entry points
		SetPixelShader(40, PixOut);	//..
    }

    pass P1
    {
		SetDepthStencilState(DSS_NoZWriteLess,0);
		SetRasterizerState(RS_NoCull);
		
		g_fZ=0;
		g_bReplaceAlpha=false;
		g_bPointSample=false;
		g_bDiscardPixels=false;

		SetVertexShader(40, VertOut);	//vs_4_0 shader model
		SetPixelShader(40, PixOut);	//ps_4_0 ..
    }
}
```

### Example of state definition:
```
DepthStencilState DSS_NoZWriteGreaterEqual
{
	DepthWriteMask=0;
	DepthFunc=Greater_equal;
};
```

### Example of samplers definition
```
SamplerState g_aSamplers[2]=	//array of samplers
{
	SamplerState	//Lerp
	{
		Filter=MIN_MAG_MIP_LINEAR;
		AddressU = Wrap;
		AddressV = Wrap;
		AddressW = Wrap;

		Filter=ANISOTROPIC;
		MaxAnisotropy=8;
	},

	SamplerState	//Point
	{
		Filter=MIN_MAG_MIP_POINT;
		AddressU = Wrap;
		AddressV = Wrap;
		AddressW = Wrap;
		MaxLOD=0;
	}
};

sampler LerpSampler
{
	Filter=MIN_MAG_MIP_LINEAR;

	AddressU = CLAMP;
	AddressV = CLAMP;
};
```

### Global initializers support
Currently supported types of initializers are scalar and vector/matrix types, such as<br/>
**uint**<br/>
**int**<br/>
**float**<br/>
**double**<br/>
With up to two dimensions (i.e. int4, float3x3, etc.)<br/>
Shader model 6.0 and higher does not support global initializers, so FXSys collects these values and creates aligned data-buffers in separate dictionary and removes it from output translation unit. Constant values support scalar math, binary, bitwise operators<br/>

### Example of global initializers:
```
#define VAR_MULTIPLIER 2.0f

typedef float4 color4;

float g_fVar = 14.0f*VAR_MULTIPLIER;

color4 g_avColors[3] = {color4(1,1,1,1),color4(1,1,1,1),color4(1,1,1,1)};

float2x2 g_amRotations[2][2] = {{float2x2(1,0,0,-1),float2x2(0,-1,1,0)},
				{float2x2(1,1,1,1),float2x2(0,0,0,0)}};

cbuffer CBSetup
{
	float4 g_vSetup = float4(1,0.5,0,0);
	float4 g_vViewport = float4(0,0,1,1);
};
```
