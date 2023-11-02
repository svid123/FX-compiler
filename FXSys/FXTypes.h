#pragma once

#define BEGIN_FX_ENUM(ename)	enum ename {
#define END_FX_ENUM	};
#define FX_ENUM(prefix,name,val)		prefix##_##name=val,

#include "FXEnums.inc"

#undef BEGIN_FX_ENUM
#undef END_FX_ENUM
#undef FX_ENUM


typedef struct{
				FX_FILTER               Filter;
				FX_TEXTURE_ADDRESS_MODE AddressU;
				FX_TEXTURE_ADDRESS_MODE AddressV;
				FX_TEXTURE_ADDRESS_MODE AddressW;
				float                   MipLODBias;
				unsigned int            MaxAnisotropy;
				FX_COMPARISON_FUNC		ComparisonFunc;
				unsigned int            BorderColor;
				float                   MinLOD;
				float                   MaxLOD;
				}FX_SAMPLER;



typedef struct {
				FX_FILL_MODE FillMode;
				FX_CULL_MODE CullMode;
				bool            FrontCounterClockwise;
				int             DepthBias;
				float           DepthBiasClamp;
				float           SlopeScaledDepthBias;
				bool            DepthClipEnable;
				bool            ScissorEnable;
				bool            MultisampleEnable;
				bool            AntialiasedLineEnable;
			} FX_RASTERIZER_DESC;

typedef struct {
				bool           AlphaToCoverageEnable;
				bool           BlendEnable[8];
				FX_BLEND       SrcBlend;
				FX_BLEND       DestBlend;
				FX_BLEND_OP    BlendOp;
				FX_BLEND       SrcBlendAlpha;
				FX_BLEND       DestBlendAlpha;
				FX_BLEND_OP    BlendOpAlpha;
				unsigned char  RenderTargetWriteMask[8];
				} FX_BLEND_DESC;

typedef struct {
				  FX_STENCIL_OP      StencilFailOp;
				  FX_STENCIL_OP      StencilDepthFailOp;
				  FX_STENCIL_OP      StencilPassOp;
				  FX_COMPARISON_FUNC StencilFunc;
				} FX_DEPTH_STENCILOP_DESC;

typedef struct {
				  bool                       DepthEnable;
				  FX_DEPTH_WRITE_MASK        DepthWriteMask;
				  FX_COMPARISON_FUNC         DepthFunc;
				  bool                       StencilEnable;
				  unsigned char              StencilReadMask;
				  unsigned char              StencilWriteMask;
				  FX_DEPTH_STENCILOP_DESC    FrontFace;
				  FX_DEPTH_STENCILOP_DESC    BackFace;
				} FX_DEPTH_STENCIL_DESC;

inline void SetDefaultSampler(FX_SAMPLER &S)
{
	memset(&S,0,sizeof(S));

	S.Filter=FX_FILTER_MIN_MAG_MIP_POINT;
	S.AddressU=S.AddressV=S.AddressW=FX_TEXTURE_ADDRESS_CLAMP;
	S.MaxLOD=1e3f;
	S.MaxAnisotropy=16;
	S.ComparisonFunc=FX_COMPARISON_NEVER;	
}

inline void SetDefaultRS(FX_RASTERIZER_DESC &RS)
{
	memset(&RS,0,sizeof(RS));
	RS.FillMode=FX_FILL_SOLID;
	RS.CullMode=FX_CULL_BACK;
	RS.FrontCounterClockwise=false;
	RS.DepthBias=0;
	RS.DepthBiasClamp=0;
	RS.SlopeScaledDepthBias=0;
	RS.DepthClipEnable=true;
	RS.ScissorEnable=false;
	RS.MultisampleEnable=false;
	RS.AntialiasedLineEnable=false;
}

inline void SetDefaultBS(FX_BLEND_DESC &BS)
{
	memset(&BS,0,sizeof(BS));
	BS.SrcBlend=FX_BLEND_ONE;
	BS.DestBlend=FX_BLEND_ZERO;
	BS.BlendOp=FX_BLEND_OP_ADD;
	BS.SrcBlendAlpha=FX_BLEND_ONE;
	BS.DestBlendAlpha=FX_BLEND_ZERO;
	BS.BlendOpAlpha=FX_BLEND_OP_ADD;
	
	memset(BS.RenderTargetWriteMask,0xF,sizeof(BS.RenderTargetWriteMask));
}

inline void SetDefaultDSS(FX_DEPTH_STENCIL_DESC &DSS)
{
	memset(&DSS,0,sizeof(DSS));
	DSS.DepthEnable=true;
	DSS.DepthWriteMask=FX_DEPTH_WRITE_MASK_ALL;
	DSS.DepthFunc=FX_COMPARISON_LESS;
	DSS.StencilEnable=false;
	DSS.StencilReadMask=0xFF;
	DSS.StencilWriteMask=0xFF;

	DSS.FrontFace.StencilFailOp=FX_STENCIL_OP_KEEP;
	DSS.FrontFace.StencilDepthFailOp=FX_STENCIL_OP_KEEP;
	DSS.FrontFace.StencilPassOp=FX_STENCIL_OP_KEEP;
	DSS.FrontFace.StencilFunc=FX_COMPARISON_ALWAYS;

	DSS.BackFace.StencilFailOp=FX_STENCIL_OP_KEEP;
	DSS.BackFace.StencilDepthFailOp=FX_STENCIL_OP_KEEP;
	DSS.BackFace.StencilPassOp=FX_STENCIL_OP_KEEP;
	DSS.BackFace.StencilFunc=FX_COMPARISON_ALWAYS;
}