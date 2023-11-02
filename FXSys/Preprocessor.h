#pragma once

#include "Expressions.h"
#include "TokenGen/TokenHandler.h"

class CTokenMan;
class CFileHandler;

typedef std::vector<std::string> TAStrings;
typedef std::map<std::string,std::string> TMTextDefinitions;

class CPreprocessor:	public CTokenHandler
{
public:
#define PREP_PRAGMA(pm)	PP_##pm,
	enum PREP_PRAGMAS
	{
		PP_NONE=0,

#include "PrepPragmas.inc"

		PP_SIZE
	};
#undef PREP_PRAGMA

private:

typedef CExpCompiler::TATokens TATokens;
typedef CExpCompiler::TToken TToken;
typedef CExpCompiler::STokenStream STokenStream;

	CExpCompiler *m_pOwner;

#define PREP_KW(kw)	PKW_##kw,
	enum PREP_KEYWORDS
	{
		PKW_NONE=0,
#include "PrepKW.inc"
		PKW_SIZE
	};
#undef PREP_KW


	struct STokenDesc
	{
		EXP_TOKEN T;
		std::string sName;
		bool bString;

		STokenDesc():T(ETN_NONE),bString(false)
		{
		}
		STokenDesc(EXP_TOKEN t,bool bStr):T(t),bString(bStr)
		{
		}
	};

	struct SProcessData
	{
		int nLine;
		unsigned int uLineOffset;//Horizontal offset
		STokenStream *pStream;
		
		int nCurBlockingChar;													//Recursive macro protection
		std::vector<std::pair<std::pair<int,int>,std::string>> aBlockingRanges;	//

		SProcessData(STokenStream *pstream=0):nCurBlockingChar(0),pStream(pstream),nLine(0),uLineOffset(0)
		{
		}
	};

	class CBasicMacro
	{
	protected:
		CPreprocessor *m_pOwner;
		std::string m_sName;

	public:
		CBasicMacro(CPreprocessor *_pOwner=0,const char *_sName=0):m_pOwner(_pOwner),m_sName(_sName){};
		virtual ~CBasicMacro(){};

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)=0;
		const std::string &getName(){return m_sName;}
	};

	class CMacro:	public CBasicMacro
	{
	protected:
		int m_nArgsCount;
		bool m_bVarArgs;

		TATokens m_aTokens;
		std::vector<char> m_anTokenArg;
		std::vector<char> m_aStringsBuf;

	public:
		CMacro(CPreprocessor *_pOwner=0,const char *_sName="");
		CMacro(CPreprocessor *_pOwner,const char *_sName,const TToken *aT,int nAllT,const TAStrings &rasArgs,bool bVarArgs);
		CMacro(const CMacro &rSrc);
		CMacro &operator =(const CMacro &rSrc);
		virtual ~CMacro();

		void SetTokens(const TToken *aT,int nAllT);
		void SetArgs(const TAStrings &rasArgs);
		int GetArgsCount(){return m_nArgsCount-int(m_bVarArgs);}
		bool IsVarArgs(){return m_bVarArgs;}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest);
	};
	class CLineMacro:	public CBasicMacro
	{
	public:
		CLineMacro(CPreprocessor *_pOwner,const char *_sName):CBasicMacro(_pOwner,_sName){}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest);
	};
	class CFileMacro:	public CBasicMacro
	{
	public:
		CFileMacro(CPreprocessor *_pOwner,const char *_sName):CBasicMacro(_pOwner,_sName){}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest);
	};
	class CDateMacro:	public CBasicMacro
	{
	public:
		CDateMacro(CPreprocessor *_pOwner,const char *_sName):CBasicMacro(_pOwner,_sName){}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest);
	};
	class CTimeMacro:	public CBasicMacro
	{
	public:
		CTimeMacro(CPreprocessor *_pOwner,const char *_sName):CBasicMacro(_pOwner,_sName){}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest);
	};


	class CTextMacro:	public CBasicMacro
	{
		std::string m_sText;
	public:
		CTextMacro(CPreprocessor *_pOwner,const char *_sName,const char *sText):CBasicMacro(_pOwner,_sName),m_sText(sText){}

		virtual bool BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest){rsDest=m_sText;return true;}
	};


	struct SIfBlock
	{
	private:
		bool bEnabled,bWasToggle,bAllow,
			bAutoEnd;
	public:
		SIfBlock(bool en,bool allow,bool autoend=false):bEnabled(en),bAllow(allow && en),bWasToggle(false),bAutoEnd(autoend)
		{
		}

		void Disallow()
		{
			bAllow=false;
		}

		bool Toggle()
		{
			bool bRet=bWasToggle;
			
			bAllow=!bAllow && bEnabled;
			bWasToggle=true;
			
			return bRet;
		}

		bool IsAllow()
		{
			return bAllow;
		}

		bool IsAutoEnd()
		{
			return bAutoEnd;
		}
	};

	struct SPragmaScope
	{
		int nBeginToken,nEndToken;
		size_t uData[4];

		SPragmaScope(int nBegin=0):nBeginToken(nBegin),nEndToken(-1)
		{
			memset(uData,0,sizeof(uData));
		}
	};

	typedef std::unordered_map<std::string,EXP_TOKEN> TMNamedTokens;
	typedef std::unordered_map<std::string,PREP_KEYWORDS> TMNamedPrepKW;
	typedef std::unordered_map<std::string,PREP_PRAGMAS> TMNamedPrepPragmas;
	typedef std::unordered_map<std::string,std::unique_ptr<CBasicMacro>> TMMacros;
	typedef std::vector<SIfBlock> TAIfBlocks;
	typedef int TSourceLine;
	
	typedef std::vector<SPragmaScope> TAPragmaScopes;
	typedef std::pair<TSourceLine,TSourceLine> TSourceRange;
	typedef std::vector<TSourceRange> TASourceSectors;
	typedef std::pair<TSourceLine,std::string> TLineDirective;
	typedef std::vector<TLineDirective> TAKeepDirective;
	
	static TMNamedTokens m_mNamedTokens;
	static TMNamedPrepKW m_mNamedPrepKW;
	static std::string m_asPrepKWName[PKW_SIZE];
	static TMNamedPrepPragmas m_mNamedPrepPragmas;
	static STokenDesc m_aTokenDesc[ETN_SIZE];

	std::string m_asTokenComment[ETN_SIZE];
	TASourceSectors m_aSourceSectors;
	TAKeepDirective m_aKeepDirective;
	
	bool m_bIncludeAbs,m_bLastIf;
	std::string m_sAdditionalDir,m_sIncludeFile,m_sPragma;
	CTokenMan *m_pTMan,*m_pPrepLexer,*m_pMacroLexer;
	EXP_TOKEN m_tLastStopKeyword;
	CBasicMacro *m_pLastStopMacro;
	int m_nLastStopLine;
	TADependences m_aDependencies;
	TMMacros m_mMacros;
	TAIfBlocks m_aIfBlocks;
	TMTextDefinitions m_mDefinitions;
	TAPragmaScopes m_aaPragmaScopes[PP_SIZE];
	std::set<std::string> m_sOpenedFiles;

	virtual void logTMan(const char *sText,bool bError);
	virtual bool onSuccessToken(CTState *pTState,void *pUserData);
	virtual void onErrorToken(CTState *pTState,void *pUserData,char nErrChar);

	bool ProcessDefine(STokenStream &rSrc);
	bool ProcessUndef(STokenStream &rSrc);
	bool ProcessIf(STokenStream &rSrc,std::stringstream &rsStream,bool bAutoEnd=false);
	bool ProcessElif(STokenStream &rSrc,std::stringstream &rsStream);
	bool ProcessIfdef(STokenStream &rSrc,bool bTrue);
	bool ProcessEndif(STokenStream &rSrc);
	bool ProcessElse(STokenStream &rSrc);
	bool ProcessInclude(STokenStream &rSrc);
	bool ProcessPragma(STokenStream &rSrc);

	PREP_PRAGMAS GetPragma(const std::string &sSrc);
	bool ExecPragma(PREP_PRAGMAS PP,STokenStream &rSrcArgs,STokenStream &rOutStream);
	bool ProcessPragmaBeginEnd(PREP_PRAGMAS PP,STokenStream &rSrcArgs,STokenStream &rOutStream);

	bool ProcessKeyword(PREP_KEYWORDS PKW,STokenStream &rSrc,std::stringstream &rsStream);
	PREP_KEYWORDS GetKeyword(const char *str,std::string &rsRet);


	void StoreDirective(int nLine,PREP_KEYWORDS PKW,SProcessData *pDest);
	bool ReadPreprocessor(PREP_KEYWORDS PKW,std::istream *f,std::stringstream &rsStream,SProcessData *pDest);
	bool ReadMacroArgs(std::istream *f,std::stringstream &rsStream,SProcessData *pDest,int nArgsCount,bool bVarArgs);
	
	void GetArgs(STokenStream &rSrc,std::vector<TATokens> &raaDest);
	void ExpandArg(const TATokens &src,TATokens &dest);

	void AddTokensComments(CTokenMan *pTMan);

	void SkipFormatHdr(char *p);

	void InitDefaultMacros();

	int InsertMacro(SProcessData &PD,std::istream *f,std::stringstream &sStream);

	bool ParseIfExpression(TToken *aT,int nAllT,SComValue &rRet);

	SPragmaScope *AddPragmaScope(PREP_PRAGMAS PP,int nTokenNum,bool bBegin);
public:
	enum READ_FILE_RESULT
	{
		RFR_ERROR=0,
		RFR_OK,
		RFR_NOFILE
	};

	CPreprocessor(CExpCompiler *pOwner);
	~CPreprocessor();

	READ_FILE_RESULT ReadFile(const char *sFileName,const char *sDir,STokenStream &rDest,CFileHandler *pFH,int nLevel=0);
						//sDir==NULL - open sFileName from additional folders

	virtual int getNextStateID(const std::string &sStateName)override;
	void Reset();
		
	TMTextDefinitions &GetDefinitions(){return m_mDefinitions;}
	unsigned long GetDefinitionsHash();

	const TADependences &GetDep(){return m_aDependencies;}
	const std::string &GetTokenComment(EXP_TOKEN T);

	void SetAdditionalDir(const std::string &sDir);

	SPragmaScope *FindPragmaScope(PREP_PRAGMAS PP,int nTokenNum);
	const TASourceSectors &GetSourceSectors(){return m_aSourceSectors;}
	const TAKeepDirective &GetKeptDirectives(){return m_aKeepDirective;}
	
	static const STokenDesc &GetTokenDesc(EXP_TOKEN T);
	static std::string GetShortFileName(const std::string &sSrc,int nSlashes=2);
	static void decode(const char *sSrc,std::string &rsDest);
};