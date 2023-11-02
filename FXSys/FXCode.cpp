#include "stdafx.h"
#include "FXCode.h"


#include <fstream>
#include <crtdbg.h>

#include <algorithm>
#include <functional>


#define FORMAT_VER 3

#define DEFAULT_NAME "$def"

const unsigned char g_auRNum[64]={0x3d,0xda,0xa7,0xd4,0x9a,0xca,0xf1,0x29,0x7b,0xf,
								0xe2,0xc2,0x74,0xc2,0x26,0x2c,0xa4,0xc1,0xfa,
								0xba,0x78,0x4,0x40,0xd8,0xe8,0x7f,0x6,0x2c,0x1d,
								0xd4,0x83,0x2d,0xe2,0xa5,0x52,0x1a,0xca,0xbf,0xfc,
								0xf5,0x6e,0xf9,0x8b,0x73,0xb7,0x2,0x40,0x76,0x8f,
								0xe4,0x1b,0x18,0xe8,0x95,0x5a,0xc6,0x81,0x48,0x9a,
								0x7f,0x4c,0xd6,0xc5,0xb8};


SFXPassGroup::SFXPassGroup()
{
	sRS=DEFAULT_NAME;
	sBS=DEFAULT_NAME;
	sDSS=DEFAULT_NAME;

	uBlendFactor=0;
	uSampleMask=-1;
	uStencilRef=0;

	for (TEntryPointVer &EP:	aEntryPoints)
		EP.second=0;
}

SFXPassGroup::SFXPassGroup(const SFXPassGroup &src)
{
	*this=src;
}

SFXPassGroup &SFXPassGroup::operator =(const SFXPassGroup &src)
{
	sName=src.sName;

	apPass.clear();
	for (auto &ptr:	src.apPass)
	{
		std::unique_ptr<SFXPass> pPass=std::make_unique<SFXPass>();
		if (ptr)
			*pPass=*ptr;

		apPass.emplace_back(std::move(pPass));
	}

	for (int n=0;n<_countof(aEntryPoints);++n)
		aEntryPoints[n]=src.aEntryPoints[n];

	mParamRange=src.mParamRange;
	mConstParam=src.mConstParam;

	uBlendFactor=src.uBlendFactor;
	uSampleMask=src.uSampleMask;
	uStencilRef=src.uStencilRef;
	sRS=src.sRS;
	sBS=src.sBS;
	sDSS=src.sDSS;

	return *this;
}

void SFXPassGroup::Save(FILE *f,int &rnRndPos)
{
	unsigned int DW;

	SFXCode::SaveString(f,sName,rnRndPos);

	fwrite(&uBlendFactor,sizeof(uBlendFactor),1,f);
	fwrite(&uSampleMask,sizeof(uSampleMask),1,f);
	fwrite(&uStencilRef,sizeof(uStencilRef),1,f);
	
	SFXCode::SaveString(f,sRS,rnRndPos);
	SFXCode::SaveString(f,sBS,rnRndPos);
	SFXCode::SaveString(f,sDSS,rnRndPos);

	DW=(unsigned int)mParamRange.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mParamRange)
	{
		SFXCode::SaveString(f,pair.first,rnRndPos);
		fwrite(&pair.second,sizeof(pair.second),1,f);
	}

	DW=(unsigned int)mConstParam.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mConstParam)
	{
		SFXCode::SaveString(f,pair.first,rnRndPos);
		SFXCode::SaveString(f,pair.second,rnRndPos);
	}

	for (int n=0;n<_countof(aEntryPoints);++n)
	{
		TEntryPointVer &EP=aEntryPoints[n];
		SFXCode::SaveString(f,EP.first,rnRndPos);
		fwrite(&EP.second,sizeof(EP.second),1,f);
	}

	DW=(unsigned int)apPass.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &ptr:	apPass)
	{
		for (TFXBytecode &aBuf:	ptr->aShaders)
		{
			DW=(unsigned int)aBuf.size();
			fwrite(&DW,sizeof(DW),1,f);
			if (aBuf.size())
				fwrite(&aBuf[0],1,aBuf.size(),f);
		}
	}
}

void SFXPassGroup::Load(std::istream &f,int &rnRndPos)
{
	unsigned int DW,n;

	sName=SFXCode::LoadString(f,rnRndPos);

	f.read((char *)&uBlendFactor,sizeof(uBlendFactor));
	f.read((char *)&uSampleMask,sizeof(uSampleMask));
	f.read((char *)&uStencilRef,sizeof(uStencilRef));
	
	sRS=SFXCode::LoadString(f,rnRndPos);
	sBS=SFXCode::LoadString(f,rnRndPos);
	sDSS=SFXCode::LoadString(f,rnRndPos);

	f.read((char *)&n,sizeof(n));
	while (n--)	
	{
		TParam P;
		std::string s=SFXCode::LoadString(f,rnRndPos);
		
		f.read((char *)&P,sizeof(P));
		mParamRange.emplace(s,P);
	}

	f.read((char *)&n,sizeof(n));
	while (n--)	
	{		
		std::string s=SFXCode::LoadString(f,rnRndPos);
		std::string ss=SFXCode::LoadString(f,rnRndPos);
						
		mConstParam.emplace(s,ss);
	}

	for (int n=0;n<_countof(aEntryPoints);++n)
	{
		TEntryPointVer &EP=aEntryPoints[n];
		EP.first=SFXCode::LoadString(f,rnRndPos);
		f.read((char *)&EP.second,sizeof(EP.second));
	}
	
	f.read((char *)&n,sizeof(n));
	while (n--)
	{
		std::unique_ptr<SFXPass> pFXPass=std::make_unique<SFXPass>();

		for (TFXBytecode &aBuf:	pFXPass->aShaders)
		{
			f.read((char *)&DW,sizeof(DW));			
			aBuf.resize(DW);
			
			if (aBuf.size())
				f.read((char *)&aBuf[0],aBuf.size());
		}

		apPass.emplace_back(std::move(pFXPass));
	}
}

void SFXTech::Save(FILE *f,int &rnRndPos)
{
	unsigned int DW;
	SFXCode::SaveString(f,sName,rnRndPos);

	DW=(unsigned int)aPassG.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (SFXPassGroup &PG:	aPassG)
		PG.Save(f,rnRndPos);
}

void SFXTech::Load(std::istream &f,int &rnRndPos)
{
	unsigned int DW;
	sName=SFXCode::LoadString(f,rnRndPos);
		
	f.read((char *)&DW,sizeof(DW));
	while (DW--)
	{
		aPassG.resize(aPassG.size()+1);
		aPassG.back().Load(f,rnRndPos);
	}
}






void SFXCodeHdr::Save(FILE *f,int &rnRndPos)
{
	unsigned short W,n;
	fwrite(&uVer,sizeof(uVer),1,f);

	W=(unsigned short)aDependences.size();
	fwrite(&W,sizeof(W),1,f);

	for (n=0;n<aDependences.size();++n)	
		aDependences[n].Save(f);

	fwrite(&uDefinitionsHash,sizeof(uDefinitionsHash),1,f);

	SFXCode::SaveString(f,sName,rnRndPos);
}

bool SFXCodeHdr::Load(std::istream &f,int &rnRndPos)
{
	unsigned short W,n;	
	f.read((char *)&uVer,sizeof(uVer));

	f.read((char *)&W,sizeof(W));	
	_ASSERTE(W<50);
	aDependences.resize(W);

	for (n=0;n<aDependences.size();++n)	
	if (!aDependences[n].Load(f))
		return false;

	f.read((char *)&uDefinitionsHash,sizeof(uDefinitionsHash));

	sName=SFXCode::LoadString(f,rnRndPos);

	return true;
}

void SFXCodeHdr::Clear()
{
	aDependences.clear();
}













SFXCode::SFXCode():uFormatVer(0),bCompiled(false),nLastWrittenLine(1),uLastWrittenToken(0)
{
	Clear();
}
SFXCode::~SFXCode()
{
}

void SFXCode::Clear()
{
	FX_BLEND_DESC BS;
	FX_RASTERIZER_DESC RS;
	FX_DEPTH_STENCIL_DESC DSS;
	
	uLastWrittenToken=0;
	nLastWrittenLine=1;
	bCompiled=false;
	Header.Clear();
	aOutLines.clear();
	
	mSamplers.clear();
	mBS.clear();
	mRS.clear();
	mDSS.clear();
	mTech.clear();
	mFuncDesc.clear();
	mDefinitions.clear();

	SetDefaultBS(BS);
	SetDefaultRS(RS);
	SetDefaultDSS(DSS);

	mBS.emplace(DEFAULT_NAME,BS);
	mRS.emplace(DEFAULT_NAME,RS);
	mDSS.emplace(DEFAULT_NAME,DSS);
}

bool SFXCode::Save(const char *sFileName)
{
	FILE *f=0;
	fopen_s(&f,sFileName,"wb");
	if (f)
	{
		int nRndPos=0;
		const char *sFN=strrchr(sFileName,'\\');
		if (sFN)
		{
			static std::hash<std::string> h;
			std::string s(sFN+1);
			_strlwr_s((char *)s.c_str(),s.length()+1);

			nRndPos=h(s) & (_countof(g_auRNum)-1);
		}

		Save(f,nRndPos);

		fclose(f);
		return true;
	}

	return false;
}

void SFXCode::Save(FILE *f,int &rnRndPos)
{
	unsigned char B=FORMAT_VER;
	unsigned long DW;

	Header.uVer=0;
	
	fwrite(&B,1,1,f);
	Header.Save(f,rnRndPos);

	fwrite(&bCompiled,1,1,f);
	


	DW=(unsigned long)mSamplers.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mSamplers)
	{
		SaveString(f,pair.first,rnRndPos);
		int nnn=ftell(f);
		fwrite(&pair.second,sizeof(pair.second),1,f);
	}
	
	DW=(unsigned long)mRS.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mRS)
	{
		SaveString(f,pair.first,rnRndPos);
		fwrite(&pair.second,sizeof(pair.second),1,f);
	}

	DW=(unsigned long)mBS.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mBS)
	{
		SaveString(f,pair.first,rnRndPos);
		fwrite(&pair.second,sizeof(pair.second),1,f);
	}

	DW=(unsigned long)mDSS.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mDSS)
	{
		SaveString(f,pair.first,rnRndPos);
		fwrite(&pair.second,sizeof(pair.second),1,f);
	}

	DW=(unsigned long)mDefinitions.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mDefinitions)
	{
		SaveString(f,pair.first,rnRndPos);
		SaveString(f,pair.second,rnRndPos);
	}

	DW=(unsigned int)mVarsInitData.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mVarsInitData)
	{
		SaveString(f,pair.first,rnRndPos);

		DW=(unsigned int)pair.second.size();
		fwrite(&DW,sizeof(DW),1,f);
		if (pair.second.size())
			fwrite(&pair.second[0],1,pair.second.size(),f);
	}

	DW=(unsigned int)mVarsAttrs.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mVarsAttrs)
	{
		SaveString(f,pair.first,rnRndPos);
		fwrite(&pair.second,sizeof(int),1,f);
	}


	DW=(unsigned long)mTech.size();
	fwrite(&DW,sizeof(DW),1,f);
	for (auto &pair:	mTech)
		pair.second.Save(f,rnRndPos);
}


bool SFXCode::Load(std::istream &rInput,const char *sFileName,bool bHeaderOnly)
{
	uFormatVer=0;	
	
	if (!rInput.fail())
	{
		int nRndPos=0;
		const char *sFN=strrchr(sFileName,'\\');
		if (sFN)
		{
			static std::hash<std::string> h;
			std::string s(sFN+1);
			_strlwr_s((char *)s.c_str(),s.length()+1);

			nRndPos=h(s) & (_countof(g_auRNum)-1);
		}


		bool bRet=Load(rInput,nRndPos,bHeaderOnly);

		bCompiled&=bRet;
		return bRet;
	}
	
	return false;
}

bool SFXCode::Load(std::istream &f,int &rnRndPos,bool bHeaderOnly)
{
	std::string s;
	unsigned long DW;
	

	uFormatVer=f.get();
	if (uFormatVer==FORMAT_VER)
	{
		if (Header.Load(f,rnRndPos))
		{
			if (bHeaderOnly)
				return true;
//			if (Header.uVer!=GetCompilerVer())
	//			return false;


			f.read((char *)&bCompiled,1);
			
			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				FX_SAMPLER &S=mSamplers[LoadString(f,rnRndPos)];
				f.read((char *)&S,sizeof(S));
			}

			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				FX_RASTERIZER_DESC &S=mRS[LoadString(f,rnRndPos)];
				f.read((char *)&S,sizeof(S));
			}
			
			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				FX_BLEND_DESC &S=mBS[LoadString(f,rnRndPos)];
				f.read((char *)&S,sizeof(S));
			}

			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				FX_DEPTH_STENCIL_DESC &S=mDSS[LoadString(f,rnRndPos)];
				f.read((char *)&S,sizeof(S));
			}
			
			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				std::string sName=LoadString(f,rnRndPos);
				mDefinitions[sName]=LoadString(f,rnRndPos);
			}

			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				std::string sName=LoadString(f,rnRndPos);
				unsigned int uSZ;
				TInitData IData;

				f.read((char *)&uSZ,sizeof(uSZ));
				if (uSZ)
				{					
					IData.resize(uSZ);
					f.read((char *)&IData[0],uSZ);
				}
				mVarsInitData[sName]=IData;				
			}

			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				std::string sName=LoadString(f,rnRndPos);
				unsigned int uAttr;
				f.read((char *)&uAttr,sizeof(uAttr));
				mVarsAttrs[sName]=uAttr;
			}

			f.read((char *)&DW,sizeof(DW));
			while (DW--)
			{
				SFXTech T;
				T.Load(f,rnRndPos);
				mTech.emplace(T.sName,T);
			}
			
			return !f.fail();
		}
	}

	return false;
}







unsigned long SFXCode::GetCompilerVer()const
{
//	_ASSERTE(pOwner);
	if (1)//pOwner)
	{
		unsigned long uVer=0;

		return uVer;
	}

	return 0;
}


void SFXCode::SaveString(FILE *f,const std::string &s,int &rnRndPos)
{
	std::string sStr(s);

	unsigned char B=(unsigned char)sStr.length();
	sStr.resize(B);

	for (size_t n=0;n<sStr.length();++n)
	{
		sStr[n]^=g_auRNum[rnRndPos++];
		rnRndPos&=_countof(g_auRNum)-1;
	}

	
	fwrite(&B,1,1,f);
	fwrite(sStr.c_str(),B,1,f);
}

std::string SFXCode::LoadString(std::istream &f,int &rnRndPos)
{
	unsigned char B=f.get();
	std::string s;

	s.resize(B);
	if (B)
		f.read((char *)s.c_str(),B);

	for (size_t n=0;n<s.length();++n)
	{
		s[n]^=g_auRNum[rnRndPos++];
		rnRndPos&=_countof(g_auRNum)-1;
	}

	return s;
}


int SFXCode::GetFormatVersion()
{
	return FORMAT_VER;
}



void SFXCode::FormatSource(std::string &sDest,bool bAllLines,std::vector<TSourceIDLine> *panLineID)
{
	sDest="";

	if (panLineID)
		panLineID->clear();

	for (size_t l=0;l<aOutLines.size();++l)
	{
		auto &tuple=aOutLines[l];

		if (panLineID)
		{
			if (bAllLines || std::get<2>(tuple))
			{
				panLineID->push_back(std::get<0>(tuple));

				sDest+=std::get<1>(tuple);				
				sDest+="\r\n";
			}
		}
		else
		{
			if (bAllLines || std::get<2>(tuple))
				sDest+=std::get<1>(tuple);
				
			sDest+="\r\n";
		}
	}

	if (sDest.length()>=2)
		sDest.resize(sDest.length()-2);
}

void SFXCode::AppendOutLine(int nLineID,unsigned int uLineOffset)
{
	aOutLines.push_back(std::make_tuple(nLineID,"",true));
	
	while (uLineOffset)
	{
		if (uLineOffset>0xFFFF)
		{
			std::get<1>(aOutLines.back())+='\t';
			uLineOffset-=1<<16;
		}
		else
		{
			std::get<1>(aOutLines.back())+=' ';
			uLineOffset--;
		}
	}
}

void SFXCode::UnmarkAllFunctions()
{
	for (auto &pair:	mFuncDesc)
	for (size_t n=pair.second.uStartLine;n<=pair.second.uLastLine;++n)
		std::get<2>(aOutLines[n])=false;
}

void SFXCode::MarkUsedFunction(const std::string &sName)
{
	auto it=mFuncDesc.lower_bound(sName);
	
	while (it!=mFuncDesc.end() && it->first==sName)
	{
		if (!std::get<2>(aOutLines[it->second.uStartLine]))
		{
			for (size_t n=it->second.uStartLine;n<=it->second.uLastLine;++n)
				std::get<2>(aOutLines[n])=true;

			for (const std::string &s:	it->second.sReferencedFunc)
				MarkUsedFunction(s);
		}

		++it;
	}
}