#ifndef COMMSTATES_INC
#define COMMSTATES_INC

RasterizerState RS_NoCull
{
	CullMode=NONE;
};

RasterizerState RS_Wireframe
{
	FillMode=WIREFRAME;
};

BlendState BS_AlphaBlend
{
	BlendEnable[0]=true;
	SrcBlend=SRC_ALPHA;
	DestBlend=INV_SRC_ALPHA;

	SrcBlendAlpha=ONE;
	DestBlendAlpha=INV_SRC_ALPHA;
};

BlendState BS_BlendOne
{
	BlendEnable[0]=true;

	SrcBlend=ONE;
	DestBlend=ONE;
	SrcBlendAlpha=ONE;
	DestBlendAlpha=ONE;
};
BlendState BS_BlendMul
{
	BlendEnable[0]=true;

	SrcBlend=ZERO;
	DestBlend=SRC_COLOR;
	SrcBlendAlpha=ZERO;
	DestBlendAlpha=SRC_ALPHA;
};

DepthStencilState DSS_NoZNoWrite
{
	DepthEnable=false;
	DepthWriteMask=0;
};
DepthStencilState DSS_NoWrite
{
	DepthFunc=GREATER;
	DepthWriteMask=0;
};

DepthStencilState DSS_Default
{
	DepthFunc=GREATER;
};

BlendState BS_NoColor
{
	RenderTargetWriteMask[0]=1;
};




#endif