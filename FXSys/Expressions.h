#pragma once

//#include "ExpVM.h"
//#include "ReferenceCreator.h"
//#include "QuickHashTable.h"

#include "Parser/ParseHandler.h"
#include <unordered_set>
#include <sstream>
#include <set>
#include <map>
#include <unordered_map>

#include "FXTypes.h"
#include "FXCode.h"
#include "FileHandler.h"
#include "ComValue.h"

//This file contains a SIMPLE stacked expression evaluator
//TODO:	Replace stack sequence parser with RPN parser
//		Optimize Calculate function, replace operators switch with FuncPtrs
//		Replace explicit values with referenced values

struct SComValue;



class CExpCompiler;
class CTokenMan;
class CExpParser;
class CExpErrors;
class CPreprocessor;
class CExpTag;

struct ID3D10Blob;


class CConstProvider
{
public:
	virtual bool GetConst(const char *sName,SComValue &ret,size_t uNamespace)=0;
};














#define DEF_ETN(name)	ETN_##name,
#define DEF_ETN_STR(name)	ETN_##name,
	enum EXP_TOKEN
	{
#include "Tokens.inc"
		ETN_SIZE
	};
#undef DEF_ETN
#undef DEF_ETN_STR

#define DEF_ERULE(name) ERULE_##name,
	enum EXP_RULE
	{
		ERULE_NONE=0,
#include "Rules.inc"
		ERULE_SIZE
	};
#undef DEF_ERULE


//Reference creator by it's name
class CExpCompiler:		public CParseHandler,public CFileHandler,CConstProvider
{
	typedef bool (* TAConstOpFunc[CV_SIZE])(SComValue *pA,SComValue *pB);
	typedef std::map<int,TAConstOpFunc> TMConstFunc;
	typedef std::shared_ptr<CExpTag> PExpTag;
	
	static size_t m_hInst;
	CFileHandler *m_pFileHandler;

	struct SPrimitiveType
	{
		std::string sName;
		CV_TYPE Type;
		char nDimsX,nDimsY;

		SPrimitiveType(const char *sname,int T,char dimsx,char dimsy=0):Type((CV_TYPE)T),nDimsX(dimsx),nDimsY(dimsy),
						sName(sname)
		{
		}

		SPrimitiveType():Type(CV_NULL),nDimsX(0),nDimsY(0)
		{
		}
	};

	struct SConstVector
	{
		SPrimitiveType *pType;
		SComValue aVals[4*4];	//For maximum matrix4x4 storage
	};

	static std::map<std::string,SPrimitiveType> m_mPrimTypes;
public:
	typedef struct{
					EXP_TOKEN T;
					int nLine;
					unsigned int uLineOffset;
					const char *sText;
					}TToken;
	typedef std::vector<TToken> TATokens;


	typedef std::set<EXP_TOKEN> TSTokens;
	struct STokenStream
	{
	struct CHash
	{
		size_t operator()(const char *val)const
		{
			return std::_Hash_array_representation(val,strlen(val)+1);
		}
	};
	struct CEqual
	{
		unsigned long operator()(const char *val0,const char *val1)const
		{
			return strcmp(val0,val1)==0;
		}
	};
	private:
		TATokens aTokens;
		int nCharBufPtr;
		std::unordered_set<const char *,CHash,CEqual> sTokenStrings;
		//CQuickHashTable<const char *,const char *> mTokenStrings;
		std::vector<char *> apCharBuffers;
		int nPos,nLastReadLine;


	public:
		STokenStream():nCharBufPtr(0),nPos(0),nLastReadLine(-1)
		{
		}
		STokenStream(TATokens &src)=delete;//:nCharBufPtr(0),nPos(0),nLastReadLine(-1)	//strings are not copied
/*		{
			aTokens=src;
		}*/
		~STokenStream();

		void reset();
		const char *storeString(const std::string &src);
		TToken &addToken();
		TToken *nextToken(bool bAdvancePos=true);
		void revert();
		
		int getTokens(TToken **pRetFirstT,const TSTokens &sTerminators,EXP_TOKEN *pRetTerminator=0);
		int getTokens(TToken **pRetFirstT,EXP_TOKEN Terminator);
		int getPos(){return nPos;}
		void setPos(int n){nPos=n;}

		static int getTokensNum(const TToken *aTokens,int nAllT,int nPos,EXP_TOKEN Terminator);
	
		bool eof(){return nPos>=(int)aTokens.size();}
		int getLastReadLine(){return nLastReadLine;}

		const TATokens &getTokens(){return aTokens;}
		void reduceTokens(int nNewCount){aTokens.resize(nNewCount);}
	};

	static TMConstFunc m_mOpFunc;


private:
	typedef std::unordered_map<std::string,EXP_RULE> TMNamedRules;

//	static TMMNamedOpsT m_mNamedOpsT;
//	static TMMNamedOps m_mNamedOps;
//	static TMMSortedOps m_mSortedOps;	//sorted by it's name string length
	static TMNamedRules m_mNamedRules;

	//typedef std::unordered_map<std::string,std::pair<int,TExpLine>> TMExpScopeVars;
	enum TYPE_QUALIFIERS_BIT
	{
		TQB_STATIC=0,
		TQB_CONST,
		TQB_VOLATILE,
		TQB_UNIFORM,

		TQB_SIZE
	};

	struct SScopeDesc
	{
		std::string sName;
		std::unordered_map<std::string,PExpTag> mTags;
		int nEnterSP;

		SScopeDesc(const char *sname,int sp):sName(sname),nEnterSP(sp)
		{
		}
	};
	std::vector<SScopeDesc> m_aScopes;
	int m_nScopeGID;

	struct SConstRecord
	{
		EXP_TOKEN Op;
		SComValue val;

		SConstRecord()
		{
		}

		SConstRecord(const SComValue &cv):val(cv),Op(ETN_NONE)
		{
		}
		SConstRecord(EXP_TOKEN T):Op(T)
		{
		}
	};
	typedef std::vector<SConstRecord> TARecords;
	typedef std::unordered_map<std::string,std::pair<std::string,SComValue>> TMPassConstValues;
	std::map<EXP_TOKEN,int> m_mOpPriority;

	CPreprocessor *m_pPreprocessor;
	CExpParser *m_pParser;
	STokenStream m_TStream;
	SFXCode *m_pOutStream;
	EXP_TOKEN m_tLastStopKeyword;
	
//Global vars initializers
	std::string m_sNewGlobalVar;
	int m_nCurrentGlobalVarInitDim;
	SPrimitiveType *m_pGlobalVarType;
	unsigned int m_uGlobalVarQualifier;
	std::vector<int> m_anVarSizes,m_anVarPointers;
	std::vector<SConstVector> m_aVarInitItems;
	int m_nCurrentVarAttr;
	std::set<TToken *> m_sStringConversionTokens;

//Samplers initializers
	std::string m_sNewSampler;
	int m_nCurrentSamplersInitDim;
	std::vector<int> m_anSamplersSizes,m_anSamplerPointers;
	std::vector<FX_SAMPLER> m_aNewSamplers;
	FX_SAMPLER m_NewSampler;

	std::pair<std::string,FX_RASTERIZER_DESC> m_NewRS;
	std::pair<std::string,FX_BLEND_DESC> m_NewBS;
	std::pair<std::string,FX_DEPTH_STENCIL_DESC> m_NewDSS;
	std::pair<std::string,SFXTech> m_NewTech;
	std::pair<std::string,SFXPassGroup> m_NewPass;
	std::pair<std::string,std::pair<int,int>> m_NewConstRange;
	TMPassConstValues m_NewPassConst;
	std::set<std::string> m_sDelayedReferences;
	SFXCode::SFuncDesc m_CurrentFunc;
	int m_nFirstAttributeOutLine;
	std::string m_sNewEnum;

	class CPassConstProvider:	public CConstProvider
	{
		CConstProvider *m_pParent;
		TMPassConstValues *m_pPassConstValues;

	public:
		virtual bool GetConst(const char *sName,SComValue &ret,size_t uNamespace) override
		{
			if (m_pParent->GetConst(sName,ret,uNamespace))
				return true;

			auto it=m_pPassConstValues->find(sName);
			if (it!=m_pPassConstValues->end())
			{
				ret=it->second.second;
				return true;
			}

			return false;
		}

		CPassConstProvider(CConstProvider *pParent,TMPassConstValues &rConstVals):m_pParent(pParent),m_pPassConstValues(&rConstVals)
		{
		};
	};
	std::unique_ptr<CPassConstProvider> m_pPassConstProvider;
	typedef std::pair<const char *,const char *> TMacroDefinition;


	int m_nLastErrorLine,m_nBlockErrorRuleLn;

	bool m_bErrorsEnabled;
	CExpErrors *m_pErrors;

	int m_nErrorsCnt,m_nNameGID,m_nLastReportedErrorLine;
	std::set<unsigned long> m_sWasErrors;
	std::vector<std::string> m_asErrors;
	std::vector<std::pair<TToken *,TToken *>> m_aIgnoreWriteTokens;


//	std::string m_sNewEnum;
	unsigned long m_uCurrentFileHash;
	
	
	std::string m_sLogFileName;

	void AddLocalStringConversions(TToken *aT,int nAllT);

	//bool IsLetter(char C,const char *sTermChars=0,int nAllTChars=0);
	//int GetString(const std::string &rsSrc,int nPos,std::string *psDest,const char *sTermChars=0);
	//int GetStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,char nSeparator=',',const char *sLevelBraces="()");
	
	//void SetChars(std::string &s,int pos0,int len,char C);

//	bool CreateEnum(const char *sName);
//	bool AddNewTag(PExpTag pTag);
	void ProcessExpression(TToken *aT,int nAllT);
	void MarkAttrReferences(TToken *aT,int nAllT);
	bool CustomAttr(TToken *aT,int nAllT);

	virtual int getNextRuleID(const std::string &sRuleName)override;
	virtual int getNextTokenID(const std::string &sStateName)override;

	bool hasRule(SExpRuleState* aStatesStack, int nAllS, EXP_RULE r);

	virtual void onSuccessRuleState(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT);
	virtual void onSuccessRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNumFirst,int nAllT);
	virtual bool onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNum,bool bProcessed);
	
	void blockOpen(bool bOpen,SExpRuleState *aStatesStack,int nAllS,int nLine);

	std::string getNextName(const char *sPrefix="");

	SScopeDesc *TopScope();

	int *FindEnumConst(const std::string &s,bool bThisScope);

	void replaceEscape(std::string &s);
	int WriteTokens(TToken *aTokens,int nAllT);
	void WriteString(const char *s,int nLine);

	void EndGlobalVar();
	void BeginGlobalVar(TToken *pToken);
	void SetTypeQualifier(TToken *pToken);
	void SetGlobalVarType(TToken *aTokens,int nAllT);
	void AddGlobalVarInitData(TToken *aTokens,int nAllT);
	//void SetGlobalVarZeroData(int nStartDimensionLevel);
	void AddNewGlobalVar();
	bool ReadConstVector(TToken *aTokens,int nAllT,SConstVector &rDest);

	void BeginSamplers(TToken *aTokens,int nAllT);
	void AddSampler(TToken *aTokens,int nAllT);
	void AddSamplers(TToken *aTokens,int nAllT);
	void SetSamplerVar(TToken *aTokens,int nAllT);

	void BeginRS(const char *sName);
	void SetRSVar(TToken *aTokens,int nAllT);
	void AddRS(TToken *aTokens,int nAllT);

	void BeginBS(const char *sName);
	void SetBSVar(TToken *aTokens,int nAllT);
	void AddBS(TToken *aTokens,int nAllT);

	void BeginDSS(const char *sName);
	void SetDSSVar(TToken *aTokens,int nAllT);
	void AddDSS(TToken *aTokens,int nAllT);

	void BeginTech(const char *sName);
	void AddTech(TToken *aTokens,int nAllT);

	void BeginPass(const char *sName);
	void AddPass(TToken *aTokens,int nAllT);

	void AddConstRange();
	void AddConst(TToken *aTokens,int nAllT);

	void SetPassRS(TToken *aTokens,int nAllT);
	void SetPassBS(TToken *aTokens,int nAllT);
	void SetPassDSS(TToken *aTokens,int nAllT);
	void SetPassShader(TToken *aTokens,int nAllT);

	unsigned int GetEnum(const std::string &sPrefix,const std::string &sName,bool *pbSuccess=0);
	SComValue GetComValue(const char *sVal);

	virtual bool GetConst(const char *sName,SComValue &ret,size_t uNamespace)override;

	bool EvaluteValue(TARecords &aRecords,int nPos,int nPrevPriority);
	bool ComputeOp(SComValue &a,SComValue b,EXP_TOKEN op);
	bool GetCommonMathType(const SComValue &a,const SComValue &b,CV_TYPE &ret);

	bool D3DCompile(const char *sSource,size_t sz,const char *sFileName,TMacroDefinition *apMacros,const char *sEntryPoint,
					const char *sShaderName,int nShaderVer,unsigned int uFlags,void *ppCode,void *ppErrorMsgs);
			
	bool CompilePassGroup(const char *sSourceName,SFXPassGroup &PG,unsigned int uFlags,const std::map<std::string,std::string> &mDefMacros);
	const char *GetShaderVer(int nShaderName,int ver);

	void InsertKeptDirectives();

	bool CreateEnum(const char *sName);
	bool AddNewTag(PExpTag pTag);
	bool AddNewEnumValue(TToken *aT,int nAllT);

	void CreatePrimitiveTypes();
protected:

	void OutputD3DCompilerErrors(const char *sSourceName,ID3D10Blob *pErrs,SFXCode::TSourceIDLine *anLineIDs,int nAllLines);
 
public:
	CExpCompiler(const char *sLogFileName=0);
	virtual ~CExpCompiler();

	bool Compile(const char *sFileName,const char *sDir,SFXCode &rDest,std::string *asDefs,int nAllDefs,unsigned int uFlags);

	void ErrorLink(SFXCode &rECode,TExpLine nLine,int err,const char *sParam0="",const char *sParam1="");
	void ErrorLn(int nLine,int err,const char *sParam0="",const char *sParam1="");
	void Error(int err,const char *sParam0="",const char *sParam1="");
	int CheckErrors(std::string *psRet=0);	


	void PushScope(const std::string &scope);
	bool PopScope();

	int GetErrorsCnt(){return m_nErrorsCnt;}
	CExpErrors *GetErrors(){return m_pErrors;}

	CPreprocessor *GetPreprocessor(){return m_pPreprocessor;}

	static void setHINSTANCE(size_t hInst){m_hInst=hInst;}	//Used to load language structure from embedded resources
	static size_t getHINSTANCE(){return m_hInst;}

	void ClearLog();
	void Log(const char *sFmt,...);
	const std::string &GetLogFileName(){return m_sLogFileName;}

	SComValue ParseConstExpression(TToken *aTokens,int nAllT,CConstProvider *pCProvider=0,const char *sNameSpace=0);

	void SetFileHandler(CFileHandler *pFH=0);
};





/*

template <typename T>
CQuickVectorSet<unsigned long> CExpCompiler<T>::m_sWasErrors;

template <typename T>
std::vector<std::string> CExpCompiler<T>::m_asErrors;

/*
template<typename T> T Calculate(CExpCompiler<T> *lpCreator,std::vector<SOperationCode> &Src)
{
 std::vector<T> Stack;
 short n,pos=0,pos2;
 T a,b,f;

 for (n=0;n<Src.size();n++)
 switch (Src[n].Operation)
 {
  case 0:Stack.push_back(((CExpReference *)Src[n].lpRef)->GetValue());break;
  case 1:Stack[Stack.size()-1]=-Stack[Stack.size()-1];break;
  case 2:
	  {
	   a=Stack[Stack.size()-2];
	   b=Stack[Stack.size()-1];
	   switch (Src[n].Data)
	   {
		case '+':f=a+b;break;
		case '-':f=a-b;break;
		case '*':f=a*b;break;
		case '/':f=a/b;break;
	   }
	   Stack.erase(Stack.end()-1);
	   Stack[Stack.size()-1]=f;
	  }break;
 }

 _ASSERTE(Stack.size()==1);

 return Stack.size()?Stack[0]:0;
}




*/