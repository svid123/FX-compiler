
##define A 1


void VertOut(VS_IN In, out VS_OUT Out)
{
	float3 vV;
	
	uint str[]=__FILE__;
	

	Out.vPos.w=sqrt(g_vColor.r);
	Out.vPos.x=((In.vPos.x+1)*0.5f*g_vTransform.x+g_vTransform.z)*2-1;
	Out.vPos.y=1-((1-In.vPos.y)*0.5f*g_vTransform.y+g_vTransform.w)*2;
	Out.vPos.z=0;
	//Out.vPos.w=1.0;//cv4.z;
	Out.vT0.x=In.vPos.x*0.5f+0.5f;
	Out.vT0.y=0.5f-In.vPos.y*0.5f;
	Out.vT0.xy=Out.vT0.xy*g_vTexTransform.zw+g_vTexTransform.xy;


	/*
	if(nAllSamples==1)
	{
		if(1<=1)
			{vAlbedo=g_tms1Albedo.Load(nvTC);vNorm=g_tms1Normals.Load(nvTC);}
		else
			{vAlbedo=g_tms1Albedo.Load(nvTC,nSample);vNorm=g_tms1Normals.Load(nvTC,nSample);}
	}
	else
	if(nAllSamples==2)
	{
		if(2<=1)
			{vAlbedo=g_tms1Albedo.Load(nvTC);vNorm=g_tms1Normals.Load(nvTC);}
		else
			{vAlbedo=g_tms2Albedo.Load(nvTC,nSample);vNorm=g_tms2Normals.Load(nvTC,nSample);}
	}
	*/
	Out.vColor.r*=g_tDiffTexture[0].Load(int3(Out.vPos.xyz));
	Out.vColor=g_vColor*str[0];
}
