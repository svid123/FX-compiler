# FXSys
HLSL Effect precompiler based on EBNF and DXC for DX12

Currently provided sources and project are intended for MSVS2022.<br />
Basically it translates FX HLSL into raw HLSL sources with removed parts of non-standard HLSL code and adds macro definitions for HLSL DXC compiler<br />
Compiler provides object SFXCode with compiled DX bytecode and additional render state settings such as: BlendState, RasterState, DepthstencilState, sampler states, global initializers as well as definitions in Microsoft FX-like manner.<br />
Currently render state objects reflects DX11-like render state objects (i.e. no conservative rasterizer in RasterDesc etc.)<br />
Additionaly it provides global definitions per pass, definition-ranges per pass with automatically built variants of passes for each value of definition range<br />

What it does:<br/>
* Parses all the sources into single translation unit (i.e. all the includes are emplaced into final translation unit for DXC)
* Removes redunant code for given pass to speedup DXC ompiling as well as remove possible conflicts due to pass definitions
* Collects into SFXCode structure and removes global constants initializers from HLSL (like 'float4 g_vSetting=float4(1,1,1,0)' => 'float4 g_vSetting'). Supported array initializers for global, as well as character string initializers for local variables
* Automatically build serparate passes for pass group based on DefRange-s

Extensions of standard HLSL are<br />
1) Support for 'enum' keyword (translates values into macro definitins, enum tag in HLSL treated as 'int' type)
2) Converts character string of initializers into array of characters (i.e. uint chars[]="text" => uint chars[]={'t','e','x','t'})
3) Precompiler collects global initializers of constants and sampelers (and removes from HLSL for DXC).
4) Has it's own C preprocessor with supported keywords:<br /> 
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
   </p>
   Allows bypass of preprocessor keywords for DXC using ## for keyword (like ##ifdef)<br />
   Supports standard C macro expansion for macro arguments
5) Has internal constant-expressions parser for preprocessor #if keyword and all the FX entities (i.e. DepthBias=1+0.5*2)
6) Provides bypass ## for preprocessor keywords (such directives are not processed by FXSys and kept for DXC)
7) Precompiler keywords<br/>
   **technique**<br/>
   **pass**<br/>
   **DefRange**<br/>
   **RasterizerState**<br/>
   **BlendState**<br/>
   **SetBlendState**<br/>
   **SetRasterizerState**<br/>
   **SetDepthStencilState**<br/>
   **SetVertexShader**<br/>
   **SetPixelShader**<br/>
   **SetGeometryShader**<br/>
   **SetHullShader**<br/>
   **SetDomainShader**<br/>
   **SetComputeShader**<br/>

Example syntax for technique definition:
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
		g_bPointSample=false;	//..
		g_bDiscardPixels=true;	//..
		DefRange(g_nShadingMode,0,5)	//Definition range for auto-generated versions of P0 pass

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

Example of state definition:
```
DepthStencilState DSS_NoZWriteGreaterEqual
{
	DepthWriteMask=0;
	DepthFunc=Greater_equal;
};
```

Example of samplers definition
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
