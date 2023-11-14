#include "stdafx.h"

#include <d3dcompiler.h>
#include "dxc/dxcapi.h"

#include "resource.h"

#include "Expressions.h"
#include "ExpTag.h"
#include "Preprocessor.h"
#include "ExpErrors.h"
#include "Parser/PState.h"
#include "TokenGen/TokenMan.h"
#include "TokenGen/TState.h"

#include "Parser/ExpParser.h"
#include "conststring.h"

#include <minmax.h>
#include <windows.h>
#include <algorithm>

using namespace fx;

#define COMPILER_VERSION 0

#define MUTE_GLOBAL_INITIALIZERS

#define EXPLOG_FILENAME "fxerr.log"

#define CONST_RANGE_LENGTH 5

#define CHAR_BUF_SIZE 8192

#define MIN_ID_POS -10000

size_t CExpCompiler::m_hInst=0;
CExpCompiler::TMNamedRules CExpCompiler::m_mNamedRules;
std::map<std::string,PBaseType> CExpCompiler::m_mPrimTypes;

CExpCompiler::TMConstFunc CExpCompiler::m_mOpFunc;



#define ConstFunc(op,type)								\
bool fnc_##op##_##type(SComValue *,SComValue *);		\
struct Sc##op##_##type{		\
	Sc##op##_##type(){		\
	if (CExpCompiler::m_mOpFunc.find(op)==CExpCompiler::m_mOpFunc.end())	\
		memset(CExpCompiler::m_mOpFunc[op],0,sizeof(CExpCompiler::m_mOpFunc[op]));	\
	CExpCompiler::m_mOpFunc[op][type]=fnc_##op##_##type;}	\
}g_c##op##_##type;			\
bool fnc_##op##_##type(SComValue *pA,SComValue *pB)		\



unsigned int CExpCompiler::SConstVector::getAllVals()
{
	_ASSERTE(pType);
	return (unsigned int)(max(pType->GetDimsX(),1)*max(pType->GetDimsY(),1));
}

void CExpCompiler::SConstVector::addMissingData()
{
	if (pType)
	{
		unsigned int uMaxVals=getAllVals();

		if (!aVals.size())
			aVals.push_back(0.0f);

		while (aVals.size()<uMaxVals)
			aVals.push_back(aVals.back());
	}
}

CExpCompiler::STokenStream::~STokenStream()
{
	for (char *ptr:	apCharBuffers)
		free(ptr);
}

void CExpCompiler::STokenStream::reset()
{
	nLastReadLine=-1;
	nPos=0;
	aTokens.clear();
	nCharBufPtr=0;
//	mTokenStrings.clear();
	sTokenStrings.clear();
}

#define DD(prefix,val,str)	{std::string s(#prefix"_"#str);_strlwr_s((char *)s.c_str(),s.length()+1);_ASSERTE(mDict.find(s)==mDict.end());mDict[s]=val;}

unsigned int CExpCompiler::GetEnum(const std::string &sPrefix,const std::string &sName,bool *pbSuccess)
{
	static std::map<std::string,unsigned int> mDict;

	if (!mDict.size())
	{
		DD(,true,true);
		DD(,false,false);

#define BEGIN_FX_ENUM(ename)
#define END_FX_ENUM
#define FX_ENUM(prefix,name,val)		DD(prefix,prefix##_##name,name)

#include "FXEnums.inc"

#undef BEGIN_FX_ENUM
#undef END_FX_ENUM
#undef FX_ENUM
	}

	std::string sPrefixName=sPrefix+"_"+sName;
	_strlwr_s((char *)sPrefixName.c_str(),sPrefixName.length()+1);
	auto it=mDict.find(sPrefixName);


	if (it!=mDict.end())
	{
		if (pbSuccess)
			*pbSuccess=true;

		return it->second;
	}
	else
	{
		int *pVal=0;
		if (sPrefix.length())
			return GetEnum("",sName,pbSuccess);

		if (pVal=FindEnumConst(sName,true))
		{
			if (pbSuccess)
				*pbSuccess=true;

			return *pVal;
		}

		if (pbSuccess)
			*pbSuccess=false;
		else
			Error(EERR_UNDEFINED_ID,sName.c_str());

		return 0;
	}
}

SComValue CExpCompiler::GetComValue(const char *sVal)
{
	SComValue ret;

	if (!strcmp(sVal,"NULL"))
	{
		ret.uType=CV_NULL;
		ret.nData=0;
	}
	else
	if (sVal[0]=='"')
	{
		ret.uType=CV_NULL;
		ret.nData=0;
	}
	else
	if (sVal[0]=='\'')
	{
		ret.uType=CV_NULL;
		ret.nData=0;
	}
	else
	if (strchr(sVal,'.') || strchr(sVal,'e'))
	{
		if (strchr(sVal,'f'))
			ret=(float)atof(sVal);
		else
			ret=(double)atof(sVal);
	}
	else
	{
		size_t len=strlen(sVal);

		if (len>1 && sVal[0]=='0' && sVal[1]=='x')
			ret=(unsigned int)_strtoui64(sVal,0,0);
		else
		if (len>1 && sVal[0]=='0' && sVal[1]=='b')
			ret=(unsigned char)_strtoui64(sVal+2,0,2);
		else
			ret=(int)strtol(sVal,0,0);
	}

	return ret;
}



const char *CExpCompiler::STokenStream::storeString(const std::string &src)
{
	auto it=sTokenStrings.find(src.c_str());

	_ASSERTE(src.length()<CHAR_BUF_SIZE);
	if (src.length()>=CHAR_BUF_SIZE)
		return 0;

	if (it!=sTokenStrings.end())
		return *it;
	else
	{
		int nBufID=nCharBufPtr>>16;
		int nPtr=nCharBufPtr & 0xFFFF;

		if (src.length()+nPtr>=CHAR_BUF_SIZE)
		{
			nBufID++;
			nPtr=0;
		}
		if (apCharBuffers.size()<=(size_t)nBufID)
			apCharBuffers.push_back((char *)malloc(CHAR_BUF_SIZE));
		
		char *sRet=apCharBuffers[nBufID]+nPtr;
		strcpy_s(sRet,CHAR_BUF_SIZE-nPtr,src.c_str());

		nCharBufPtr=(nBufID<<16) | (nPtr+(int)src.length()+1);

		auto iit=sTokenStrings.insert(sRet);
		_ASSERTE(iit.second);
		return sRet;
	}
}

CExpCompiler::TToken &CExpCompiler::STokenStream::addToken()
{
	aTokens.resize(aTokens.size()+1);
	return aTokens.back();
}

void CExpCompiler::STokenStream::revert()
{
	nPos=max(nPos-1,0);
}
CExpCompiler::TToken *CExpCompiler::STokenStream::nextToken(bool bAdvancePos)
{
	int n=nPos;
	if (nPos<(int)aTokens.size())
	{
		if (bAdvancePos)
			nPos++;

		nLastReadLine=aTokens[n].nLine;
		return &aTokens[n];
	}

	return 0;
}

int CExpCompiler::STokenStream::getTokens(TToken **pRetFirstT,const TSTokens &sTerminators,EXP_TOKEN *pRetTerminator)
{
	int nPos0=nPos;
	TToken *pT;

	if (pRetFirstT)
		*pRetFirstT=&aTokens[nPos];

	do
	{
		pT=nextToken(true);
	}while (pT && pT->T!=ETN_NONE && sTerminators.find(pT->T)==sTerminators.end());
	
	if (pT && pRetTerminator)
		*pRetTerminator=pT->T;

	return max(nPos-nPos0-1,0);
}

int CExpCompiler::STokenStream::getTokens(TToken **pRetFirstT,EXP_TOKEN Terminator)
{
	int nPos0=nPos;
	int nLevel=0;
	TToken *pT=0;

	if (pRetFirstT)
		*pRetFirstT=&aTokens[nPos];

	do
	{
		if (Terminator==ETN_BRACEC_CLOSE)
		{
			if (pT && pT->T==ETN_BRACEC_OPEN)
				nLevel++;
			if (pT && pT->T==ETN_BRACEC_CLOSE)
				nLevel--;
		}

		pT=nextToken(true);
	}while (pT && pT->T!=ETN_NONE && ((pT->T!=Terminator && pT->T!=ETN_SEMICOLON) || nLevel));
	
	return max(nPos-nPos0-1,0);
}

int CExpCompiler::STokenStream::getTokensNum(const TToken *aTokens,int nAllT,int nPos,EXP_TOKEN Terminator)
{
	int nLevel=0;
	int n;
	for (n=nPos;n<nAllT && (aTokens[n].T!=Terminator || nLevel);++n)
		if (aTokens[n].T==ETN_BRACEC_OPEN)
			nLevel++;
		else
		if (aTokens[n].T==ETN_BRACEC_CLOSE)
			nLevel--;

	return n-nPos;
}

















#include "rules.inl"

#define NEXT_PRIORITY nPriority++
#define DEF_OP(op) m_mOpPriority[op]=nPriority


CExpCompiler::CExpCompiler(const char *sLogFileName)
{
	m_pFileHandler=this;
	m_uCurrentFileHash=0;
	m_pErrors=new CExpErrors();
	m_bErrorsEnabled=true;
	m_nBlockErrorRuleLn=-1;
	
	m_pPassConstProvider.reset(new CPassConstProvider(this,m_NewPassConst));

	//m_RootArrayItem.pChild=std::make_shared<SArrayItems>();
	m_nNameGID=0;
	m_pOutStream=0;
	m_nLastErrorLine=0;	
	m_nScopeGID=0;

	int nPriority=0;

	DEF_OP(ETN_OP_LOR);
	DEF_OP(ETN_OP_LAND);

	NEXT_PRIORITY;
	DEF_OP(ETN_OP_GATE);
	DEF_OP(ETN_OP_LESS);
	DEF_OP(ETN_OP_EQUAL);
	DEF_OP(ETN_OP_NOTEQUAL);
	DEF_OP(ETN_OP_LESSEQ);
	DEF_OP(ETN_OP_GATEEQ);

	NEXT_PRIORITY;
	DEF_OP(ETN_OP_ASR);
	DEF_OP(ETN_OP_ASL);

	NEXT_PRIORITY;
	DEF_OP(ETN_OP_PLUS);
	DEF_OP(ETN_OP_MINUS);
	DEF_OP(ETN_OP_AND);
	DEF_OP(ETN_OP_XOR);
	DEF_OP(ETN_OP_OR);

	NEXT_PRIORITY;
	DEF_OP(ETN_OP_MUL);
	DEF_OP(ETN_OP_DIV);
	DEF_OP(ETN_OP_MOD);

	NEXT_PRIORITY;
	DEF_OP(ETN_OP_NOT);
	DEF_OP(ETN_OP_BITNOT);


	if (sLogFileName)
		m_sLogFileName=sLogFileName;
	else
		m_sLogFileName=EXPLOG_FILENAME;


	if (!m_mNamedRules.size())
	{
#define DEF_ERULE(name)	m_mNamedRules[#name]=ERULE_##name;//m_aTokenDesc[ETN_##name].sName="ETN_"#name;
		#include "Rules.inc"
#undef DEF_ERULE
	}

	


	m_pPreprocessor=new CPreprocessor(this);

	m_pParser=0;

	std::string sTemp;
	const char *sText;
	HRSRC hRes=FindResource((HMODULE)m_hInst,MAKEINTRESOURCE(IDR_RULES),"TEXT");
//	_ASSERTE(hRes);
	if (hRes)
		sText=(char *)LockResource(LoadResource((HMODULE)m_hInst,hRes));
	else
	{
		CPreprocessor::decode(g_srules,sTemp);
		sText=sTemp.c_str();
	}
	m_pParser=new CExpParser(this,sText);


	_ASSERTE(m_pParser);

	PushScope("Global");
}


CExpCompiler::~CExpCompiler()
{
	delete m_pErrors;
	delete m_pPreprocessor;

	if (m_pParser)
		delete m_pParser;
}





void CExpCompiler::PushScope(const std::string &scope)
{
	std::string sScope=scope;

	if (!sScope.length())
	{
		char p[8];
		_itoa_s(m_nScopeGID++,p,10);
		sScope="_";
		sScope+=p;
	}

	m_aScopes.push_back(SScopeDesc(sScope.c_str(),0));
	
	//m_sScope+=sScope;
	//m_sScope+='@';
}
bool CExpCompiler::PopScope()
{
	_ASSERTE(/*m_sScope.length() && */m_aScopes.size()>1);

	if (m_aScopes.size()>1)// && m_sScope.length())
	{
		/*if (m_sScope.length()>1)
			m_sScope.resize(m_sScope.rfind('@',m_sScope.length()-2)+1);
		else
			m_sScope="";
			*/
		//m_nSP=m_aScopes.back().nEnterSP;
		m_aScopes.pop_back();

		return true;
	}
	
	return false;
}



void CExpCompiler::ErrorLn(int nLine,int e,const char *sParam0,const char *sParam1)
{
	if (!m_bErrorsEnabled || m_nBlockErrorRuleLn>=0)
		return;

	char p[2048]="";

	if (nLine>=0)
	{
		std::string sFN="??";

		if (m_pPreprocessor->GetDep().size()>(size_t)(nLine>>24 & 0xFF))
			sFN=CPreprocessor::GetShortFileName(m_pPreprocessor->GetDep()[(nLine>>24 & 0xFF)].sFileName);

		if (nLine)
			sprintf_s(p,"%s (%i): ",sFN.c_str(),nLine & 0xFFFFFF);
		else
			sprintf_s(p,"%s: ",sFN.c_str());
	}

	strcat_s(p,m_pErrors->formatError((EXP_ERRORS)e,sParam0,sParam1).c_str());	
	m_nErrorsCnt++;

	conststring err(p);
	if (m_sWasErrors.insert((unsigned long)err.hash()).second)
	{
		m_asErrors.push_back(err);
		OutputDebugStringA(p);
		OutputDebugStringA("\n");
	}

	m_nBlockErrorRuleLn=nLine;
}

void CExpCompiler::Error(int err,const char *sParam0,const char *sParam1)
{
	ErrorLn(m_nLastErrorLine,err,sParam0,sParam1);
}


/*
bool CExpCompiler::IsLetter(char C,const char *sTermChars,int nAllTChars)
{
	if (!sTermChars || !nAllTChars)
		return (C>='A' && C<='Z') || (C>='a' && C<='z') || (C>='0' && C<='9') || C=='_';
	else
	{
		int n;
		for (n=0;n<nAllTChars && C!=sTermChars[n];++n);

		return n>=nAllTChars;
	}
}

char charLower(char C)
{
	char S[2]={C,0};
	_strlwr_s(S);
	return S[0];
}
/*
int CExpCompiler::GetString(const std::string &rsSrc,int nPos,std::string *psDest,const char *sTermChars)
{
	int n;
	int nAllTChars=(sTermChars?(int)strlen(sTermChars):0);

	if (psDest)
		*psDest="";
	for (n=nPos;n<(int)rsSrc.length() && IsLetter(rsSrc[n],sTermChars,nAllTChars);++n)
		if (psDest)
			*psDest+=rsSrc[n];

	return n;
}

int CExpCompiler::GetStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,char nSeparator,const char *sBraces)
{
	int n,m,nLevel=0;
	bool bStr=false;
	int sz=(int)strlen(sBraces);

	if (psDest)
		*psDest="";

	for (n=nPos;n<(int)rsSrc.length() && (nLevel || bStr || rsSrc[n]!=nSeparator);++n)
	{
		char C=rsSrc[n];

		if (C=='"')
			bStr=!bStr;

		if (!bStr)
		{
			for (m=0;m<sz;m+=2)
			if (C==sBraces[m])
				nLevel++;

			for (m=1;m<sz;m+=2)
			if (C==sBraces[m])
				nLevel--;
		}

		if (psDest)
			(*psDest)+=C;
	}

	return n;
}
*/
/*
void CExpCompiler::SetChars(std::string &s,int pos0,int len,char C)
{
	char *p=(char *)s.c_str()+pos0;
	
	while (len>0)
	{
		*(p++)=C;
		len--;
	}
}

std::string CExpCompiler::GetFileSpaceName(const char *sName)
{
	char p[16];
	std::string sRet(sName);

	_ASSERTE(m_uCurrentFileHash);
	_itoa_s(m_uCurrentFileHash,p,16);
	sRet+="@";
	sRet+=p;

	return sRet;
}
*/




void CExpCompiler::ClearLog()
{
	if (m_sLogFileName.length())
	{
		FILE *f=0;
		fopen_s(&f,m_sLogFileName.c_str(),"w");
		if (f)
			fclose(f);
	}
}

void CExpCompiler::Log(const char *sFmt,...)
{
	if (m_sLogFileName.length())
	{
		FILE *f=0;
		fopen_s(&f,m_sLogFileName.c_str(),"a");
		if (f)
		{
			va_list argptr;
			va_start(argptr, sFmt);
		
			size_t d0=(size_t)va_arg(argptr,char *),
					d1=(size_t)va_arg(argptr,char *),
					d2=(size_t)va_arg(argptr,char *),
					d3=(size_t)va_arg(argptr,char *);
		
			fprintf(f,sFmt,		d0,d1,d2,d3);

			va_end(argptr);
			fclose(f);
		}
	}
}

int CExpCompiler::CheckErrors(std::string *psRet)
{
	int nRet=(int)m_asErrors.size();

	if (m_asErrors.size())
	{
		if (psRet)
			*psRet="";

		for (std::string &sErr:	m_asErrors)
		{
			Log("%s\n",sErr.c_str());

			if (psRet)
			{
				*psRet+=sErr;
				*psRet+="\n";
			}
		}

		m_asErrors.clear();
		m_sWasErrors.clear();
	}

	return nRet;
}











/*bool CExpCompiler::ReadFile(const char *sFileName)
{
	bool bRet=false;
	m_TStream.reset();

	FILE *f=fopen(sFileName,"r");
	if (f)
	{
		std::stringstream sStream;
		int nErrCnt=m_nErrorsCnt;
		size_t n,sz;
		int nLine=0;
		char p[1024];

		bRet=true;
		m_pTMan->reset();

		while (!feof(f) && bRet)
		{
			if (!fgets(p,sizeof(p),f))
				p[0]=0;
			sStream.str(p);
			sStream.clear();
			nLine++;

			char C;
			while ((C=sStream.get()) && !sStream.eof())
			{
				CTokenMan::PROCESS_RESULT pRes=m_pTMan->process(C,nLine);
				
				if (!pRes)
				{
					bRet=false;
					break;
				}

				if (pRes==CTokenMan::PRES_INTERRUPTED_UNPROC)
					sStream.unget();

				if (pRes==CTokenMan::PRES_INTERRUPTED_PROC || pRes==CTokenMan::PRES_INTERRUPTED_UNPROC)
				{
					if (m_tLastStopKeyword>ETN_PKW_FIRST && m_tLastStopKeyword>ETN_PKW_LAST)
					{
						if (p[0]!='#')
							Error(EERR_PREP_START);
						else
						{
							TATokens aPrepTokens;

							if (!ReadPreprocessor(f,sStream,aPrepTokens))
							{
								bRet=false;
								break;
							}
						}
					}

					m_tLastStopKeyword=ETN_NONE;
				}
			}

			if (feof(f))
				m_pTMan->process(0,nLine);

		}

		fclose(f);
		TToken &rT=m_TStream.addToken();
		rT.T=ETN_NONE;
		rT.sText=0;
		rT.nLine=nLine;

		bRet&=nErrCnt==m_nErrorsCnt;
	}

	return bRet;
}
*/






bool CExpCompiler::Compile(const char *sFileName,const char *_sDir,SFXCode &rDest,std::string *asDefs,int nAllDefs,
						unsigned int uFlags)
{
	std::string sDir(_sDir?_sDir:"");
	
	CreatePrimitiveTypes();

	m_anIDDimSizes.clear();
	m_sNewGlobalID="";
	m_pGlobalIDType=0;
	m_pGlobalIDBaseType=0;
	m_sCurrentStructName="";
	m_tLastStopKeyword=ETN_NONE;
	m_nLastErrorLine=0;
	m_nBlockErrorRuleLn=-1;
	m_nErrorsCnt=0;
	//m_sScope="";
	m_aScopes.clear();
	PushScope("Global");
	m_uCurrentFileHash=(unsigned long)conststring(sFileName).hash();
	m_sDelayedReferences.clear();
	m_CurrentFunc.Reset();
	m_nFirstAttributeOutLine=-1;
	m_aIgnoreWriteTokens.clear();
	m_uGlobalVarQualifier=0;
	m_nCurrentVarAttr=0;
	m_sStringConversionTokens.clear();
	//m_asErrors.clear();
	//m_sWasErrors.clear();	

	rDest.Clear();
	m_pOutStream=&rDest;
	
	int nErrCnt=m_nErrorsCnt;

	if (sDir.length() && sDir[sDir.length()-1]!='\\')
		sDir+='\\';
	

	m_pPreprocessor->Reset();

//Macros from command-line
	for (int n=0;n<nAllDefs;++n)
	{
		std::string sName=asDefs[n];
		std::string sVal;
		size_t pos=sName.find('=');

		if (pos!=-1)
		{
			sVal=sName.substr(pos+1);
			sName.resize(pos);
		}

		m_pPreprocessor->GetDefinitions().emplace(sName,sVal);
	}

	CPreprocessor::READ_FILE_RESULT RRes;
	if ((RRes=m_pPreprocessor->ReadFile(sFileName,_sDir?sDir.c_str():0,m_TStream,m_pFileHandler))!=CPreprocessor::RFR_OK)
	{
		if (RRes==CPreprocessor::RFR_NOFILE)
			ErrorLn(-1,EERR_NO_FILE,sFileName);

		return false;
	}

	/*
	OutputDebugString("Source sectors:\n");
	auto &SS=m_pPreprocessor->GetSourceSectors();
	for (auto &pair:	SS)
	{
		char p[256];
		
		sprintf_s(p,"%s: %i-%i\n",m_pPreprocessor->GetDep()[pair.first>>24].sFileName.c_str(),pair.first & 0xFFFFFF,pair.second & 0xFFFFFF);
		OutputDebugString(p);
	}
	*/
	


	const std::string &sBaseFileName=m_pPreprocessor->GetDep()[0].sFileName;
	int nDriveDirLen=(int)sBaseFileName.find('\\');
	
	//_ASSERTE(nBaseDirLen!=-1);

	rDest.Header.aDependences=m_pPreprocessor->GetDep();
	rDest.Header.uDefinitionsHash=m_pPreprocessor->GetDefinitionsHash();

	rDest.Header.sName=sFileName;

	for (size_t n=0;n<rDest.Header.aDependences.size();++n)
	//if (n)
	{
		std::string &sFN=rDest.Header.aDependences[n].sFileName;

		if (nDriveDirLen==-1 || (sFN.length()>(size_t)nDriveDirLen && !strncmp(sFN.c_str(),sBaseFileName.c_str(),nDriveDirLen)))
		{
			sFN=SCodeDependence::ConvertFileNameRelative(sBaseFileName,sFN);
			rDest.Header.aDependences[n].bRelPath=true;
		}
	}
	
	
	std::vector<int> anTokens;
	anTokens.resize(m_TStream.getTokens().size());
	for (size_t n=0;n<anTokens.size();++n)
		anTokens[n]=m_TStream.getTokens()[(int)n].T;

	

	_ASSERTE(m_pParser);
	if (anTokens.size())
	{
		m_pParser->process(&anTokens[0],(int)anTokens.size());

		rDest.bCompiled=(nErrCnt==m_nErrorsCnt);

		if (rDest.bCompiled)
		{
			InsertKeptDirectives();

			std::string sName=sFileName,sSourceName;
			bool bRes=true;
			std::map<std::string,std::string> mDefaultMacros;

			sName=sName.substr((int)sName.rfind('\\')+1);
			if ((int)sName.rfind('.')!=-1)
				sName.resize(sName.rfind('.'));


		//Fill-in default macros
			for (auto &pair:	m_pOutStream->mTech)
			for (SFXPassGroup &PG:	pair.second.aPassG)
			{
				for (auto &pair:	PG.mConstParam)
				if (mDefaultMacros.find(pair.first)==mDefaultMacros.end())
					mDefaultMacros[pair.first]=pair.second;

				for (auto &pair:	PG.mParamRange)
				if (mDefaultMacros.find(pair.first)==mDefaultMacros.end())
				{
					char p[32];
					_itoa_s(pair.second.nMin,p,10);
					mDefaultMacros[pair.first]=p;
				}
			}

		//Default macros from enums
			for (auto &pair:	m_pOutStream->mDefinitions)
				mDefaultMacros[pair.first]=pair.second;

			for (auto &pair:	m_aScopes[0].mTags)
				mDefaultMacros[pair.first]="int";
			
			for (auto &pair:	m_pOutStream->mTech)
			{	
				for (SFXPassGroup &PG:	pair.second.aPassG)
				{
					int nErrCnt=m_nErrorsCnt;
					
					bRes=CompilePassGroup(sName.c_str(),PG,uFlags,mDefaultMacros);

					if (nErrCnt!=m_nErrorsCnt)
					{
						ErrorLn(0,EERR_TECH,pair.second.sName.c_str());
						m_nBlockErrorRuleLn=-1;
						m_sWasErrors.clear();
					}

					if (!bRes)
						break;
				}

				if (!bRes)
					break;
			}

			rDest.bCompiled=bRes;
		}
	}


	//rDest.bCompiled=(nErrCnt==m_nErrorsCnt);

	return rDest.bCompiled;
}

void CExpCompiler::OutputD3DCompilerErrors(const char *sSourceName_,ID3D10Blob *pErr,SFXCode::TSourceIDLine *anLineIDs,int nAllLines)
{
	std::string sSourceName=sSourceName_;
	std::string sSrcErr=(char *)pErr->GetBufferPointer();
	int nPos=0;
	std::string sLine;
	char sNum[1024];

	_ASSERTE(nAllLines && anLineIDs);//m_pOutStream->aOutLines.size());

	do
	{
		int pos=(int)sSrcErr.find('\n',nPos)+1;

		if (pos)
			sLine=sSrcErr.substr(nPos,pos-nPos-1);
		else
		{
			sLine=sSrcErr.substr(nPos);
			pos=(int)sSrcErr.length();
		}

		if (sLine.length())
		{
			int p=(int)sLine.find(sSourceName)+1,pp;

			if (p)
			{
				p=(int)sLine.find('(',p)+1;

				if (p)
				{				
					pp=(int)sLine.find(',',p);
						
					strncpy_s(sNum,sLine.c_str()+p,pp-p);
					sNum[pp-p]=0;

					sLine=sLine.substr(sLine.find(':',pp)+2);

					int nLine=atoi(sNum);
			
					if (nLine>0)
					{
						//ErrorLn(std::get<0>(m_pOutStream->aOutLines[nLine-1]),EERR_RAWTEXT,sLine.c_str());
						_ASSERTE(nLine<=nAllLines);
						if (nLine<=nAllLines)
							ErrorLn(anLineIDs[nLine-1],EERR_RAWTEXT,sLine.c_str());
					}
					else
					{
						sLine="\t"+sLine;
						ErrorLn(-1,EERR_RAWTEXT,sLine.c_str());
					}
				}
				else
					ErrorLn(0,EERR_RAWTEXT,sLine.c_str());
			}
			else
				ErrorLn(0,EERR_RAWTEXT,sLine.c_str());
		}

		m_nBlockErrorRuleLn=-1;
		nPos=pos;
	}while (nPos<(int)sSrcErr.length());
}



void CExpCompiler::blockOpen(bool bOpen,SExpRuleState *aStatesStack,int nAllS,int nLine)
{
	if (bOpen)
	{		
		/*if (m_sDefinition.length())	//func body
		{
			auto it=m_pOutStream->mFuncDesc.find(m_sDefinition);

			m_pCurrentFunc=0;

			_ASSERTE(it!=m_pOutStream->mFuncDesc.end());
			PushScope(m_sDefinition);

			
			m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_SP_STORE_LEVEL),0,nLine));

			if (it!=m_pOutStream->mFuncDesc.end())
			{
				SExpFuncDesc &rFD=it->second;
				rFD.nEntry=m_nFuncJumpPos+1;
				rFD.nDefinitionLine=nLine;

				m_pCurrentFunc=&rFD;

				for (int n=0;n<rFD.asArgs.size();++n)
				{
					if (FindEnumConst(rFD.asArgs[n],false) || 
						m_pOutStream->mFuncDesc.find(rFD.asArgs[n])!=m_pOutStream->mFuncDesc.end() || IsReference(ERC_GLOBAL,rFD.asArgs[n].c_str()))
							Error(EERR_REDEFINITION,rFD.asArgs[n].c_str());

					m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_LD_STACK),-(-2-n*2),nLine));
					m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_REFERENCE),
						(size_t)CreateReference(ERC_LOCAL,rFD.asArgs[n].c_str()),nLine));
					m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_ASSIGN),2,nLine));
					AddNewID(rFD.asArgs[n].c_str(),0,m_nLastErrorLine);
				}
			}

			GetCodeBlock()->sFunction=m_sDefinition;
			m_sDefinition="";
		}
		else //block
		*/
		{
			PushScope(m_CurrentFunc.sName);//getNextName("block"));
		}		
	}
	else
	{		
		std::string sLastName=m_aScopes.size()?m_aScopes.back().sName:"";

		if (!PopScope())
			Error(EERR_UNEXPECTED_CHAR,(char *)'}');
		/*else
		{
			bool bReturned=aStatesStack[nAllS-1].auParam[3]!=0;
			if (!bReturned)
			{
				if (m_pOutStream->aOps.size() && m_pOutStream->aOps.back().pOpDesc->Op==EOP_SP_STORE)
					m_pOutStream->aOps.pop_back();
				else
					m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_SP_RESTORE),0,nLine));
			}

			if (m_aScopes.size()==1 && m_nFuncJumpPos>=0) //func body end
			{
				if (!bReturned)
				{
				//	if (m_pOutStream->aOps.back().pOpDesc->Op!=EOP_RET)
						m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_RET),0,nLine));
				}

				m_pOutStream->aOps[m_nFuncJumpPos].uData=(m_pOutStream->aOps.size()-m_nFuncJumpPos-1)<<16;
				m_nFuncJumpPos=-1;

				if (m_pCurrentFunc)				
					m_pCurrentFunc->nSize=(int)m_pOutStream->aOps.size()-m_pCurrentFunc->nEntry;

				_ASSERTE(nAllS>1 && aStatesStack[nAllS-2].pRule->getID()==ERULE_func_def);
				if (aStatesStack[nAllS-2].auParam[0] && !aStatesStack[nAllS-1].auParam[3])
					Error(EERR_NOT_ALL_RETURN,sLastName.c_str());

				m_pCurrentFunc=0;
			}
			else	//block end
			{	
				if (bReturned && nAllS>1)
					propagateReturn(aStatesStack,nAllS-2);

				if (m_aBreaksCont.size())
				{
					m_aBreaksCont.back().nBlockNests--;
					_ASSERTE(m_aBreaksCont.back().nBlockNests>=0);
				}
			}
		}
		*/
	}
}




CExpCompiler::SScopeDesc *CExpCompiler::TopScope()
{
	return &m_aScopes.back();
}


std::string CExpCompiler::getNextName(const char *sPrefix)
{
	char p[64];
	sprintf_s(p,"@%s_%i",sPrefix,m_nNameGID);
	m_nNameGID++;

	return p;
}


int CExpCompiler::getNextTokenID(const std::string &sStateName)
{
	int nRet=m_pPreprocessor->getNextStateID(sStateName);
	_ASSERTE(nRet<ETN_SIZE);

	return nRet;
}

int CExpCompiler::getNextRuleID(const std::string &sRuleName)
{
	if (sRuleName.length())
	{
		auto it=m_mNamedRules.find(sRuleName);
		if (it!=m_mNamedRules.end())
			return it->second;

		_ASSERTE(false);
	}

	return __super::getNextRuleID(sRuleName)+ERULE_SIZE;
}

bool IsTextToken(EXP_TOKEN T)
{
	return (T>ETN_KW_FIRST && T<ETN_KW_LAST) || T==ETN_ID || (T>ETN_CONST_FIRST && T<ETN_CONST_LAST);
}

bool IsOpToken(EXP_TOKEN T)
{
	return T>=ETN_OP_PLUS && T<ETN_OP_ASR;
}

void CExpCompiler::replaceEscape(std::string &s)
{
	static const char anPattern[][2]={{'n','\n'},									
									{'t','\t'},
									{'"','"'},
									{'r','\r'},
									{'\\','\\'},
									{'\'','\''},
									{'a','a'},
									{'b','b'},
									{'f','f'},
									{'?','?'},	
									{'v','v'}};	
	int n,p,pos=0;

	do{
		p=(int)s.find('\\',pos);

		if (p!=-1)
		{
			char C=s[p+1];

			if ((C>='0' && C<='9') || C=='x')
			{
				size_t m;
				char pp[8]={0};

				for (m=p+1;m<s.length() && (int)m<p+5 && ((s[m]>='0' && s[m]<='9') || (s[m]>='a' && s[m]<='f') ||
													(m==p+1 && s[m]=='x'));++m)
					pp[m-p-1]=s[m];
				pp[m-p-1]=0;
				if (pp[0]=='x')
					pp[0]='0';
				
				s.erase(p,m-p-1);
				C=(char)strtol(pp,0,C=='x'?16:0);
				s[p]=C;
				pos=p+1;
			}
			else
			{
				for (n=0;n<_countof(anPattern) && anPattern[n][0]!=C;++n);

				if (n<_countof(anPattern))
				{
					s[p]=anPattern[n][1];
					s.erase(p+1,1);
					pos=p+1;
				}
				else
					pos=p+2;
			}
		}
	}while (p>=0 && pos<(int)s.length());
}


int CExpCompiler::WriteTokens(TToken *aTokens,int nAllT)
{
	int nFirstEmptyLines=0;
	std::string sString;
	
	for (int n=0;n<nAllT;++n)
	{
		TToken &rT=aTokens[n];

		while (m_aIgnoreWriteTokens.size() && &rT>m_aIgnoreWriteTokens.begin()->second)
			m_aIgnoreWriteTokens.erase(m_aIgnoreWriteTokens.begin());

		if (!m_aIgnoreWriteTokens.size() || &rT<m_aIgnoreWriteTokens.begin()->first)
		{
			const char *sText=rT.sText;			

			if (!m_pOutStream->aOutLines.size())
				m_pOutStream->AppendOutLine(rT.nLine,rT.uLineOffset);

			if (!sText)
				sText=m_pPreprocessor->GetTokenComment(rT.T).c_str();
			else
			if (rT.T==ETN_CONST_STRING && m_sStringConversionTokens.find(&rT)!=m_sStringConversionTokens.end())
			{
				std::string str=sText;
				size_t pos=0;
				
				str=str.substr(1,str.length()-2);
				replaceEscape(str);

				sString="{";
				if (pos<str.length())
				while (pos<str.length())
				{
					char C=str[pos];

					if (C>=' ' && C!='\'' && C!='\\')
					{
						sString+='\'';
						sString+=C;
						sString+='\'';
					}
					else
					{
						char pp[32];
						_itoa_s(C,pp,10);
						sString+=pp;
					}
					
					++pos;
					if (pos<str.length())
						sString+=',';
					else
					{
						sString+=",0";
					}
				}
				else
					sString+="0";
				sString+='}';

				sText=sString.c_str();
			}


			if (m_pOutStream->nLastWrittenLine!=rT.nLine)
			{
				int nDiff=(rT.nLine & 0xFFFFFF)-(m_pOutStream->nLastWrittenLine & 0xFFFFFF);
			
				nDiff=min(max(nDiff,1),2);

				for (int m=0;m<nDiff;++m)
					m_pOutStream->AppendOutLine(rT.nLine,((m<nDiff-1)?0:rT.uLineOffset));

				if (!n)
					nFirstEmptyLines=nDiff-1;
			}
			else		
			if ((IsTextToken(rT.T) && IsTextToken((EXP_TOKEN)m_pOutStream->uLastWrittenToken)) || 
				(rT.T==m_pOutStream->uLastWrittenToken))
				std::get<1>(m_pOutStream->aOutLines.back())+=' ';

			std::get<1>(m_pOutStream->aOutLines.back())+=sText;

			m_pOutStream->nLastWrittenLine=rT.nLine;
			m_pOutStream->uLastWrittenToken=rT.T;
		}
	}

	m_sStringConversionTokens.clear();

	return nFirstEmptyLines;
}

void CExpCompiler::WriteString(const char *s,int nLine)
{
	if (!m_pOutStream->aOutLines.size())
		m_pOutStream->aOutLines.push_back(std::make_tuple(nLine,"",true));

	if (m_pOutStream->nLastWrittenLine!=nLine)
	{
		int nDiff=(nLine & 0xFFFFFF)-(m_pOutStream->nLastWrittenLine & 0xFFFFFF);
		if (nDiff<=0)
			nDiff=1;

		nDiff=min(nDiff,2);
			
		for (int m=0;m<nDiff;++m)
			m_pOutStream->aOutLines.push_back(std::make_tuple(nLine,"",true));
	}
	else		
	if (IsTextToken((EXP_TOKEN)m_pOutStream->uLastWrittenToken))
		std::get<1>(m_pOutStream->aOutLines.back())+=' ';

	std::get<1>(m_pOutStream->aOutLines.back())+=s;
	m_pOutStream->uLastWrittenToken=ETN_ID;
	m_pOutStream->nLastWrittenLine=nLine;
}

/*
bool CExpCompiler::canCreateVar(SExpRuleState *aStatesStack,int nAllS)
{
	SExpRuleState *pRS=0;
	for (int n=nAllS-1;n>=0;--n)
	{
		pRS=&aStatesStack[n];
		if (pRS->pRule->getID()==ERULE_block ||
			pRS->pRule->getID()==ERULE_root ||
			pRS->pRule->getID()==ERULE_kw_if ||
			pRS->pRule->getID()==ERULE_kw_for ||
			pRS->pRule->getID()==ERULE_kw_do ||
			pRS->pRule->getID()==ERULE_kw_while ||
			pRS->pRule->getID()==ERULE_kw_switch)
				break;
	}

	return pRS && (pRS->pRule->getID()==ERULE_block || pRS->pRule->getID()==ERULE_root);
}
*/
void CExpCompiler::onSuccessRuleState(CPState *pState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT)
{
	TToken *pFirstT=(TToken *)&m_TStream.getTokens()[nStartTokenNum];
	SExpRuleState *pCurRS=&aStatesStack[nAllS-1];
	EXP_RULE expRule=(EXP_RULE)pState->getID();

	m_nLastErrorLine=pFirstT->nLine;
	/*
	if (isReturnedBlock(aStatesStack,nAllS-1))
	{
		if (expRule==ERULE_block && pCurRS->nState==0)	//Mark nested blocks as returned
			pCurRS->auParam[3]=1;

		return;
	}
	*/
	if (m_nBlockErrorRuleLn>=0 && nAllS==3 && aStatesStack[2].pRule->getID()!=ERULE_empty_sentence && pCurRS->nState==0)
	{		
		if (pFirstT->nLine!=m_nBlockErrorRuleLn)
			m_nBlockErrorRuleLn=-1;
	}


	switch (expRule)
	{
		case ERULE_kw_struct:if (pCurRS->nState==0 && m_nCurrentVarAttr)
							{
								m_nCurrentVarAttr=0;
							}
							else
							if (pCurRS->nState==1)
							{
								m_sCurrentStructName=pFirstT->sText;
							}
							else
							if (pCurRS->nState==2)
							{
								PushScope(m_sCurrentStructName);
							}
							else
							if (pCurRS->nState==4)
							{
								PopScope();
								m_sCurrentStructName="";
							}
				break;

		case ERULE_kw_cbuffer:if (pCurRS->nState==1 && m_nCurrentVarAttr)
							{
								if (m_nCurrentVarAttr)
									m_pOutStream->mVarsAttrs[pFirstT->sText]=m_nCurrentVarAttr;
								
								//EndIDDef();
							}
				break;

		case ERULE_define_id:if (pCurRS->nState==0)
							{
								if (m_aScopes.size()>1)
									m_sNewGlobalID="";
								else
									BeginIDDef(pFirstT);
							}
				break;

		case ERULE_init_list:if (m_sNewGlobalID.length() && m_bParseIDInit)
							{
								if (pCurRS->nState==0)
								{
									_ASSERTE(!m_anVarPointers.size());

									if (!m_anVarPointers.size())
									{
										std::vector<int> anDimSizes;
										m_pGlobalIDType->Unroll(&anDimSizes);

										if (!anDimSizes.size())
										{
											m_anVarPointers.push_back(0);
											m_aVarInitItems.resize(1);
											m_nCurrentGlobalVarInitDim=0;
										}
										else
										{
											m_anVarPointers.resize(anDimSizes.size());

											int nSize=1;
											for (int sz:	anDimSizes)
												nSize*=sz;

											m_aVarInitItems.resize(nSize);
										}
									}
								}
#ifdef MUTE_GLOBAL_INITIALIZERS
								if (pCurRS->nState==0)
								{
									m_aIgnoreWriteTokens.emplace_back(std::make_pair(pFirstT,pFirstT));
								}
								else
								if (pCurRS->nState==1)
								{
									m_aIgnoreWriteTokens.back().second=pFirstT+nAllT-1;
								}
#endif
							}
				break;

		case ERULE_init_block:if (m_sNewGlobalID.length() && m_bParseIDInit)
							{
								if (pCurRS->nState==0)	//{
								{										
									m_nCurrentGlobalVarInitDim++;

									if ((size_t)m_nCurrentGlobalVarInitDim<m_anVarPointers.size())
										m_anVarPointers[m_nCurrentGlobalVarInitDim]=0;
									else
									if ((size_t)m_nCurrentGlobalVarInitDim>m_anVarPointers.size())	//for "aggregate type as array" init
										Error(EERR_TOO_MANY_INIT,m_sNewGlobalID.c_str());
								}
								else
								if (pCurRS->nState==2)	//}
								{
									bool bAggregateAsArrayInit=(size_t)m_nCurrentGlobalVarInitDim==m_anVarPointers.size();

									if (bAggregateAsArrayInit)
										AddGlobalVarInitData(m_cvInit);

									m_nCurrentGlobalVarInitDim--;
									if (m_nCurrentGlobalVarInitDim>=0 && !bAggregateAsArrayInit && (size_t)m_nCurrentGlobalVarInitDim<m_anVarPointers.size())
										m_anVarPointers[m_nCurrentGlobalVarInitDim]++;
								}
							}
				break;

		case ERULE_var_qualified_type:if (m_aScopes.size()==1 && !HasRule(aStatesStack,nAllS,ERULE_func_def))	//Only in global scope
									{
										if (pCurRS->nState==0)
										{
											SetTypeQualifier(pFirstT,nAllT);
										}
										else
										if (pCurRS->nState==1)
											SetGlobalIDType(pFirstT,nAllT);
									}
									else
										m_pGlobalIDBaseType=0;
				break;
				
		case ERULE_kw_enum:if (pCurRS->nState==1)
							if (CreateEnum(pFirstT->sText))
								m_sNewEnum=pFirstT->sText;
							else
								m_sNewEnum="";
				break;

		case ERULE_kw_defrange:if (pCurRS->nState==2)
									m_NewConstRange.first=pFirstT->sText;
								else
								if (pCurRS->nState==4)
									m_NewConstRange.second.first=ParseConstExpression(pFirstT,nAllT,m_pPassConstProvider.get(),"");
								else
								if (pCurRS->nState==6)
									m_NewConstRange.second.second=ParseConstExpression(pFirstT,nAllT,m_pPassConstProvider.get(),"");
							break;

		case ERULE_set_dss:if (pCurRS->nState==4)
							m_NewPass.second.uStencilRef=ParseConstExpression(pFirstT,nAllT,this);
							break;

		case ERULE_set_bs:if (pCurRS->nState==4)
							m_NewPass.second.uBlendFactor=ParseConstExpression(pFirstT,nAllT,this);
						else
						if (pCurRS->nState==6)
							m_NewPass.second.uSampleMask=ParseConstExpression(pFirstT,nAllT,this);
							break;

		case ERULE_kw_pass:if (pCurRS->nState==2)
								BeginPass(m_TStream.getTokens()[nStartTokenNum-1].sText);
							break;

		case ERULE_kw_technique:if (pCurRS->nState==2)
								BeginTech(m_TStream.getTokens()[nStartTokenNum-1].sText);
							break;

		case ERULE_kw_depthstencilstate:if (pCurRS->nState==2)
								BeginDSS(m_TStream.getTokens()[nStartTokenNum-1].sText);
							break;

		case ERULE_kw_blendstate:if (pCurRS->nState==2)
								BeginBS(m_TStream.getTokens()[nStartTokenNum-1].sText);
							break;

		case ERULE_kw_rasterstate:if (pCurRS->nState==2)
								BeginRS(m_TStream.getTokens()[nStartTokenNum-1].sText);
							break;

		case ERULE_kw_sampler:if (pCurRS->nState==1)
								BeginSamplers(pFirstT,nAllT);
							break;

		case ERULE_sampler_in_array:
		case ERULE_sampler:if (pCurRS->nState==0)
							{
								if (!m_anSamplersSizes.size())
								{
									m_anSamplersSizes.push_back(1);
									m_anSamplerPointers.push_back(0);									

									m_aNewSamplers.resize(1);
								}

								SetDefaultSampler(m_NewSampler);
							}break;

		case ERULE_samplers_dimension:if (pCurRS->nState==0)
									{										
										m_nCurrentSamplersInitDim++;

										if ((size_t)m_nCurrentSamplersInitDim<m_anSamplerPointers.size())
											m_anSamplerPointers[m_nCurrentSamplersInitDim]=0;
										else
											Error(EERR_TOO_MANY_INIT,m_sNewSampler.c_str());
									}
									else
									if (pCurRS->nState==3)
									{
										m_nCurrentSamplersInitDim--;
										if (m_nCurrentSamplersInitDim>=0)
											m_anSamplerPointers[m_nCurrentSamplersInitDim]++;
									}
							break;

		case ERULE_kw_switch:
				break;

		case ERULE_func_name:if (m_aScopes.size()==1)
							{
								if (pCurRS->nState==1)
								{
									m_CurrentFunc.sName=pFirstT->sText;

									bool bFound=false;
									GetEnum("",pFirstT->sText,&bFound);
								
									if (bFound)
										Error(EERR_REDEFINITION,pFirstT->sText);

									m_uGlobalVarQualifier=0;
								}
							}
							//else
								//ErrorLn(pFirstT->nLine,EERR_NESTED_FUNC);
				break;


		case ERULE_func_def:
			break;

		case ERULE_block:if (pCurRS->nState==0)
						{
							blockOpen(true,aStatesStack,nAllS,pFirstT->nLine);
						}
						else
						if (pCurRS->nState==2)
						{
							blockOpen(false,aStatesStack,nAllS,pFirstT->nLine);
						}
			break;

		case ERULE_kw_if:if (pCurRS->nState==4)//true part
						{
			/*
							if ((pFirstT+nAllT)->T==ETN_KW_ELSE)
							{
								int nTrueJumpPos=(int)m_pOutStream->aOps.size();
								m_pOutStream->aOps.push_back(SOperationCode(CExpVM::OpDesc(EOP_JUMP),0));
								pCurRS->auParam[1]=nTrueJumpPos;
							}
									//nFalseJumpPos 
							m_pOutStream->aOps[pCurRS->auParam[0]].uData=(m_pOutStream->aOps.size()-pCurRS->auParam[0]-1)<<16;
							*/
						}
						else
						if (pCurRS->nState==5)//false part
						{
							//m_pOutStream->aOps[pCurRS->auParam[1]].uData=(m_pOutStream->aOps.size()-pCurRS->auParam[1]-1)<<16;
						}
						break;

		case ERULE_kw_for:
			break;

		case ERULE_kw_do:
			break;

		case ERULE_kw_while:
			break;		

	
	}

	if (expRule==ERULE_any_sentence && nAllS==2)
	{
		/*
		m_aCodeBlocks.back()->uSize=m_pOutStream->aOps.size()-m_aCodeBlocks.back()->uStreamPos;
		if (m_aCodeBlocks.back()->uSize)
			m_aCodeBlocks.push_back(std::make_unique<SCodeBlock>(m_pOutStream->aOps.size()));
		else
			*m_aCodeBlocks.back()=SCodeBlock(m_pOutStream->aOps.size());
			*/
	}
}

void CExpCompiler::onSuccessRule(CPState *pState,SExpRuleState *aStatesStack,int nAllS,int nTokenNumFirst,int nAllT)
{
	TToken *pFirstT=(TToken *)&m_TStream.getTokens()[nTokenNumFirst];
	m_nLastErrorLine=pFirstT->nLine;

	//if (isReturnedBlock(aStatesStack,nAllS-1))
		//return;

	EXP_RULE expRule=(EXP_RULE)pState->getID();

	switch (expRule)
	{
		case ERULE_id_def_strict:
		case ERULE_id_def:m_uGlobalVarQualifier=0;
				break;

		case ERULE_define_id:EndIDDef();
							if (m_pGlobalIDBaseType && HasRule(aStatesStack,nAllS,ERULE_kw_typedef))
								DefineNewTypeID(pFirstT,nAllT);
				break;

		case ERULE_enum_def:if (m_sNewEnum.length())
								AddNewEnumValue(pFirstT,nAllT);
				break;

		case ERULE_kw_defconst:AddConst(pFirstT,nAllT);
							break;

		case ERULE_kw_defrange:AddConstRange();
							break;

		case ERULE_set_shader:SetPassShader(pFirstT,nAllT);
							break;

		case ERULE_set_dss:SetPassDSS(pFirstT,nAllT);
							break;
		case ERULE_set_bs:SetPassBS(pFirstT,nAllT);
							break;
		case ERULE_set_rs:SetPassRS(pFirstT,nAllT);
							break;

		case ERULE_kw_pass:AddPass(pFirstT,nAllT);
							break;

		case ERULE_kw_technique:AddTech(pFirstT,nAllT);
							break;


		case ERULE_kw_depthstencilstate:AddDSS(pFirstT,nAllT);
							break;
		case ERULE_dss_definition:SetDSSVar(pFirstT,nAllT);
				break;

		case ERULE_kw_blendstate:AddBS(pFirstT,nAllT);
							break;
		case ERULE_bs_definition:SetBSVar(pFirstT,nAllT);
				break;

		case ERULE_kw_rasterstate:AddRS(pFirstT,nAllT);
							break;
		case ERULE_rs_definition:SetRSVar(pFirstT,nAllT);
				break;

		case ERULE_array_dim:if (m_sNewGlobalID.length())
							{
								if (nAllT>2)
									m_anIDDimSizes.push_back((int)ParseConstExpression(pFirstT+1,nAllT-2,this));
								else
									Error(EERR_ARRAY,m_sNewGlobalID.c_str(),0);
							}
				break;




		case ERULE_kw_sampler:AddSamplers(pFirstT,nAllT);
							break;

		case ERULE_sampler_in_array:
		case ERULE_sampler:AddSampler(pFirstT,nAllT);
							break;


		case ERULE_samplers_array_size:
								{
									m_anSamplersSizes.push_back((int)ParseConstExpression(pFirstT+1,nAllT-2,this));
									m_anSamplerPointers.push_back(0);

									int nSize=1;
									for (int sz:	m_anSamplersSizes)
										nSize*=sz;

									size_t uOldSZ=m_aNewSamplers.size();
									m_aNewSamplers.resize(nSize);

									for (;uOldSZ<m_aNewSamplers.size();++uOldSZ)
										SetDefaultSampler(m_aNewSamplers[uOldSZ]);
								}break;

		case ERULE_sampler_definition:SetSamplerVar(pFirstT,nAllT);
				break;

		case ERULE_exception_symbols:Error(EERR_UNEXPECTED_TOKEN,(char *)pFirstT);									
			break;

		case ERULE_var_func_definition:if (m_aScopes.size()==1)
								{
									SFXCode::SFuncDesc *pFDesc=0;
									if (m_CurrentFunc.sName.length())
									{
										for (const std::string &sFuncName:	m_sDelayedReferences)
											m_CurrentFunc.sReferencedFunc.insert(sFuncName);

										auto it=m_pOutStream->mFuncDesc.emplace(m_CurrentFunc.sName,m_CurrentFunc);

										pFDesc=&it->second;
										if (m_nFirstAttributeOutLine<0)
											pFDesc->uStartLine=m_pOutStream->aOutLines.size();
										else
											pFDesc->uStartLine=m_nFirstAttributeOutLine;

										m_CurrentFunc.Reset();
									}
									m_sDelayedReferences.clear();

									int nBeginingEmptyLines=WriteTokens(pFirstT,nAllT);

									if (pFDesc)
									{
										pFDesc->uStartLine+=nBeginingEmptyLines;
										pFDesc->uLastLine=m_pOutStream->aOutLines.size()?m_pOutStream->aOutLines.size()-1:0;
									}
								}
			break;
					
		
		case ERULE_kw_cbuffer:WriteTokens(pFirstT,nAllT);							
				break;

		case ERULE_kw_struct:if (m_aScopes.size()==1 && nAllS<=3)
								WriteTokens(pFirstT,nAllT);
				break;

		case ERULE_kw_typedef:if (m_aScopes.size()==1 && nAllS<=3)
								WriteTokens(pFirstT,nAllT);							
				break;

		case ERULE_static_attr:if (m_aScopes.size()==1)
							{
								if (!CustomAttr(pFirstT,nAllT))
								{
									if (m_nFirstAttributeOutLine<0)
										m_nFirstAttributeOutLine=(int)m_pOutStream->aOutLines.size();

									MarkAttrReferences(pFirstT,nAllT);
									WriteTokens(pFirstT,nAllT);
								}
							}
				break;

		case ERULE_under_const_array:
				break;
		case ERULE_under_const_expr:
				break;

		case ERULE_kw_switch:break;

		case ERULE_kw_case:
				break;

		case ERULE_kw_default:
			break;

		case ERULE_init_data:if (m_sNewGlobalID.length() && m_bParseIDInit)
							{
								if ((size_t)m_nCurrentGlobalVarInitDim<m_anVarPointers.size())
								{	
									if (ReadConstVector(pFirstT,nAllT,m_cvInit))
										AddGlobalVarInitData(m_cvInit);
								}
								else //Aggregate values inited as arrays
								{
									m_cvInit.pType=dynamic_cast<const CVectorType *>(m_pGlobalIDType->Unroll());

									if (m_cvInit.aVals.size()<m_cvInit.getAllVals())
									{
										m_cvInit.aVals.resize(m_cvInit.aVals.size()+1);
										m_cvInit.aVals.back()=ParseConstExpression(pFirstT,nAllT,this);
									}
									else
										Error(EERR_TOO_MANY_INIT,m_sNewGlobalID.c_str());
								}
							}
							else
							{
								AddLocalStringConversions(pFirstT,nAllT);
								ProcessExpression(pFirstT,nAllT);
							}
			break;

		case ERULE_init_list:if (m_sNewGlobalID.length() && m_bParseIDInit)
								AddNewInitedVar();
			break;

		//case ERULE_init_single:
		case ERULE_single_expression:
		case ERULE_arg_expression:
		case ERULE_kw_expression:{
									ProcessExpression(pFirstT,nAllT);
								}break;

		case ERULE_kw_if:
			break;
		case ERULE_kw_else:
			break;
		
		case ERULE_kw_for:
			break;

		case ERULE_kw_do:
			break;

		
		case ERULE_kw_while:
			break;

		case ERULE_kw_break:
			break;

		case ERULE_kw_continue:
			break;
	}

	if (m_aScopes.size()==1 && expRule!=ERULE_static_attr && nAllS==3)
		m_nFirstAttributeOutLine=-1;
}

bool CExpCompiler::HasRule(SExpRuleState* aStatesStack, int nAllS, EXP_RULE r)
{
	while (nAllS-->0)
	if (aStatesStack[nAllS].pRule->getID()==r)
		return true;

	return false;
}

bool CExpCompiler::onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNum,bool bProcessed)
{
	bool bErr=true;
	if (bProcessed)
		return true;
	
	TToken *pErrT=(TToken *)&m_TStream.getTokens()[nTokenNum];
	SExpRuleState *pCurRS=&aStatesStack[nAllS-1];
	EXP_RULE expRule=(EXP_RULE)pTState->getID();
	m_nLastErrorLine=pErrT->nLine;

	switch (expRule)
	{
		case ERULE_var_def://if (hasRule(aStatesStack, nAllS, ERULE_var_def))
						{
							if (pCurRS->nState == 1)
							{
								EndIDDef();
								return false;	//Not a error on #1 state
							}
							/*else
								if (pCurRS->nState == 3)
								{
									if (pErrT->T != ETN_BRACEC_OPEN)	//Error when we get anything else than function definition '('
									{
										Error(EERR_CHAR_EXPECTED_FOUND_TOKEN, (char*)'(', (char*)pErrT);
										return true;
									}

									EndIDDef();
								}*/
						}
						return false;
			break;

		//case ERULE_func_name:if (pCurRS->nState==1 || m_aScopes.size()>1)	//Not a error on #1 state, and inside any nested scopes {..}
			//					return false;								//as local function definitions are not expected
			//break;
			
		case ERULE_kw_if:if (pCurRS->nState>=4)
							return false;
			break;

		case ERULE_init_braces:if (pCurRS->nState==1)
						return false;
			break;
	}


	switch (pErrT->T)
	{
		case ETN_NONE:Error(EERR_EOF);
						return true;
			break;

		default:Error(EERR_UNEXPECTED_TOKEN,(char *)pErrT);
				return true;
	}

	return false;
}


void CExpCompiler::BeginRS(const char *sName)
{
	SetDefaultRS(m_NewRS.second);
	m_NewRS.first=sName;
}



void CExpCompiler::SetRSVar(TToken *aTokens,int nAllT)
{
	static const char *asVars[]={"fillmode","cullmode","frontcounterclockwise","depthbias","depthbiasclamp","slopescaleddepthbias",
								"depthclipenable","scissorenable","multisampleenable","antialiasedlineenable"};
	FX_RASTERIZER_DESC &RS=m_NewRS.second;
	int nVarName;
	TToken *pConst=&aTokens[2];
	std::string s=aTokens[0].sText;

	_strlwr_s((char *)s.c_str(),s.length()+1);
	for (nVarName=0;nVarName<_countof(asVars) && strcmp(s.c_str(),asVars[nVarName]);++nVarName);

	TToken *pExpr=aTokens+2;
	int nAllExprT=nAllT-3;

	switch (nVarName)
	{
		case 0:RS.FillMode=(FX_FILL_MODE)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_FILL");
			break;
		case 1:RS.CullMode=(FX_CULL_MODE)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_CULL");
			break;
		case 2:RS.FrontCounterClockwise=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 3:RS.DepthBias=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 4:RS.DepthBiasClamp=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 5:RS.SlopeScaledDepthBias=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 6:RS.DepthClipEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 7:RS.ScissorEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 8:RS.MultisampleEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 9:RS.AntialiasedLineEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;

		default:Error(EERR_UNDEFINED_ID,aTokens[0].sText);
	}
}

void CExpCompiler::AddRS(TToken *aTokens,int nAllT)
{	
	m_pOutStream->mRS[m_NewRS.first]=m_NewRS.second;
}

void CExpCompiler::SetTypeQualifier(TToken *pToken,int nAllT)
{
	static std::map<int,TYPE_QUALIFIERS_BIT> mTQ;
	if (!mTQ.size())
	{
		mTQ[ETN_KW_STATIC]=TQB_STATIC;
		mTQ[ETN_KW_CONST]=TQB_CONST;
		mTQ[ETN_KW_VOLATILE]=TQB_VOLATILE;
		mTQ[ETN_KW_UNIFORM]=TQB_UNIFORM;
		mTQ[ETN_KW_EXTERN]=TQB_EXTERN;


		_ASSERTE(mTQ.size()==TQB_SIZE);
	}

	while (nAllT--)
	{
		auto it=mTQ.find(pToken->T);
		if (it!=mTQ.end())
			m_uGlobalVarQualifier|=1<<it->second;

		pToken++;
	}
}

void CExpCompiler::SetGlobalIDType(TToken *aTokens,int nAllT)
{
	int n=0;
	bool bUnsigned=false;

	if (aTokens[0].T==ETN_KW_SIGNED || aTokens[0].T==ETN_KW_UNSIGNED)
	{
		if (aTokens[0].T==ETN_KW_UNSIGNED)
			bUnsigned=true;

		n++;
	}

	if (n<nAllT && aTokens[n].sText)	//Not a struct
		m_pGlobalIDBaseType=FindType(aTokens[n].sText);
	else
		m_pGlobalIDBaseType=0;
}

void CExpCompiler::EndIDDef()
{
	if (m_sNewGlobalID.length() && m_pGlobalIDBaseType)
	{
		PBaseType pType=std::make_shared<CTypedef>((m_anIDDimSizes.size()?"":m_sNewGlobalID.c_str()),0,m_pGlobalIDBaseType,m_uGlobalVarQualifier);		
		while (m_anIDDimSizes.size())
		{
			pType=std::make_shared<CTypedef>((m_anIDDimSizes.size()==1?m_sNewGlobalID.c_str():""),m_anIDDimSizes.back(),pType);			
			m_anIDDimSizes.pop_back();
		}

		m_pGlobalIDType=pType;

		unsigned int uTQ=0;
		pType->Unroll(0,&uTQ);
		m_bParseIDInit=(uTQ & (1<<TQB_STATIC | 1<<TQB_CONST))!=(1<<TQB_STATIC | 1<<TQB_CONST);
	}
	else
		m_pGlobalIDType=0;
	
	
	m_nCurrentVarAttr=0;
}

void CExpCompiler::BeginIDDef(TToken *pToken)
{
	m_sNewGlobalID="";
	m_pGlobalIDType=0;

	if (m_pGlobalIDBaseType)
	{
		m_sNewGlobalID=pToken->sText;

		m_anIDDimSizes.clear();
		m_anVarPointers.clear();
		m_aVarInitItems.clear();
		m_cvInit.clear();
		m_nCurrentGlobalVarInitDim=-1;
		m_bParseIDInit=true;
	}
	else
		m_bParseIDInit=false;


	if (m_nCurrentVarAttr)
		m_pOutStream->mVarsAttrs[pToken->sText]=m_nCurrentVarAttr;
}

void CExpCompiler::DefineNewTypeID(TToken *aTokens,int nAllT)
{
	if (m_pGlobalIDType)
	{
		m_mTypes.emplace(m_sNewGlobalID,m_pGlobalIDType);
	}
}

void CExpCompiler::AddNewInitedVar()
{
	const int nAlignment=16;
	const CVectorType *pVT=dynamic_cast<const CVectorType *>(m_pGlobalIDType->Unroll());
	CV_TYPE Type=CV_TYPE(pVT->GetType() & CV_TYPE_MASK);
	int nItemSize=(Type==CV_DOUBLE?8:4),nXStride;
	int nDimsY=max(pVT->GetDimsY(),1);
	int nDimsX=max(pVT->GetDimsX(),1);
	std::vector<unsigned char> aBuf;
	int ptr=0;

	nXStride=(nDimsX*nItemSize+(nAlignment-1)) & ~(nAlignment-1);
	aBuf.resize(nXStride*nDimsY*m_aVarInitItems.size()-(nXStride-nDimsX*nItemSize));

	for (size_t n=0;n<m_aVarInitItems.size();++n)
	{
		int v=0;

		for (int y=0;y<nDimsY;++y)
		{
			unsigned int *auI=(unsigned int *)(&aBuf[ptr]);
			unsigned long long int *auLLI=(unsigned long long int *)(&aBuf[ptr]);

			for (int x=0;x<nDimsX;++x,++v)
			{
				if (v<(int)m_aVarInitItems[n].aVals.size())
				switch (Type)
				{
					case CV_INT:*(auI++)=m_aVarInitItems[n].aVals[v];
						break;

					case CV_FLOAT:*((float *)(auI++))=m_aVarInitItems[n].aVals[v];
						break;

					case CV_DOUBLE:*((double *)(auLLI++))=m_aVarInitItems[n].aVals[v];
						break;

					default:_ASSERTE(FALSE);
						break;
				}
			}

			ptr+=nXStride;
			_ASSERTE((char *)auI-(char *)(&aBuf[0])<=(int)aBuf.size());
			_ASSERTE((char *)auLLI-(char *)(&aBuf[0])<=(int)aBuf.size());
		}
	}

	_ASSERTE(m_pOutStream->mVarsInitData.find(m_sNewGlobalID)==m_pOutStream->mVarsInitData.end());
	m_pOutStream->mVarsInitData[m_sNewGlobalID]=aBuf;
	
	m_sNewGlobalID="";
}


void CExpCompiler::AddGlobalVarInitData(SConstVector &cv)
{
	int ptr=0;
	int stride=1;
	std::vector<int> anIDDimSizes;
	const CBaseType *pBaseType;

	_ASSERTE(m_pGlobalIDType);
	pBaseType=m_pGlobalIDType->Unroll(&anIDDimSizes);

	if (!anIDDimSizes.size())
		anIDDimSizes.push_back(1);

	for (size_t n=0;n<m_anVarPointers.size();++n)
	if (m_anVarPointers[n]>=anIDDimSizes[n])
	{
		Error(EERR_TOO_MANY_INIT,m_sNewGlobalID.c_str());
		return;
	}


	for (int n=(int)anIDDimSizes.size()-1;n>=0;--n)
	{
		ptr+=m_anVarPointers[n]*stride;
		stride*=anIDDimSizes[n];
	}
	
	SConstVector &rCV=m_aVarInitItems[ptr];
	const CVectorType *pVT=dynamic_cast<const CVectorType *>(m_pGlobalIDType->Unroll());
	unsigned int uAllVals=(unsigned int)(max(pVT->GetDimsX(),1)*max(pVT->GetDimsY(),1));
	
	if (!cv.pType)
		cv.pType=pVT;

	rCV.aVals.resize(uAllVals);
	rCV.pType=cv.pType;
	cv.addMissingData();

	unsigned int uSrcValsNumber=cv.getAllVals();

	if (uSrcValsNumber==1)
	{
		for (unsigned int n=0;n<uAllVals;++n)
			rCV.aVals[n]=cv.aVals[0];
	}
	else
	if (uSrcValsNumber==uAllVals)
	{
		for (unsigned int n=0;n<uAllVals;++n)
			rCV.aVals[n]=cv.aVals[n];
	}
	else
		Error(EERR_TYPE_MISMATCH,cv.pType->GetName().c_str(),m_pGlobalIDBaseType->GetName().c_str());

	m_anVarPointers.back()++;

	cv.clear();
}

bool CExpCompiler::ReadConstVector(TToken *aTokens,int nAllT,SConstVector &rDest)
{
	PBaseType pConstructType=(aTokens[0].T==ETN_ID?FindType(aTokens[0].sText):PBaseType());
	const CVectorType *pType=0;
	bool bRet=true;
	std::vector<int> anDimSizes;
	int nTotalSize=1;

		
	if (pConstructType)
		pType=dynamic_cast<const CVectorType *>(pConstructType->Unroll(&anDimSizes));

	for (int nSZ:	anDimSizes)
		nTotalSize*=nSZ;

	if (nTotalSize>1)
	{
		Error(EERR_WRONG_INIT_TYPE,aTokens[0].sText);
		bRet=false;
	}
	else
	if (!pType)
	{
		rDest.aVals.resize(1);
		SComValue &cv=rDest.aVals[0];

		cv=ParseConstExpression(aTokens,nAllT,this);

		switch (cv.uType & CV_TYPE_MASK)
		{
			case CV_NULL:
			case CV_CHAR:
			case CV_SHORT:
			case CV_INT:pType=(CVectorType *)((cv.uType & CV_UNSIGNED)?m_mPrimTypes["uint"].get():m_mPrimTypes["int"].get());
				break;

			case CV_DOUBLE:pType=(CVectorType *)m_mPrimTypes["double"].get();
				break;

			default:pType=(CVectorType *)m_mPrimTypes["float"].get();
		}
	}
	else
	{
		int ptr=1;
		unsigned int uMaxVals=(unsigned int)(max(pType->GetDimsX(),1)*max(pType->GetDimsY(),1));

		if (ptr<nAllT && aTokens[ptr].T==ETN_BRACEC_OPEN)
		{
			int nLevel=1,ptr0=++ptr;

			while (nLevel && ptr<nAllT)
			{
				TToken *pT=&aTokens[ptr];

				if (nLevel==1 && pT->T==ETN_COMMA)
				{
					if (rDest.aVals.size()<uMaxVals)
					{
						rDest.aVals.resize(rDest.aVals.size()+1);
						SComValue &rCV=rDest.aVals.back();

						rCV=ParseConstExpression(&aTokens[ptr0],ptr-ptr0,this);
						rCV.cast(pType->GetType());
					}
					else
					{
						Error(EERR_TOO_MANY_INIT,m_sNewGlobalID.c_str());
						bRet=false;
						break;
					}

					ptr0=ptr+1;
				}

				if (pT->T==ETN_BRACEC_OPEN)
					nLevel++;
				if (pT->T==ETN_BRACEC_CLOSE)
					nLevel--;

				ptr++;
			}
			ptr--;

			if (ptr-ptr0<1)
			{
				Error(EERR_UNEXPECTED_TOKEN,(char *)&aTokens[ptr],0);
				bRet=false;
			}
			else
			if (rDest.aVals.size()<uMaxVals)
			{
				rDest.aVals.resize(rDest.aVals.size()+1);
				SComValue &rCV=rDest.aVals.back();

				rCV=ParseConstExpression(&aTokens[ptr0],ptr-ptr0,this);
				rCV.cast(pType->GetType());				
			}
			else
			{
				Error(EERR_TOO_MANY_INIT,m_sNewGlobalID.c_str());
				bRet=false;
			}
			/*
			if (!cnt)
				rDest.aVals[cnt++]=0.0f;
			
			for (;cnt<nMaxVals;++cnt)
				rDest.aVals[cnt]=rDest.aVals[cnt-1];
			*/
			if (ptr<nAllT-1)
			{
				Error(EERR_UNEXPECTED_TOKEN,(char *)&aTokens[ptr+1],0);
				bRet=false;
			}

			if (nLevel)
			{
				Error(EERR_MISSING_CHAR,(char *)')',0);
				bRet=false;
			}
		}
		else
		{
			Error(EERR_UNEXPECTED_TOKEN,(char *)&aTokens[ptr],0);
			bRet=false;
		}
	}

	rDest.pType=pType;

	return bRet;
}

void CExpCompiler::BeginSamplers(TToken *aTokens,int nAllT)
{
	m_sNewSampler=aTokens[0].sText;
	m_aNewSamplers.clear();
	m_anSamplersSizes.clear();
	m_anSamplerPointers.clear();
	m_nCurrentSamplersInitDim=-1;
}

void CExpCompiler::SetSamplerVar(TToken *aTokens,int nAllT)
{
	static const char *asVars[]={"filter","addressu","addressv","addressw","miplodbias","maxanisotropy","comparisonfunc","bordercolor","minlod","maxlod"};
	FX_SAMPLER &S=m_NewSampler;
	int nVarName;
	std::string s=aTokens[0].sText;

	_strlwr_s((char *)s.c_str(),s.length()+1);
	for (nVarName=0;nVarName<_countof(asVars) && strcmp(s.c_str(),asVars[nVarName]);++nVarName);
	
	TToken *pExpr=aTokens+2;
	int nAllExprT=nAllT-3;

	switch (nVarName)
	{
		case 0:S.Filter=(FX_FILTER)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_FILTER");
			break;
		case 1:S.AddressU=(FX_TEXTURE_ADDRESS_MODE)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_TEXTURE_ADDRESS");
			break;
		case 2:S.AddressV=(FX_TEXTURE_ADDRESS_MODE)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_TEXTURE_ADDRESS");
			break;
		case 3:S.AddressW=(FX_TEXTURE_ADDRESS_MODE)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_TEXTURE_ADDRESS");
			break;
		case 4:S.MipLODBias=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 5:S.MaxAnisotropy=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 6:S.ComparisonFunc=(FX_COMPARISON_FUNC)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_COMPARISON");
			break;
		case 7:S.BorderColor=(unsigned int)ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 8:S.MinLOD=ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 9:S.MaxLOD=ParseConstExpression(pExpr,nAllExprT,this);
			break;

		default:Error(EERR_UNDEFINED_ID,aTokens[0].sText);
	}
}


void CExpCompiler::AddSamplers(TToken *aTokens,int nAllT)
{
	if (aTokens[0].T==ETN_KW_SAMPLERCMPSTATE)
		WriteString("SamplerComparisonState",aTokens[1].nLine);
	else
		WriteString("SamplerState",aTokens[1].nLine);
	
	char p[1024];

	if (m_anSamplersSizes.size()==1 && m_anSamplersSizes[0]==1)
		WriteString(m_sNewSampler.c_str(),aTokens[1].nLine);
	else
	{
		strcpy_s(p,m_sNewSampler.c_str());
		for (size_t n=0;n<m_anSamplersSizes.size();++n)
			sprintf_s(p+strlen(p),sizeof(p)-strlen(p),"[%i]",m_anSamplersSizes[n]);
		
		WriteString(p,aTokens[1].nLine);
	}
		
	WriteString(";",aTokens[1].nLine);

	//int d=0;
	int ptr=0;
	memset(&m_anSamplerPointers[0],0,sizeof(m_anSamplerPointers[0])*m_anSamplerPointers.size());

	do
	{
		strcpy_s(p,m_sNewSampler.c_str());

		if (m_anSamplersSizes.size()>1 || m_anSamplersSizes[0]>1)		
			sprintf_s(p+strlen(p),sizeof(p)-strlen(p),"[%i]",ptr);
		//for (size_t n=0;n<m_anSamplerPointers.size();++n)
			//sprintf_s(p+strlen(p),sizeof(p)-strlen(p),"[%i]",m_anSamplerPointers[n]);		

		m_pOutStream->mSamplers[p]=m_aNewSamplers[ptr++];
		/*
		d=(int)m_anSamplerPointers.size()-1;
		bool bDec;
		do
		{
			bDec=false;
			m_anSamplerPointers[d]++;

			if (m_anSamplerPointers[d]>=m_anSamplersSizes[d])
			{
				m_anSamplerPointers[d]=0;
				d--;
				if (d>=0)					
					bDec=true;
			}
		}while (bDec);*/
	}while (ptr<(int)m_aNewSamplers.size());//d>=0);
}

void CExpCompiler::AddSampler(TToken *aTokens,int nAllT)
{
	int ptr=0;
	int stride=1;

	for (size_t n=0;n<m_anSamplerPointers.size();++n)
	if (m_anSamplerPointers[n]>=m_anSamplersSizes[n])
	{
		Error(EERR_TOO_MANY_INIT,m_sNewSampler.c_str());
		return;
	}


	if (nAllT==1)
	{
		auto it=m_pOutStream->mSamplers.find(aTokens[0].sText);

		if (it!=m_pOutStream->mSamplers.end())
			m_NewSampler=it->second;
		else
			Error(EERR_UNDEFINED_ID,aTokens[0].sText);
	}

	for (int n=(int)m_anSamplersSizes.size()-1;n>=0;--n)
	{
		ptr+=m_anSamplerPointers[n]*stride;
		stride*=m_anSamplersSizes[n];
	}

	m_aNewSamplers[ptr]=m_NewSampler;
	m_anSamplerPointers.back()++;
}


void CExpCompiler::BeginBS(const char *sName)
{
	SetDefaultBS(m_NewBS.second);
	m_NewBS.first=sName;
}

void CExpCompiler::SetBSVar(TToken *aTokens,int nAllT)
{
	static const char *asVars[]={"alphatocoverageenable","blendenable","srcblend","destblend","blendop","srcblendalpha","destblendalpha",
								"blendopalpha","rendertargetwritemask"};
	FX_BLEND_DESC &BS=m_NewBS.second;
	int nVarName;
	TToken *pConst=&aTokens[2];
	std::string s=aTokens[0].sText;

	_strlwr_s((char *)s.c_str(),s.length()+1);
	for (nVarName=0;nVarName<_countof(asVars) && strcmp(s.c_str(),asVars[nVarName]);++nVarName);

	TToken *pExpr=aTokens+2;
	int nAllExprT=nAllT-3;

	switch (nVarName)
	{
		case 0:BS.AlphaToCoverageEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;

		case 1:BS.BlendEnable[min((int)GetComValue(pConst->sText),_countof(BS.BlendEnable))]=(bool)ParseConstExpression(aTokens+5,nAllT-6,this,"");
			break;

		case 2:BS.SrcBlend=(FX_BLEND)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND");
			break;

		case 3:BS.DestBlend=(FX_BLEND)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND");
			break;

		case 4:BS.BlendOp=(FX_BLEND_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND_OP");
			break;

		case 5:BS.SrcBlendAlpha=(FX_BLEND)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND");
			break;

		case 6:BS.DestBlendAlpha=(FX_BLEND)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND");
			break;

		case 7:BS.BlendOpAlpha=(FX_BLEND_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_BLEND_OP");
			break;

		case 8:BS.RenderTargetWriteMask[min((int)GetComValue(pConst->sText),_countof(BS.RenderTargetWriteMask))]=(int)ParseConstExpression(aTokens+5,nAllT-6,this,"");
			break;

		default:Error(EERR_UNDEFINED_ID,aTokens[0].sText);
	}
}

void CExpCompiler::AddBS(TToken *aTokens,int nAllT)
{
	_ASSERTE(m_NewBS.second.DestBlendAlpha);
	m_pOutStream->mBS[m_NewBS.first]=m_NewBS.second;
}


void CExpCompiler::BeginDSS(const char *sName)
{
	SetDefaultDSS(m_NewDSS.second);
	m_NewDSS.first=sName;
}

void CExpCompiler::SetDSSVar(TToken *aTokens,int nAllT)
{
	static const char *asVars[]={"depthenable","depthwritemask","depthfunc","stencilenable","stencilreadmask","stencilwritemask",
								"frontfacestencilfail","frontfacestencilzfail","frontfacestencilpass","frontfacestencilfunc",
								"backfacestencilfail","backfacestencilzfail","backfacestencilpass","backfacestencilfunc"};
	FX_DEPTH_STENCIL_DESC &DSS=m_NewDSS.second;
	int nVarName;
	std::string s=aTokens[0].sText;

	_strlwr_s((char *)s.c_str(),s.length()+1);
	for (nVarName=0;nVarName<_countof(asVars) && strcmp(s.c_str(),asVars[nVarName]);++nVarName);

	TToken *pExpr=aTokens+2;
	int nAllExprT=nAllT-3;

	switch (nVarName)
	{
		case 0:DSS.DepthEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 1:DSS.DepthWriteMask=(FX_DEPTH_WRITE_MASK)(int)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 2:DSS.DepthFunc=(FX_COMPARISON_FUNC)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_COMPARISON");
			break;
		case 3:DSS.StencilEnable=(bool)ParseConstExpression(pExpr,nAllExprT,this,"");
			break;
		case 4:DSS.StencilReadMask=(int)ParseConstExpression(pExpr,nAllExprT,this);
			break;
		case 5:DSS.StencilWriteMask=(int)ParseConstExpression(pExpr,nAllExprT,this);
			break;

		case 6:DSS.FrontFace.StencilFailOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 7:DSS.FrontFace.StencilDepthFailOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 8:DSS.FrontFace.StencilPassOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 9:DSS.FrontFace.StencilFunc=(FX_COMPARISON_FUNC)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_COMPARISON");
			break;

		case 10:DSS.BackFace.StencilFailOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 11:DSS.BackFace.StencilDepthFailOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 12:DSS.BackFace.StencilPassOp=(FX_STENCIL_OP)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_STENCIL_OP");
			break;
		case 13:DSS.BackFace.StencilFunc=(FX_COMPARISON_FUNC)(int)ParseConstExpression(pExpr,nAllExprT,this,"FX_COMPARISON");
			break;

		default:Error(EERR_UNDEFINED_ID,aTokens[0].sText);
	}
}

void CExpCompiler::AddDSS(TToken *aTokens,int nAllT)
{	
	m_pOutStream->mDSS[m_NewDSS.first]=m_NewDSS.second;
}

bool CExpCompiler::GetConst(const char *sName,SComValue &ret,size_t uNamespace)
{
	const char *sPrefix=uNamespace?(char *)uNamespace:"";
	bool bSuccess;
	int n=GetEnum(sPrefix,sName,&bSuccess);

	if (bSuccess)
		ret=n;

	return bSuccess;
}


SComValue CExpCompiler::ParseConstExpression(TToken *aTokens,int nAllT,CConstProvider *pCProvider,const char *sNameSpace)
{
	TARecords aRecords;
	int nBraceLevel=0;

	for (int n=0;n<nAllT;++n)	
	{
		TToken &rT=aTokens[n];

		switch (rT.T)
		{
			case ETN_ID:if (pCProvider)
						{
							SComValue cv;
							if (!pCProvider->GetConst(rT.sText,cv,(size_t)sNameSpace))
							{
								Error(EERR_UNDEFINED_ID,rT.sText);
								return SComValue();
							}
							else
								aRecords.push_back(SConstRecord(cv));
						}
						else
						{
							Error(EERR_UNDEFINED_ID,rT.sText);
							return SComValue();
						}
				break;

			case ETN_CONST_INT:
			case ETN_CONST_INT_BIN:
			case ETN_CONST_INT_OCT:
			case ETN_CONST_INT_HEX:
			case ETN_CONST_FLOAT:
			case ETN_CONST_FLOAT_E:
			case ETN_CONST_DOUBLE:
			case ETN_CONST_DOUBLE_E:aRecords.push_back(SConstRecord(GetComValue(rT.sText)));
				break;



			default:if (rT.T==ETN_BRACEC_CLOSE || rT.T==ETN_BRACEC_OPEN || m_mOpPriority.find(rT.T)!=m_mOpPriority.end())
						aRecords.push_back(SConstRecord(rT.T));
					else
					{
						Error(EERR_UNEXPECTED_TOKEN,(char *)&rT);
						return SComValue();
					}

					if (rT.T==ETN_BRACEC_OPEN)
						nBraceLevel++;
					if (rT.T==ETN_BRACEC_CLOSE)
						nBraceLevel--;
				break;
		}

		if (nBraceLevel<0)
		{
			Error(EERR_UNMATCHED_BRACESC);
			return SComValue();
		}
	}

	if (nBraceLevel!=0)
	{
		Error(EERR_UNMATCHED_BRACESC);
		return SComValue();
	}

	if (EvaluteValue(aRecords,0,-1))
	{
		_ASSERTE(aRecords.size()==1);
		return aRecords[0].val;
	}
	else
	{
		Error(EERR_EXPRESSION);
		return SComValue();
	}
}

bool CExpCompiler::EvaluteValue(TARecords &aRecords,int nPos,int nPrevPriority)
{
	if (nPos>=(int)aRecords.size())
		return false;

	while (1)
	{
		EXP_TOKEN StartingOp=aRecords[nPos].Op;

		if (StartingOp==ETN_NONE)	//const value
		{
			//ret=aRecords[nPos].val;

			if (nPos>=(int)aRecords.size()-1 || aRecords[nPos+1].Op==ETN_BRACEC_CLOSE)
				return true;

			EXP_TOKEN Op=aRecords[nPos+1].Op;
			if (Op!=ETN_NONE)
			{
				int nPriority=m_mOpPriority[Op];

				if (nPriority>nPrevPriority)
				{
					if (EvaluteValue(aRecords,nPos+2,nPriority))
					{
						if (ComputeOp(aRecords[nPos].val,aRecords[nPos+2].val,Op))
						{
							for (int n=0;n<2;++n)
								aRecords.erase(aRecords.begin()+nPos+1);
						}
						else
							return false;
					}
					else
						return false;
				}
				else
					return true;
			}
			else
				return false;
		}
		else
		if (StartingOp==ETN_BRACEC_OPEN)
		{
			if (EvaluteValue(aRecords,nPos+1,-1))
			{		
				aRecords[nPos]=aRecords[nPos+1];
				for (int n=0;n<2;++n)
					aRecords.erase(aRecords.begin()+nPos+1);
			}
			else
				return false;
		}
		else
		if (StartingOp==ETN_OP_NOT || StartingOp==ETN_OP_BITNOT)
		{
			if (EvaluteValue(aRecords,nPos+1,100))
			{
				ComputeOp(aRecords[nPos+1].val,SComValue(),StartingOp);

				aRecords[nPos]=aRecords[nPos+1];
				aRecords.erase(aRecords.begin()+nPos+1);
			}
			else
				return false;
		}
		else
		if (StartingOp==ETN_OP_MINUS)
		{
			if (EvaluteValue(aRecords,nPos+1,100))
			{
				ComputeOp(aRecords[nPos+1].val,SComValue(-1),ETN_OP_MUL);

				aRecords[nPos]=aRecords[nPos+1];
				aRecords.erase(aRecords.begin()+nPos+1);
			}
			else
				return false;
		}
		else
			return false;
	}

	return true;
}

bool CExpCompiler::GetCommonMathType(const SComValue &a,const SComValue &b,CV_TYPE &ret)
{
	//static const CV_TYPE auConversion[]={CV_INT,CV_INT,CV_INT,CV_INT,CV_DOUBLE,CV_REF,CV_CTYPE};
	//_ASSERTE(_countof(auConversion)==CV_SIZE);

	unsigned char Ta=a.uType & CV_TYPE_MASK,
				Tb=b.uType & CV_TYPE_MASK;

	ret=(CV_TYPE)max(Ta,Tb);
	_ASSERTE(ret<CV_SIZE);
	if (ret<CV_INT)
	{
		ret=CV_INT;
		return true;
	}
	else
	if (ret==CV_INT)
		ret=CV_TYPE(CV_INT | (((Ta+(a.uType>>5)) | (Tb+(b.uType>>5)))<<3 & CV_UNSIGNED));
	

	return ret!=a.uType || ret!=b.uType;
}

bool CExpCompiler::ComputeOp(SComValue &a,SComValue b,EXP_TOKEN op)
{
	CV_TYPE CT;

	if ((b.uType & CV_TYPE_MASK)>CV_NULL)
	{
		GetCommonMathType(a,b,CT);

		a.cast(CT);
		b.cast(CT);
	}
	else
		CT=(CV_TYPE)a.type();

	CT=CV_TYPE(CT & CV_TYPE_MASK);

	auto it=m_mOpFunc.find(op);
	if (it==m_mOpFunc.end() || !it->second[CT] || !it->second[CT](&a,&b))
	{
		Error(EERR_INCORRECT_OP_USAGE,m_pPreprocessor->GetTokenComment(op).c_str());
		return false;
	}

	return true;
}

void CExpCompiler::BeginTech(const char *sName)
{
	m_NewTech.second.aPassG.clear();
	m_NewTech.second.sName=sName;
	m_NewTech.first=sName;
}

void CExpCompiler::AddTech(TToken *aTokens,int nAllT)
{
	if (m_pOutStream->mTech.find(m_NewTech.first)==m_pOutStream->mTech.end())
		m_pOutStream->mTech[m_NewTech.first]=m_NewTech.second;
	else
		Error(EERR_REDEFINITION,m_NewTech.first.c_str());
}

void CExpCompiler::BeginPass(const char *sName)
{
	m_NewPass.second=SFXPassGroup();
	m_NewPass.second.sName=sName;
	m_NewPass.first=sName;
	m_NewPassConst.clear();
}

void CExpCompiler::AddPass(TToken *aTokens,int nAllT)
{
	for (auto &pair:	m_NewPassConst)
		m_NewPass.second.mConstParam.emplace(pair.first,pair.second.first);
	m_NewTech.second.aPassG.push_back(m_NewPass.second);
}

void CExpCompiler::SetPassRS(TToken *aTokens,int nAllT)
{
	const char *sName=aTokens[2].sText;
	auto it=m_pOutStream->mRS.find(sName);

	if (it!=m_pOutStream->mRS.end())
		m_NewPass.second.sRS=sName;
	else
		Error(EERR_UNDEFINED_ID,sName);
}

void CExpCompiler::SetPassBS(TToken *aTokens,int nAllT)
{
	const char *sName=aTokens[2].sText;
	auto it=m_pOutStream->mBS.find(sName);

	if (it!=m_pOutStream->mBS.end())
		m_NewPass.second.sBS=sName;
	else
		Error(EERR_UNDEFINED_ID,sName);
}

void CExpCompiler::SetPassDSS(TToken *aTokens,int nAllT)
{
	const char *sName=aTokens[2].sText;
	auto it=m_pOutStream->mDSS.find(sName);

	if (it!=m_pOutStream->mDSS.end())
		m_NewPass.second.sDSS=sName;
	else
		Error(EERR_UNDEFINED_ID,sName);
}

void CExpCompiler::SetPassShader(TToken *aTokens,int nAllT)
{
	int nID=aTokens[0].T-ETN_KW_SETVS;
	int ver;
	_ASSERTE(nID<SFXPass::FXS_SIZE);

	ver=ParseConstExpression(aTokens+2,nAllT-5,this);
	if (ver && nID<SFXPass::FXS_SIZE)
		m_NewPass.second.aEntryPoints[nID]=std::make_pair(aTokens[nAllT-2].sText,ver);
}

const char *CExpCompiler::GetShaderVer(int nShaderName,int ver)
{
	static const char *sName[SFXPass::FXS_SIZE]={"vs_","ps_","gs_","hs_","ds_","cs_"};
	static char sRet[32];
	char p[16];
	int nVer=ver/10,
		nSubVer=ver-nVer*10;

	strcpy_s(sRet,sName[nShaderName]);
	_itoa_s(nVer,p,10);
	strcat_s(sRet,p);
	strcat_s(sRet,"_");
	_itoa_s(nSubVer,p,10);
	strcat_s(sRet,p);


	return sRet;
}


bool CExpCompiler::CompilePassGroup(const char *sSourceName_,SFXPassGroup &PG,unsigned int uFlags,const std::map<std::string,std::string> &mDefMacros)
{
	bool bRet=true;
	std::vector<TMacroDefinition> aConstMacros;
	std::vector<std::pair<std::string,	std::tuple<int,int,int,std::string>>> aParams;
	int nShaderNum=0;
	std::string sSource;
	std::vector<SFXCode::TSourceIDLine> anLineID;

	m_pOutStream->UnmarkAllFunctions();
		
	for (nShaderNum=0;nShaderNum<SFXPass::FXS_SIZE;++nShaderNum)
	if (PG.aEntryPoints[nShaderNum].first.length())
		m_pOutStream->MarkUsedFunction(PG.aEntryPoints[nShaderNum].first);


	m_pOutStream->FormatSource(sSource,false,&anLineID);

//Add unreferenced implicit default macros
	for (auto &pair:	mDefMacros)
	{
		bool bUsed=PG.mConstParam.find(pair.first)!=PG.mConstParam.end() ||
				PG.mParamRange.find(pair.first)!=PG.mParamRange.end();

		if (!bUsed)
		{
			aConstMacros.resize(aConstMacros.size()+1);
			TMacroDefinition &rM=aConstMacros.back();
			rM.first=pair.first.c_str();
			rM.second=pair.second.c_str();
		}
	}

//Explicit macros
	for (auto &pair:	PG.mConstParam)
	{
		aConstMacros.resize(aConstMacros.size()+1);

		TMacroDefinition &rM=aConstMacros.back();
		rM.first=pair.first.c_str();
		rM.second=pair.second.c_str();
	}

	aParams.resize(PG.mParamRange.size());
	for (auto &pair:	PG.mParamRange)
	{
		char p[32];
		_itoa_s(pair.second.nMin,p,10);
		aParams[pair.second.uIndex]=std::make_pair(pair.first,	std::make_tuple(pair.second.nMin,pair.second.nMin,pair.second.nMax,p));
	}

	int nConstCount=(int)aConstMacros.size();
	aConstMacros.resize(nConstCount+aParams.size()+1);

	int nPassNum=0;
	int nSwitchedParam;
	do
	{
		std::unique_ptr<SFXPass> pPass=std::make_unique<SFXPass>();

		for (int n=0;n<(int)aParams.size();++n)
		{
			aConstMacros[nConstCount+n].first=aParams[n].first.c_str();
			aConstMacros[nConstCount+n].second=std::get<3>(aParams[n].second).c_str();
		}

		
		for (nShaderNum=0;nShaderNum<SFXPass::FXS_SIZE && bRet;++nShaderNum)
		if (PG.aEntryPoints[nShaderNum].first.length())
		{
			ID3DBlob *pCode=0,*pErr=0;
			SFXPassGroup::TEntryPointVer &EP=PG.aEntryPoints[nShaderNum];
			std::string sSourceName(sSourceName_);
			char p[32];

			_itoa_s(nPassNum,p,10);
			sSourceName+=std::string("_")+PG.sName+"["+std::string(p)+"]_"+GetShaderVer(nShaderNum,EP.second);

			if (!D3DCompile(sSource.c_str(),sSource.length(),
					sSourceName.c_str(),aConstMacros.size()?&aConstMacros[0]:0,EP.first.c_str(),GetShaderVer(nShaderNum,EP.second),EP.second,uFlags,&pCode,&pErr))
			{
				bRet=false;
			}
			else
			{
				pPass->aShaders[nShaderNum].resize(pCode->GetBufferSize());
				memcpy(&pPass->aShaders[nShaderNum][0],pCode->GetBufferPointer(),pCode->GetBufferSize());
				pCode->Release();
			}

			if (pErr)
			{
				static const char *asShaders[SFXPass::FXS_SIZE]={"Vertex Shader","Pixel Shader","Geometry Shader","Hull Shader","Domain Shader","Compute shader"};
				int nErrCnt=m_nErrorsCnt;

				OutputD3DCompilerErrors(sSourceName.c_str(),pErr,&anLineID[0],(int)anLineID.size());
				pErr->Release();
		
				if (nErrCnt!=m_nErrorsCnt)
				{
					ErrorLn(0,EERR_PASS,(PG.sName+": "+asShaders[nShaderNum]).c_str());
					m_nBlockErrorRuleLn=-1;
				}
			}
		}

		if (bRet)
			PG.apPass.emplace_back(std::move(pPass));



		nSwitchedParam=0;
		bool bNext=true;
		while (bNext && nSwitchedParam<(int)aParams.size())
		{
			int p0=nSwitchedParam;
			int &val=std::get<0>(aParams[p0].second);
			val++;

			if (val>std::get<2>(aParams[p0].second))
			{
				val=std::get<1>(aParams[p0].second);
				nSwitchedParam++;
				bNext=true;
			}
			else
				bNext=false;

			char p[32];
			_itoa_s(val,p,10);
			std::get<3>(aParams[p0].second)=p;
		}
		nPassNum++;
	}while (nSwitchedParam<(int)aParams.size() && bRet);

	return bRet;
}

bool CExpCompiler::D3DCompile(const char *sSource,size_t sz,const char *sFileName,TMacroDefinition *apMacros,const char *sEntryPoint,
				const char *sShaderName,int nShaderVer,unsigned int uFlags,void *ppCode,void *ppErrorMsgs)
{
	if (nShaderVer<=51)
		return D3DCompile2(sSource,sz,
				sFileName,(D3D_SHADER_MACRO *)apMacros,0,sEntryPoint,sShaderName,uFlags,0,0,0,0,(ID3DBlob **)ppCode,(ID3DBlob **)ppErrorMsgs)==S_OK;
	else
	{
		const std::pair<int,const wchar_t *> aFlags[]={
														{D3DCOMPILE_SKIP_OPTIMIZATION,	DXC_ARG_SKIP_OPTIMIZATIONS},
														{D3DCOMPILE_OPTIMIZATION_LEVEL0,DXC_ARG_OPTIMIZATION_LEVEL0},
														{D3DCOMPILE_OPTIMIZATION_LEVEL1,DXC_ARG_OPTIMIZATION_LEVEL1},
														{D3DCOMPILE_OPTIMIZATION_LEVEL2,DXC_ARG_OPTIMIZATION_LEVEL2},
														{D3DCOMPILE_OPTIMIZATION_LEVEL3,DXC_ARG_OPTIMIZATION_LEVEL3},
														{D3DCOMPILE_WARNINGS_ARE_ERRORS,DXC_ARG_WARNINGS_ARE_ERRORS},
														{D3DCOMPILE_SKIP_VALIDATION,	DXC_ARG_SKIP_VALIDATION},
														{D3DCOMPILE_DEBUG,				DXC_ARG_DEBUG},
														{D3DCOMPILE_PACK_MATRIX_ROW_MAJOR,DXC_ARG_PACK_MATRIX_ROW_MAJOR},
														{D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR,DXC_ARG_PACK_MATRIX_COLUMN_MAJOR},
														{D3DCOMPILE_AVOID_FLOW_CONTROL,	DXC_ARG_AVOID_FLOW_CONTROL},
														{D3DCOMPILE_PREFER_FLOW_CONTROL,DXC_ARG_PREFER_FLOW_CONTROL},
														{D3DCOMPILE_ENABLE_STRICTNESS,	DXC_ARG_ENABLE_STRICTNESS},
														{D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY,DXC_ARG_ENABLE_BACKWARDS_COMPATIBILITY},
														{D3DCOMPILE_IEEE_STRICTNESS,	DXC_ARG_IEEE_STRICTNESS}
														};
		IDxcUtils *pUtils=0;
		std::vector<const wchar_t *> asArgs;
		std::vector<std::unique_ptr<std::wstring>> apsDefines;
		std::wstring wsEntryPoint,wsTarget,wsFileName;
		size_t szRet;

		wsFileName.resize(strlen(sFileName));
		mbstowcs_s(&szRet,(wchar_t *)wsFileName.c_str(),wsFileName.length()+1,sFileName,wsFileName.length()+1);

		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
		IDxcBlobEncoding *pSource=0;
		pUtils->CreateBlob(sSource,(unsigned int)sz, CP_UTF8, &pSource);
		IDxcCompiler3 *pCompiler=0;
		DxcCreateInstance(CLSID_DxcCompiler,IID_PPV_ARGS(&pCompiler));

		wsEntryPoint.resize(strlen(sEntryPoint));
		mbstowcs_s(&szRet,(wchar_t *)wsEntryPoint.c_str(),wsEntryPoint.length()+1,sEntryPoint,wsEntryPoint.length());		
		asArgs.push_back(L"-E");
		asArgs.push_back(wsEntryPoint.c_str());

		wsTarget.resize(strlen(sShaderName));
		mbstowcs_s(&szRet,(wchar_t *)wsTarget.c_str(),wsTarget.length()+1,sShaderName,wsTarget.length());		
		asArgs.push_back(L"-T");
		asArgs.push_back(wsTarget.c_str());

		// Strip reflection data and pdbs (see later)
		if (uFlags & D3DCOMPILE_DEBUG)
		{
			asArgs.push_back(L"-Qembed_debug");

			asArgs.push_back(DXC_ARG_DEBUG_NAME_FOR_SOURCE);
			asArgs.push_back(wsFileName.c_str());
		}
		else
			asArgs.push_back(L"-Qstrip_debug");
		//asArgs.push_back(L"-Qstrip_reflect");

		
		for (auto &pair:	aFlags)
		if (uFlags & pair.first)
			asArgs.push_back(pair.second);


		if (apMacros)
		while (apMacros->first)		
		{
			wchar_t p[1024];
			std::unique_ptr<std::wstring> psDefine=std::make_unique<std::wstring>();

			mbstowcs_s(&szRet,p,apMacros->first,strlen(apMacros->first));
			*psDefine=p;
			(*psDefine)+=L"=";			
			mbstowcs_s(&szRet,p,apMacros->second,strlen(apMacros->second));
			(*psDefine)+=p;

			asArgs.push_back(L"-D");
			asArgs.push_back(psDefine->c_str());			

			apsDefines.push_back(std::move(psDefine));
			apMacros++;
		}

		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = pSource->GetBufferPointer();
		sourceBuffer.Size = pSource->GetBufferSize();
		sourceBuffer.Encoding = 0;

		IDxcResult *pCompileResult=0;
		bool bRet=pCompiler->Compile(&sourceBuffer, asArgs.data(), (unsigned int)asArgs.size(), nullptr, IID_PPV_ARGS(&pCompileResult))==S_OK;

		IDxcBlobUtf8 *pErrors=0;
		const int nSkipLines=2;
		pCompileResult->GetOutput(DXC_OUT_ERRORS,IID_PPV_ARGS(&pErrors),nullptr);
		
		if (pErrors && pErrors->GetStringLength() > 0)
		{
			std::string sErrs=(char *)pErrors->GetBufferPointer(),s;
			size_t pos=0,pos1,pos0;
			int nSkip=0;

			while (pos<sErrs.size())
			{
				for (pos1=pos;pos1<sErrs.size() && sErrs[pos1]!='\n';++pos1);

				if (!nSkip)
				{
					std::string sLine=sErrs.substr(pos,pos1-pos);

					if (sLine.find("warning: ")==0 || sLine.find("error: ")==0)
					{
						s+=sFileName;
						s+="(,): ";
						s+=sLine;
					}
					else
					{
						if ((pos0=sLine.find(':'))!=-1)
						{
							sLine[pos0]='(';
							sLine.replace(0,pos0,sFileName);

							pos0=sLine.find(':');
							if (pos0!=-1)
							{
								sLine[pos0]=',';
								pos0=sLine.find(':',pos0+1);
								if (pos0!=-1)
									sLine.insert(pos0,")");
							}
						}

						//if (sLine.find("error: ")!=-1)
							//bRet=false;

						s+=sLine;
						s+='\n';
						nSkip=nSkipLines;
					}
				}
				else
					nSkip--;

				pos=pos1+1;
			}
			
			ID3D10Blob *pErrs=0;
			D3DCreateBlob(s.length()+1,&pErrs);
			memcpy(pErrs->GetBufferPointer(),s.c_str(),s.length()+1);

			*((ID3DBlob **)ppErrorMsgs)=pErrs;
			pErrors->Release();
		}

		if (bRet)
		{
			IDxcBlob *pCode=0;
			pCompileResult->GetOutput(DXC_OUT_OBJECT,IID_PPV_ARGS(&pCode),nullptr);

			if (pCode && pCode->GetBufferSize())
			{
				ID3D10Blob *pC=0;
				size_t sz=pCode->GetBufferSize();
				D3DCreateBlob(pCode->GetBufferSize(),&pC);
				memcpy(pC->GetBufferPointer(),pCode->GetBufferPointer(),pCode->GetBufferSize());

				*((ID3DBlob **)ppCode)=pC;
				pCode->Release();
			}
			else
				bRet=false;
		}

		if (pCompileResult)
			pCompileResult->Release();
		pCompiler->Release();
		pSource->Release();
		pUtils->Release();
		return bRet;
	}
}


void CExpCompiler::AddConst(TToken *aTokens,int nAllT)
{
	TToken *pID=&aTokens[0];
	bool bFound=false;
	GetEnum("",pID->sText,&bFound);

	if (!bFound && m_NewPass.second.mParamRange.find(pID->sText)==m_NewPass.second.mParamRange.end())
//	if (m_NewPassConst.find(pID->sText)==m_NewPassConst.end())
	{		
		if (aTokens[2].T==ETN_CONST_STRING)
		{
			std::string s=aTokens[2].sText;
			m_NewPassConst[pID->sText]=std::make_pair(s.substr(1,s.length()-2),SComValue());
		}
		else
		{
			SComValue cv=ParseConstExpression(aTokens+2,nAllT-2,m_pPassConstProvider.get(),"");
			char p[32]="";

			if (cv.type()<=CV_INT)
			{
				if (cv.uType & CV_UNSIGNED)
					sprintf_s(p,"0x%x",(int)cv);
				else
					sprintf_s(p,"%i",(int)cv);
			}
			else
			if (cv.type()==CV_FLOAT)
				sprintf_s(p,"%f",(float)cv);
			else
			if (cv.type()==CV_DOUBLE)
				sprintf_s(p,"%f",(double)cv);
			else
				_ASSERTE(FALSE);

			if (p[0])
				m_NewPassConst[pID->sText]=std::make_pair(p,cv);
		}
	}
	else
		Error(EERR_REDEFINITION,pID->sText);
}

void CExpCompiler::AddConstRange()
{
	bool bFound=false;
	GetEnum("",m_NewConstRange.first,&bFound);

	if (!bFound && m_NewPass.second.mParamRange.find(m_NewConstRange.first)==m_NewPass.second.mParamRange.end() &&
		m_NewPassConst.find(m_NewConstRange.first)==m_NewPassConst.end())
	{
		if (m_NewPass.second.mParamRange.size()<6)
		{
			if (m_NewConstRange.second.second-m_NewConstRange.second.first+1<=CONST_RANGE_LENGTH)
			{
				SFXPassGroup::TParam P;
				P.uIndex=(unsigned char)m_NewPass.second.mParamRange.size();
				P.nMin=min(m_NewConstRange.second.first,m_NewConstRange.second.second);
				P.nMax=max(m_NewConstRange.second.first,m_NewConstRange.second.second);

				m_NewPass.second.mParamRange.insert(std::make_pair(m_NewConstRange.first,P));
			}
			else
				Error(EERR_TOO_LARGE_CONSTRANGE,m_NewConstRange.first.c_str());
		}
		else
			Error(EERR_TOO_MANY_CONSTRANGES,m_NewPass.first.c_str());
	}
	else
		Error(EERR_REDEFINITION,m_NewConstRange.first.c_str());
}

void CExpCompiler::SetFileHandler(CFileHandler *pFH)
{
	m_pFileHandler=pFH;
	if (!m_pFileHandler)
		m_pFileHandler=this;
}

void CExpCompiler::InsertKeptDirectives()
{
	auto &aSS=m_pPreprocessor->GetSourceSectors();
	auto &aKD=m_pPreprocessor->GetKeptDirectives();
	std::vector<SFXCode::TSourceLine> aOut,&raSrc=m_pOutStream->aOutLines;
	size_t uCurLine=0,uCurKD=0;

	for (auto &pair:	aSS)
	{
		int nSLine=(uCurLine<raSrc.size()?std::get<0>(raSrc[uCurLine]):-1);
		int nMinLine=pair.first & 0xFFFFFF,nMaxLine=pair.second & 0xFFFFFF,
			nFileID=pair.first & 0xFF000000;


		while ((nSLine & 0xFF000000)==nFileID && (nSLine & 0xFFFFFF)>=nMinLine && (nSLine & 0xFFFFFF)<=nMaxLine)
		{
			size_t sz0=aOut.size();
			while (uCurKD<aKD.size() && (aKD[uCurKD].first & 0xFF000000)==nFileID &&
										(aKD[uCurKD].first & 0xFFFFFF)<(nSLine & 0xFFFFFF))
			{
				aOut.emplace_back(std::make_tuple(aKD[uCurKD].first,aKD[uCurKD].second,true));
				uCurKD++;
			}

			if (sz0<aOut.size())
			for (auto &pair:	m_pOutStream->mFuncDesc)
			{
				if (pair.second.uStartLine>=sz0)
					pair.second.uStartLine+=aOut.size()-sz0;
				if (pair.second.uLastLine>sz0)
					pair.second.uLastLine+=aOut.size()-sz0;
			}


			aOut.emplace_back(raSrc[uCurLine]);
			uCurLine++;

			if (uCurLine<raSrc.size())
				nSLine=std::get<0>(raSrc[uCurLine]);
			else
				break;
		}

		size_t sz0=aOut.size();
		int nPrevLine=0;
		while (uCurKD<aKD.size() && (aKD[uCurKD].first & 0xFF000000)==nFileID &&
									(aKD[uCurKD].first & 0xFFFFFF)>=nMinLine && (aKD[uCurKD].first & 0xFFFFFF)<=nMaxLine &&
									aKD[uCurKD].first>nPrevLine)
		{
			aOut.emplace_back(std::make_tuple(aKD[uCurKD].first,aKD[uCurKD].second,true));
			nPrevLine=aKD[uCurKD].first;
			uCurKD++;
		}

		if (sz0<aOut.size())
		for (auto &pair:	m_pOutStream->mFuncDesc)
		{
			if (pair.second.uStartLine>=sz0)
				pair.second.uStartLine+=aOut.size()-sz0;
			if (pair.second.uLastLine>sz0)
				pair.second.uLastLine+=aOut.size()-sz0;
		}
	}

	/*_ASSERTE(rsSrc==sOut);
	_ASSERTE(anLines.size()==m_pOutStream->anSourceLines.size() &&
			!memcmp(&anLines[0],&m_pOutStream->anSourceLines[0],anLines.size()*sizeof(int)));
			*/
	_ASSERTE(uCurKD>=aKD.size());
	raSrc=aOut;
}

void CExpCompiler::ProcessExpression(TToken *aT,int nAllT)
{
	for (int n=0;n<nAllT-1;++n)
	if (aT[n].T==ETN_ID && aT[n+1].T==ETN_BRACEC_OPEN && (!n || aT[n-1].T!=ETN_OP_MEM))
	{		
		auto it=m_pOutStream->mFuncDesc.find(aT[n].sText);

		if (it!=m_pOutStream->mFuncDesc.end())
		{
			if (m_CurrentFunc.sName.length())
				m_CurrentFunc.sReferencedFunc.insert(it->first);
			else
			{
				auto it=m_pOutStream->mFuncDesc.find("");
				if (it==m_pOutStream->mFuncDesc.end())
					it=m_pOutStream->mFuncDesc.emplace("",SFXCode::SFuncDesc());
				
				it->second.sReferencedFunc.insert(it->first);
			}
		}
	}
}

bool CExpCompiler::CustomAttr(TToken *aT,int nAllT)
{
	static const char *asAttrs[]={"root_param","root_const"};
	bool bRet=false;

	static_assert(_countof(asAttrs)==SFXCode::VAB_SIZE,"_countof(asAttrs)==SFXCode::VAB_SIZE");

	if (nAllT==3 && aT[1].T==ETN_ID)
	{
		int n;
		for (n=0;n<_countof(asAttrs) && strcmp(asAttrs[n],aT[1].sText);++n);

		if (n<_countof(asAttrs))
		{
			m_nCurrentVarAttr|=1<<n;
			bRet=true;
		}
	}

	return bRet;
}

void CExpCompiler::MarkAttrReferences(TToken *aT,int nAllT)
{
	std::string s;

	for (int n=0;n<nAllT-1;++n)
	if (aT[n].T==ETN_CONST_STRING)
	{
		s=aT[n].sText+1;

		if (s.length()>=2)
		{
			s.resize(s.length()-1);
			auto it=m_pOutStream->mFuncDesc.find(s);

			if (it!=m_pOutStream->mFuncDesc.end())
				m_sDelayedReferences.insert(it->first);
		}
	}
}

bool CExpCompiler::CreateEnum(const char *sName)
{
	PExpTag pEnum=std::make_shared<CExpEnum>(sName);
	return AddNewTag(pEnum);
}

bool CExpCompiler::AddNewTag(PExpTag pTag)
{
	if (m_aScopes.back().mTags.find(pTag->GetName())!=m_aScopes.back().mTags.end())
		Error(EERR_REDEFINITION,pTag->GetName().c_str());
	else
	{
		m_aScopes.back().mTags.insert(std::make_pair(pTag->GetName(),pTag));
		return true;
	}

	return false;
}

int *CExpCompiler::FindEnumConst(const std::string &s,bool bThisScope)
{
	int n;

	for (n=(int)m_aScopes.size()-1;n>=0;--n)
	{
		SScopeDesc &rScope=m_aScopes[n];
		auto it=rScope.mTags.begin();

		while (it!=rScope.mTags.end())
		{
			CExpEnum *pEnum=dynamic_cast<CExpEnum *>(it->second.get());
			if (pEnum && pEnum->GetIDConst(s))
				return pEnum->GetIDConst(s);

			++it;
		}

		if (bThisScope)
			return 0;
	}

	return 0;
}

bool CExpCompiler::AddNewEnumValue(TToken *aT,int nAllT)
{
	TToken *ptID=&aT[0];
	bool bFound=false;

	_ASSERTE(nAllT && ptID->T==ETN_ID);

	GetEnum("",ptID->sText,&bFound);

	if (bFound || /*FindEnumConst(ptID->sText,true) || */m_pOutStream->mFuncDesc.find(ptID->sText)!=m_pOutStream->mFuncDesc.end())
	{
		Error(EERR_REDEFINITION,ptID->sText);
		return false;
	}

	int val=0;
	if (nAllT>2)
	{
		SComValue cv;
		if ((cv=ParseConstExpression(aT+2,nAllT-2,this,"")).uType!=CV_NULL)	
		{
			if ((cv.uType & CV_TYPE_MASK)>=CV_FLOAT)
			{
				Error(EERR_NOT_INT);
				return false;
			}

			val=cv;
		}
		else
		{
			Error(EERR_CONST_EXPECTED);
			return false;
		}
	}

	if (!((CExpEnum *)(m_aScopes.back().mTags[m_sNewEnum].get()))->AddID(ptID->sText,nAllT>2?&val:0))
	{
		Error(EERR_REDEFINITION,ptID->sText);
		return false;
	}
	else
	{
		char p[32];
		int *pVal=((CExpEnum *)m_aScopes.back().mTags[m_sNewEnum].get())->GetIDConst(ptID->sText);
		_ASSERTE(pVal);
		_itoa_s(pVal?*pVal:0,p,10);
		m_pOutStream->mDefinitions[ptID->sText]=p;
	}

	return true;
}

void CExpCompiler::AddLocalStringConversions(TToken *aT,int nAllT)
{
	while (nAllT--)
	{
		if (aT->T==ETN_CONST_STRING)
			m_sStringConversionTokens.insert(aT);

		aT++;
	}
}

#define ADD_PRIM_TYPE(nDims,name,type)	m_mPrimTypes.emplace((nDims>1?#name#nDims:#name),PBaseType(new CVectorType((nDims>1?#name#nDims:#name),type,nDims)));

void CExpCompiler::CreatePrimitiveTypes()
{
	if (!m_mPrimTypes.size())
	{
		ADD_PRIM_TYPE(1,int,CV_INT);
		ADD_PRIM_TYPE(1,uint,CV_INT | CV_UNSIGNED);
		ADD_PRIM_TYPE(1,float,CV_FLOAT);
		ADD_PRIM_TYPE(1,double,CV_DOUBLE);

		ADD_PRIM_TYPE(2,int,CV_INT);
		ADD_PRIM_TYPE(2,uint,CV_INT | CV_UNSIGNED);
		ADD_PRIM_TYPE(2,float,CV_FLOAT);
		ADD_PRIM_TYPE(2,double,CV_DOUBLE);

		ADD_PRIM_TYPE(3,int,CV_INT);
		ADD_PRIM_TYPE(3,uint,CV_INT | CV_UNSIGNED);
		ADD_PRIM_TYPE(3,float,CV_FLOAT);
		ADD_PRIM_TYPE(3,double,CV_DOUBLE);

		ADD_PRIM_TYPE(4,int,CV_INT);
		ADD_PRIM_TYPE(4,uint,CV_INT | CV_UNSIGNED);
		ADD_PRIM_TYPE(4,float,CV_FLOAT);
		ADD_PRIM_TYPE(4,double,CV_DOUBLE);

		std::vector<std::pair<std::string,CVectorType *>> a1DTypes;
		for (auto &pair:	m_mPrimTypes)
		if (((CVectorType *)pair.second.get())->GetDimsX()==1)
			a1DTypes.emplace_back(pair.first,(CVectorType *)pair.second.get());

		for (auto &pair:	a1DTypes)
		for (int m=1;m<=4;++m)
		for (int n=1;n<=4;++n)
		{
			char p[32];
			sprintf_s(p,"%s%ix%i",pair.first.c_str(),m,n);		
			m_mPrimTypes.emplace(p,PBaseType(new CVectorType(p,pair.second->GetType(),n,m)));
		}
	}

	m_mTypes.clear();
	for (auto &pair:	m_mPrimTypes)
		m_mTypes.emplace(pair.first,pair.second);
}

PBaseType CExpCompiler::FindType(const char *sName)
{
	if (sName)
	{
		auto it=m_mTypes.find(sName);

		if (it!=m_mTypes.end())
			return it->second;
	}

	return PBaseType();
}

#include "ConstFunc.inc"