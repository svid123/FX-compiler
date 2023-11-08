#include "stdafx.h"
#include "FXCode.h"


#include <fstream>
#include <crtdbg.h>

#include <algorithm>
#include <functional>


#define FORMAT_VER 4

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

bool SFXPassGroup::Serialize(SSerializerIOContext &io)
{
	io<<sName;

	io<<uBlendFactor;
	io<<uSampleMask;
	io<<uStencilRef;

	io<<sRS;
	io<<sBS;
	io<<sDSS;

	io<<mParamRange;
	io<<mConstParam;

	for (auto &item:	aEntryPoints)
		io<<item;	

	io<<apPass;

	return true;
}

bool SFXTech::Serialize(SSerializerIOContext &io)
{
	io<<sName;
	io<<aPassG;
	return true;
}






bool SFXCodeHdr::Serialize(SSerializerIOContext &io)
{	
	io<<uVer;

	io<<aDependences;
	io<<uDefinitionsHash;
	io<<sName;

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
	mVarsInitData.clear();
	mVarsAttrs.clear();

	SetDefaultBS(BS);
	SetDefaultRS(RS);
	SetDefaultDSS(DSS);

	mBS.emplace(DEFAULT_NAME,BS);
	mRS.emplace(DEFAULT_NAME,RS);
	mDSS.emplace(DEFAULT_NAME,DSS);

	bSerializeHeaderOnly=false;
}

bool SFXCode::Save(const char *sFileName)
{
	bool bRet=false;
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

		bSerializeHeaderOnly=false;

		SSerializerIOContext IO(f,nRndPos);
		Header.uVer=0;

		bRet=Serialize(IO);

		fclose(f);
	}

	return false;
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

		bSerializeHeaderOnly=bHeaderOnly;

		SSerializerIOContext IO(rInput,nRndPos);
		bool bRet=Serialize(IO);

		bCompiled&=bRet;
		return bRet;
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

bool SFXCode::Serialize(SSerializerIOContext &io)
{
	uFormatVer=FORMAT_VER;
	io<<uFormatVer;

	if (uFormatVer!=FORMAT_VER)
		return false;
	
	io<<Header;
	if (bSerializeHeaderOnly)
		return true;

	io<<bCompiled;

	io<<mSamplers;
	io<<mRS;
	io<<mBS;
	io<<mDSS;
	io<<mDefinitions;
	io<<mVarsInitData;
	io<<mVarsAttrs;

	io<<mTech;

	return !io.bErrors;
}