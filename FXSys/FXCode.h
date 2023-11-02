#pragma once

#include "CodeDependence.h"

#include <vector>
#include <set>
#include <memory>
#include <unordered_map>
#include <map>

#include <iostream>

#include "FXTypes.h"

typedef int TExpLine;

class CExpCompiler;

struct SFXCodeHdr
{
	std::string sName;
	TADependences aDependences;
	unsigned long uVer,
				uDefinitionsHash;
	SFXCodeHdr():uVer(0),uDefinitionsHash(0)
	{
	}

	void Save(FILE *f,int &rnRndPos);
	bool Load(std::istream &f,int &rnRndPos);

	void Clear();
};

typedef std::vector<unsigned char> TFXBytecode;

struct SFXPass
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
};


struct SFXPassGroup
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

	void Save(FILE *f,int &rnRndPos);
	void Load(std::istream &f,int &rnRndPos);

	SFXPassGroup &operator =(const SFXPassGroup &src);
};

struct SFXTech
{
	std::string sName;
	std::vector<SFXPassGroup> aPassG;

	void Save(FILE *f,int &rnRndPos);
	void Load(std::istream &f,int &rnRndPos);
};

struct SFXCode
{
	typedef int TSourceIDLine;
	typedef bool TEnabledLine;
	typedef std::tuple<TSourceIDLine,std::string,TEnabledLine> TSourceLine;
	typedef std::vector<unsigned char> TInitData;

	bool Load(std::istream &f,int &rnRndPos,bool bHeaderOnly);
	void Save(FILE *f,int &rnRndPos);

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

	static void SaveString(FILE *f,const std::string &s,int &rnRndPos);
	static std::string LoadString(std::istream &f,int &rnRndPos);

	static int GetFormatVersion();
	void FormatSource(std::string &sDest,bool bAllLines=false,std::vector<TSourceIDLine> *panLineID=0);
	void MarkUsedFunction(const std::string &sName);
	void UnmarkAllFunctions();

	void AppendOutLine(int nLineID,unsigned int uLineOffset);

	void Clear();
};


