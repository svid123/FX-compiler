#pragma once

#include "CodeDependence.h"

#include <vector>
#include <set>
#include <memory>
#include <unordered_map>
#include <map>

#include <iostream>

#include "FXTypes.h"
#include "Serializer.h"

typedef int TExpLine;

class CExpCompiler;

struct SFXCodeHdr:	public SSerializable
{
	std::string sName;
	TADependences aDependences;
	unsigned long uVer,
				uDefinitionsHash;
	SFXCodeHdr():uVer(0),uDefinitionsHash(0)
	{
	}

	virtual bool Serialize(SSerializable::SSerializerIOContext &io) override;

	void Clear();
};

typedef std::vector<unsigned char> TFXBytecode;

struct SFXPass:	public SSerializable
{
	enum FX_SHADER
	{
		FXS_VS=0,
		FXS_PS,
		FXS_GS,
		FXS_HS,
		FXS_DS,
		FXS_CS,

		FXS_SIZE
	};

	TFXBytecode aShaders[FXS_SIZE];
	virtual bool Serialize(SSerializable::SSerializerIOContext &io) override
	{
		for (TFXBytecode &code:	aShaders)
			io<<code;

		return true;
	}
};


struct SFXPassGroup:	public SSerializable
{
	typedef struct{
					unsigned char uIndex;
					char nMin,nMax;
					}TParam;
	typedef unsigned int TPassHash;
	typedef std::pair<std::string,int> TEntryPointVer;
	
	std::string sName;
	std::vector<std::unique_ptr<SFXPass>> apPass;
	std::unordered_map<std::string,TParam> mParamRange;
	std::unordered_map<std::string,std::string> mConstParam;

	TEntryPointVer aEntryPoints[SFXPass::FXS_SIZE];

	unsigned int uBlendFactor,uSampleMask;
	unsigned char uStencilRef;
	std::string sRS,sBS,sDSS;

	SFXPassGroup();
	SFXPassGroup(const SFXPassGroup &src);

	virtual bool Serialize(SSerializable::SSerializerIOContext &io) override;

	SFXPassGroup &operator =(const SFXPassGroup &src);
};

struct SFXTech:	public SSerializable
{
	std::string sName;
	std::vector<SFXPassGroup> aPassG;

	virtual bool Serialize(SSerializable::SSerializerIOContext &io) override;
};

struct SFXCode:	public SSerializable
{
	typedef int TSourceIDLine;
	typedef bool TEnabledLine;
	typedef std::tuple<TSourceIDLine,std::string,TEnabledLine> TSourceLine;
	typedef std::vector<unsigned char> TInitData;

	

	struct SFuncDesc
	{
		std::string sName;
		size_t uStartLine,uLastLine;
		std::set<std::string> sReferencedFunc;

		SFuncDesc():uStartLine(0),uLastLine(0)
		{
		}
		SFuncDesc(const char *sname):sName(sname),uStartLine(0),uLastLine(0)
		{
		}
		
		void Reset()
		{
			sName="";
			sReferencedFunc.clear();
		}
	};

	bool bSerializeHeaderOnly;
	virtual bool Serialize(SSerializable::SSerializerIOContext &io) override;

public:
	enum VAR_ATTR_BIT
	{
		VAB_ROOTPARAM=0,
		VAB_ROOTCONST,

		VAB_SIZE
	};

	unsigned char uFormatVer;
	int nLastWrittenLine;
	unsigned int uLastWrittenToken;

	std::unordered_map<std::string,SFXTech> mTech;

	std::unordered_map<std::string,FX_SAMPLER> mSamplers;
	std::unordered_map<std::string,FX_RASTERIZER_DESC> mRS;
	std::unordered_map<std::string,FX_BLEND_DESC> mBS;
	std::unordered_map<std::string,FX_DEPTH_STENCIL_DESC> mDSS;
	std::multimap<std::string,SFuncDesc> mFuncDesc;
	std::vector<TSourceLine> aOutLines;
	std::unordered_map<std::string,std::string> mDefinitions;
	std::unordered_map<std::string,TInitData> mVarsInitData;
	std::unordered_map<std::string,unsigned int> mVarsAttrs;
	
	SFXCodeHdr Header;
	bool bCompiled;

	SFXCode();
	~SFXCode();
	
	bool Save(const char *sFileName);
	bool Load(std::istream &rInput,const char *sFileName,bool bHeaderOnly);

	unsigned long GetCompilerVer()const;

	static int GetFormatVersion();
	void FormatSource(std::string &sDest,bool bAllLines=false,std::vector<TSourceIDLine> *panLineID=0);
	void MarkUsedFunction(const std::string &sName);
	void UnmarkAllFunctions();

	void AppendOutLine(int nLineID,unsigned int uLineOffset);

	void Clear();
};


