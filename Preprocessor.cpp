#include "stdafx.h"
#include <sys/stat.h>
#include <time.h>

#include "Preprocessor.h"
#include "ExpErrors.h"

#include "TokenGen/TokenMan.h"
#include "TokenGen/TState.h"
#include <direct.h>
#include <Windows.h>
#include "resource.h"

#include <functional>


#define FILE_STR_SIZE 1024


CPreprocessor::TMNamedPrepPragmas CPreprocessor::m_mNamedPrepPragmas;
CPreprocessor::TMNamedPrepKW CPreprocessor::m_mNamedPrepKW;
CPreprocessor::TMNamedTokens CPreprocessor::m_mNamedTokens;
CPreprocessor::STokenDesc CPreprocessor::m_aTokenDesc[ETN_SIZE];
std::string CPreprocessor::m_asPrepKWName[PKW_SIZE];

CPreprocessor::CMacro::CMacro(CPreprocessor *_pOwner,const char *_sName):CBasicMacro(_pOwner,_sName),m_bVarArgs(false)
{
}
CPreprocessor::CMacro::CMacro(CPreprocessor *_pOwner,const char *_sName,const TToken *aT,int nAllT,const TAStrings &rasArgs,bool bVarArgs):
							CBasicMacro(_pOwner,_sName),m_bVarArgs(bVarArgs)
{
	SetTokens(aT,nAllT);

	if (bVarArgs)
	{
		TAStrings aArgs=rasArgs;
		aArgs.push_back("__VA_ARGS__");
		SetArgs(aArgs);
	}
	else
	SetArgs(rasArgs);
}

CPreprocessor::CMacro::CMacro(const CMacro &rSrc):CBasicMacro(rSrc)
{
	*this=rSrc;
}

CPreprocessor::CMacro &CPreprocessor::CMacro::operator =(const CMacro &rSrc)
{
	_ASSERTE(FALSE);

	SetTokens(rSrc.m_aTokens.size()?&rSrc.m_aTokens[0]:0,(int)rSrc.m_aTokens.size());
	
	m_nArgsCount=rSrc.m_nArgsCount;
	m_anTokenArg=rSrc.m_anTokenArg;
	
	m_sName=rSrc.m_sName;
	m_pOwner=rSrc.m_pOwner;

	return *this;
}

CPreprocessor::CMacro::~CMacro()
{
}

void CPreprocessor::CMacro::SetArgs(const TAStrings &rasArgs)
{
	m_nArgsCount=(int)rasArgs.size();
	
	if (m_nArgsCount)
	{
		if (m_anTokenArg.size())
			memset(&m_anTokenArg[0],-1,m_anTokenArg.size());

		for (size_t n=0;n<m_aTokens.size();++n)
		if (m_aTokens[n].T==ETN_PREP_ID)
		{
			auto it=std::find(rasArgs.begin(),rasArgs.end(),	m_aTokens[n].sText);
			if (it!=rasArgs.end())
				m_anTokenArg[n]=char(it-rasArgs.begin());
		}
	}
}

void CPreprocessor::CMacro::SetTokens(const TToken *aT,int nAllT)
{
	int n;
	int nSZ=0;

	for (n=0;n<nAllT;++n)
	if (aT[n].sText)
		nSZ+=(int)strlen(aT[n].sText)+1;
	
	_ASSERTE(nSZ<4096);
	
	m_aTokens.clear();
	m_aStringsBuf.resize(nSZ);
	char *ptr=m_aStringsBuf.size()?&m_aStringsBuf[0]:0;

	for (n=0;n<nAllT;++n)
	{
		const TToken &rT=aT[n];
		m_aTokens.push_back(rT);

		if (rT.sText)
		{
			m_aTokens.back().sText=ptr;
			strcpy_s(ptr,strlen(rT.sText)+1,rT.sText);
			ptr+=strlen(rT.sText)+1;
			_ASSERTE(ptr<=&m_aStringsBuf[0]+m_aStringsBuf.size());
		}
	}

	m_anTokenArg.resize(nAllT);
	if (nAllT)
		memset(&m_anTokenArg[0],-1,nAllT);
}


bool CPreprocessor::CMacro::BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)
{
	if (nAllAT<m_nArgsCount-int(m_bVarArgs))
		return false;

	rsDest="";

	for (size_t n=0;n<m_aTokens.size();++n)
	{
		bool bStr=false;
		bool bExpandedToken=aaExpandedTokens!=0;
		bool bDoubleHash=false;
		int nArgNum=m_anTokenArg[n];
		TToken *pT=&m_aTokens[n];
		_ASSERTE(pT->T>ETN_PREP_FIRST && pT->T<ETN_PREP_LAST);

		

		if (n && (nArgNum>=0 || pT->T==ETN_PREP_ID))
		{
			if (m_aTokens[n-1].T==ETN_PREP_HASH)
			{
				rsDest+='"';
				bStr=true;
				bExpandedToken=false;
			}
			else
			if (m_aTokens[n-1].T==ETN_PREP_DOUBLEHASH)
			{
				bExpandedToken=false;
				bDoubleHash=true;
			}
		}


		if (nArgNum>=0)
		{
			int nCount=1;
			bool bVarArg=false;

			if (nArgNum>=m_nArgsCount-int(m_bVarArgs))	//__VA_ARGS__
			{				
				nCount=nAllAT-m_nArgsCount+1;
				bVarArg=true;
			}

			if (bVarArg && !nCount && bDoubleHash)
			{
				if (rsDest.length() && rsDest.back()==',')
					rsDest.pop_back();
			}
			else
			while (nCount>0)
			{
				TATokens &aSrc=bExpandedToken?aaExpandedTokens[nArgNum]:aaTokens[nArgNum];
				for (TToken &rST:	aSrc)
				{
					if (m_pOwner->GetTokenDesc(rST.T).bString)
						rsDest+=rST.sText;
					else
						rsDest+=m_pOwner->GetTokenComment(rST.T);
				}

				if (nCount>1)
					rsDest+=',';

				nCount--;
				nArgNum++;
			}
		}
		else
		{
			auto &TDesc=m_pOwner->GetTokenDesc(pT->T);
			if (TDesc.bString)
				rsDest+=pT->sText;
			else
				rsDest+=m_pOwner->GetTokenComment(pT->T);
		}

		if (bStr)
			rsDest+='"';
	}

	return true;
}

bool CPreprocessor::CLineMacro::BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)
{
	char p[32];
	_itoa_s(m_pOwner->m_nLastStopLine & 0xFFFFFF,p,10);
	rsDest=p;

	return true;
}

bool CPreprocessor::CFileMacro::BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)
{
	if (m_pOwner->GetDep().size()>(m_pOwner->m_nLastStopLine>>24 & 0xFF))
	{
		std::string s=CPreprocessor::GetShortFileName(m_pOwner->GetDep()[(m_pOwner->m_nLastStopLine>>24 & 0xFF)].sFileName);
		for (int n=(int)s.length()-1;n>=0;--n)
			if (s[n]=='\\')
				s.insert(s.begin()+n,'\\');
		rsDest="\""+s+"\"";
	}
	else
		rsDest="\"\"";
	
	return true;
}

bool CPreprocessor::CDateMacro::BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)
{
	static const char *asMonth[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
	char p[64];
	tm TM;
	time_t T;

	time(&T);
	localtime_s(&TM,&T);
	
	sprintf_s(p,"\"%s %i %i\"",asMonth[TM.tm_mon],TM.tm_mday,TM.tm_year+1900);
	rsDest=p;
	
	return true;
}

bool CPreprocessor::CTimeMacro::BuildString(TATokens *aaTokens,TATokens *aaExpandedTokens,int nAllAT,std::string &rsDest)
{
	char p[64];
	tm TM;
	time_t T;

	time(&T);
	localtime_s(&TM,&T);
	
	sprintf_s(p,"\"%.2i:%.2i:%.2i\"",TM.tm_hour,TM.tm_min,TM.tm_sec);
	rsDest=p;
	
	return true;
}





#include "lex.inl"
#include "lex_prep.inl"
#include "lex_macro.inl"


CPreprocessor::CPreprocessor(CExpCompiler *pOwner):m_pOwner(pOwner)
{
	SetAdditionalDir("");

	m_pTMan=0;
	m_pPrepLexer=0;
	m_pMacroLexer=0;

	if (!m_mNamedTokens.size())
	{
		for (int n=0;n<ETN_SIZE;++n)
			m_aTokenDesc[n].T=(EXP_TOKEN)n;

#define DEF_ETN(name)	m_mNamedTokens["ETN_"#name]=ETN_##name;m_aTokenDesc[ETN_##name].sName="ETN_"#name;
#define DEF_ETN_STR(name)	m_mNamedTokens["ETN_"#name]=ETN_##name;m_aTokenDesc[ETN_##name].bString=true;m_aTokenDesc[ETN_##name].sName="ETN_"#name;
		#include "Tokens.inc"
#undef DEF_ETN
#undef DEF_ETN_STR

#define PREP_KW(kw)	m_mNamedPrepKW[#kw]=PKW_##kw;m_asPrepKWName[PKW_##kw]=#kw;
		#include "PrepKW.inc"
#undef PREP_KW

#define PREP_PRAGMA(pm) m_mNamedPrepPragmas[#pm]=PP_##pm;
		#include "PrepPragmas.inc"
#undef PREP_PRAGMA
	}

	std::string sTemp;
	const char *sText;
	HRSRC hRes=FindResource((HMODULE)pOwner->getHINSTANCE(),MAKEINTRESOURCE(IDR_LEX),"TEXT");
//	_ASSERTE(hRes);
	if (hRes)
		sText=(char *)LockResource(LoadResource((HMODULE)pOwner->getHINSTANCE(),hRes));
	else
	{
		decode(g_slex,sTemp);
		sText=sTemp.c_str();
	}
	m_pTMan=new CTokenMan(this,sText);
	AddTokensComments(m_pTMan);



	hRes=FindResource((HMODULE)pOwner->getHINSTANCE(),MAKEINTRESOURCE(IDR_LEX_PREP),"TEXT");
//	_ASSERTE(hRes);
	if (hRes)
		sText=(char *)LockResource(LoadResource((HMODULE)pOwner->getHINSTANCE(),hRes));
	else
	{
		decode(g_slex_prep,sTemp);
		sText=sTemp.c_str();
	}
	m_pPrepLexer=new CTokenMan(this,sText);
	AddTokensComments(m_pPrepLexer);

	hRes=FindResource((HMODULE)pOwner->getHINSTANCE(),MAKEINTRESOURCE(IDR_LEX_MACRO),"TEXT");
//	_ASSERTE(hRes);
	if (hRes)
		sText=(char *)LockResource(LoadResource((HMODULE)pOwner->getHINSTANCE(),hRes));
	else
	{
		decode(g_sLexMacro,sTemp);
		sText=sTemp.c_str();
	}
	m_pMacroLexer=new CTokenMan(this,sText);
	AddTokensComments(m_pMacroLexer);


	_ASSERTE(m_pTMan);
	_ASSERTE(m_pPrepLexer);
	_ASSERTE(m_pMacroLexer);


	m_pLastStopMacro=0;
	m_tLastStopKeyword=ETN_NONE;
}

CPreprocessor::~CPreprocessor()
{
	if (m_pTMan)
		delete m_pTMan;
	if (m_pPrepLexer)
		delete m_pPrepLexer;
	if (m_pMacroLexer)
		delete m_pMacroLexer;
}

void CPreprocessor::AddTokensComments(CTokenMan *pTMan)
{
	const auto &mTokens=pTMan->getTokens();
	for (auto &pair:	mTokens)
	if (pair.second->getID()<ETN_SIZE)
	{
		m_pOwner->GetErrors()->addTokenComment(pair.second->getID(),pair.second->getComment().c_str());
		m_asTokenComment[pair.second->getID()]=pair.second->getComment();
	}
}


bool CPreprocessor::onSuccessToken(CTState *pTState,void *pUserData)
{
	int nID=pTState->getID();
	SProcessData *pPD=(SProcessData *)pUserData;


	if (nID==ETN_INVALID)
		m_pOwner->ErrorLn(pPD->nLine,EERR_INV_CHAR);
	else
	if (nID==ETN_COMMENT_BLOCK)
	{
		if (pTState->getAcceptedChars().length()<2 || 
			pTState->getAcceptedChars().substr(pTState->getAcceptedChars().length()-2)!="*/")
				m_pOwner->ErrorLn(pPD->nLine,EERR_COMMENT_EOF);
	}
	else
	if (nID<ETN_SIZE)
	{
		TMMacros::iterator mit;

		if (nID==ETN_ID && (mit=m_mMacros.find(pTState->getAcceptedChars()))!=m_mMacros.end())
		{
			size_t n;
			for (n=0;n<pPD->aBlockingRanges.size();++n)
			{
				auto &BRange=pPD->aBlockingRanges[n];

				if (pPD->nCurBlockingChar-1>=BRange.first.first &&
					pPD->nCurBlockingChar-1<=BRange.first.second &&
					BRange.second==pTState->getAcceptedChars())
					break;
			}

			if (n>=pPD->aBlockingRanges.size())
			{
				m_tLastStopKeyword=(EXP_TOKEN)nID;
				m_pLastStopMacro=mit->second.get();
				m_nLastStopLine=pPD->nLine;
				return false;
			}
		}

		TToken *pT=0;
		std::string sStr;

		if (nID==ETN_CONST_STRING && 
			pPD->pStream->getTokens().size() && pPD->pStream->getTokens().back().T==ETN_CONST_STRING)
		{
			sStr=pPD->pStream->getTokens().back().sText;
			sStr.resize(sStr.length()-1);
			sStr+=pTState->getAcceptedChars().substr(1);
			pT=(TToken *)&pPD->pStream->getTokens().back();
		}


		if (!pT)
			pT=&pPD->pStream->addToken();
		
		pT->T=(EXP_TOKEN)pTState->getID();
		pT->nLine=pPD->nLine;
		pT->sText=0;
		pT->uLineOffset=pPD->uLineOffset;

		if (m_aTokenDesc[pT->T].bString)
		{
			const std::string *pStr=sStr.length()?&sStr:&pTState->getAcceptedChars();

			pT->sText=pPD->pStream->storeString(*pStr);
			if (!pT->sText)
				m_pOwner->ErrorLn(pPD->nLine,EERR_STR_BUF_LIMIT,pStr->c_str());
		}
	}

	return true;
}

void CPreprocessor::onErrorToken(CTState *pTState,void *pUserData,char nErrChar)
{
	int nID=pTState->getID();
	SProcessData *pPD=(SProcessData *)pUserData;

	if (((nID>ETN_CONST_FIRST && nID<ETN_CONST_LAST) || nID==ETN_PREP_STR || nID==ETN_PREP_FILE_STR) && (nErrChar=='\n' || nErrChar=='\r'))
		m_pOwner->ErrorLn(pPD->nLine,EERR_NEWLINE_CONST);
	else
	if (nID>ETN_CONST_CHAR && nID<ETN_CONST_LAST)	
		m_pOwner->ErrorLn(pPD->nLine,EERR_BAD_SUFFIX,(char *)nErrChar);//,pTState->getName().c_str());	
	else
	if (nID==ETN_CONST_CHAR && nErrChar=='\'')
		m_pOwner->ErrorLn(pPD->nLine,EERR_EMPTY_CHAR_CONST);
	else
		m_pOwner->ErrorLn(pPD->nLine,EERR_UNEXPECTED_CHAR,(char *)nErrChar);//,pTState->getName().c_str());
}

void CPreprocessor::logTMan(const char *sText,bool bError)
{
	__super::logTMan(sText,bError);
	OutputDebugString(sText);
}

int CPreprocessor::getNextStateID(const std::string &sStateName)
{
	auto it=m_mNamedTokens.find(sStateName);
	if (it!=m_mNamedTokens.end())
		return it->second;
		
	return __super::getNextStateID(sStateName)+ETN_SIZE;
}



CPreprocessor::PREP_KEYWORDS CPreprocessor::GetKeyword(const char *str,std::string &rsRet)
{
	const char *p=str+1;
	
	while ((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')
		++p;

	rsRet=std::string(str+1,p-str-1);
	
	
	auto it=m_mNamedPrepKW.find(rsRet);	
	if (it!=m_mNamedPrepKW.end())
		return it->second;


	return PKW_NONE;
}

void CPreprocessor::SetAdditionalDir(const std::string &sDir)
{
	std::string sDir_=sDir;
	char p[1024];
	_getcwd(p,sizeof(p));

	m_sAdditionalDir=p;
	m_sAdditionalDir+='\\';

	SCodeDependence::CorrectFileName(m_sAdditionalDir,true);

	if (sDir_.length())
	{
		SCodeDependence::CorrectFileName(sDir_,true);

		m_sAdditionalDir=SCodeDependence::MakePathFileName(m_sAdditionalDir,sDir_.c_str());		
		m_sAdditionalDir+='\\';

		SCodeDependence::CorrectFileName(m_sAdditionalDir,true);
	}
}

void CPreprocessor::Reset()
{
	m_aKeepDirective.clear();
	m_aSourceSectors.clear();
	m_sPragma="";
	m_sIncludeFile="";
	m_bIncludeAbs=false;
	m_aDependencies.clear();
	m_mMacros.clear();
	m_aIfBlocks.clear();
	m_sOpenedFiles.clear();

	for (TAPragmaScopes &raPS:	m_aaPragmaScopes)
		raPS.clear();
}

CPreprocessor::SPragmaScope *CPreprocessor::AddPragmaScope(PREP_PRAGMAS PP,int nTokenNum,bool bBegin)
{
	TAPragmaScopes &aScopes=m_aaPragmaScopes[PP];
	SPragmaScope *pRet=0;

	_ASSERTE(!aScopes.size() || nTokenNum>=aScopes.back().nBeginToken);

	if (!aScopes.size() || nTokenNum>=aScopes.back().nBeginToken)
	{
		if (bBegin)
		{
			aScopes.push_back(SPragmaScope(nTokenNum));
			pRet=&aScopes.back();
		}
		else		
		{
			int n=(int)aScopes.size()-1;

			while (n>=0 && aScopes[n].nEndToken!=-1)
				n--;

			if (n>=0)
			{
				aScopes[n].nEndToken=nTokenNum;
				pRet=&aScopes[n];
			}
		}
	}

	return pRet;
}

CPreprocessor::SPragmaScope *CPreprocessor::FindPragmaScope(PREP_PRAGMAS PP,int nTokenNum)
{
	TAPragmaScopes &aScopes=m_aaPragmaScopes[PP];
	SPragmaScope *pRet=0;
	int n;

	for (n=0;n<aScopes.size() && aScopes[n].nBeginToken<=nTokenNum;++n);

	if (n>0)
	{
		while (--n>=0 && aScopes[n].nEndToken<nTokenNum && aScopes[n].nEndToken>=0);

		if (n>=0)
			pRet=&aScopes[n];
	}

	return pRet;
}

void CPreprocessor::SkipFormatHdr(char *p)
{
	const char sUTF8[]={(char)0xEF,(char)0xBB,(char)0xBF};
	
	if (!strncmp(p,sUTF8,_countof(sUTF8)))
	{
		size_t len=strlen(p)-3;
		for (size_t n=0;n<=len;++n)
			p[n]=p[n+3];
	}
}


CPreprocessor::READ_FILE_RESULT CPreprocessor::ReadFile(const char *sFileName,const char *sDir,STokenStream &rDest,CFileHandler *pFH,int nLevel)
{
	READ_FILE_RESULT Ret=RFR_NOFILE;
	SProcessData PD;
	std::string sCompleteFN;
	if (sDir)
	{
		//sCompleteFN=std::string(sDir)+sFileName;
		//if (sDir[0])
			sCompleteFN=SCodeDependence::MakePathFileName(sDir,sFileName);
		//else
			//sCompleteFN=sFileName;

		//SCodeDependence::CorrectFileName(sCompleteFN,true);
	}
	else
		sCompleteFN=SCodeDependence::MakePathFileName(m_sAdditionalDir,sFileName);


	if (m_sOpenedFiles.find(sCompleteFN)!=m_sOpenedFiles.end())
	{
		TToken &rT=rDest.addToken();
		rT.T=ETN_NONE;
		rT.sText=0;
		rT.nLine=0;
		return RFR_OK;
	}
	m_sOpenedFiles.insert(sCompleteFN);

	auto it=std::find(m_aDependencies.begin(),m_aDependencies.end(),sCompleteFN);
	int nFileID;
	bool bWasFile=false;
	bool bCheckFormat=true;
	int nSectorBeginingLine;

	if (it!=m_aDependencies.end())
	{
		nFileID=int(it-m_aDependencies.begin());
		bWasFile=true;
	}
	else
	{
		nFileID=(int)m_aDependencies.size();
		m_aDependencies.push_back(SCodeDependence(sCompleteFN.c_str()));
	}

	if (!nLevel)
		rDest.reset();
	PD.pStream=&rDest;
	PD.nLine=int(nFileID<<24);
	nSectorBeginingLine=PD.nLine+1;

	m_pLastStopMacro=0;
	m_tLastStopKeyword=ETN_NONE;

	std::istream *f=pFH->OpenFileBIN(sCompleteFN.c_str());
	//FILE *f=fopen(sCompleteFN.c_str(),"r");
	
	InitDefaultMacros();

	if (f)
	{
		std::stringstream sStream;
		int nErrCnt=m_pOwner->GetErrorsCnt();
		char p[FILE_STR_SIZE];
		bool bFinish=false;

		if (!bWasFile)
			m_aDependencies[nFileID].uChangeDHMS=pFH->GetChangeDHMS(sCompleteFN.c_str());

		Ret=RFR_OK;
		m_pTMan->reset();

		while ((!f->eof() || !sStream.eof()) && Ret && !bFinish)
		{
			char C;
			bool bUnderComment=false;			
			CTState **apStates;
			if (m_pTMan->getActiveStates(&apStates)==1)
				bUnderComment=(apStates[0]->getID()==ETN_COMMENT_BLOCK);

			f->getline(p,sizeof(p)-1);
			if (!f->eof())
				strcat_s(p,"\n");

			if (bCheckFormat)
			{
				bCheckFormat=false;
				SkipFormatHdr(p);
			}
			
			sStream.get();
			if (sStream.eof())
			{
				sStream.str(p);

				char *pp=p;
				char C;
				PD.uLineOffset=0;
				do{
					C=*(pp++);

					if (C==' ')
						PD.uLineOffset++;
					else
					if (C=='\t')
						PD.uLineOffset+=1<<16;
				}while (C && C!='\r' && C!='\n' && C<=' ');

				PD.nLine++;
			}
			else
				sStream.unget();
			sStream.clear();

			m_bLastIf=false;

			if (!bUnderComment)
			{
				int nSymbolPos=0;
				while ((C=p[nSymbolPos])==' ' || C=='\t')
					nSymbolPos++;

				if (p[nSymbolPos]=='#')
				{
					if (p[nSymbolPos+1]=='#')	//Store preprocessor value
					{
						if (!m_aIfBlocks.size() || m_aIfBlocks.back().IsAllow())
						{
							nSymbolPos++;
							std::string s;
							PREP_KEYWORDS PKW=GetKeyword(p+nSymbolPos,s);

							sStream.seekg(nSymbolPos);
							if (PKW==PKW_NONE || PKW==PKW_include)
							{
								m_pOwner->ErrorLn(PD.nLine,EERR_PREP_INVALID,s.c_str());
								continue;
							}
							else
							{
								STokenStream sPrep;
								SProcessData PPD;

								sStream.seekg(s.length()+1,std::ios_base::cur);
								PPD.pStream=&sPrep;
								PPD.nLine=PD.nLine;

								if (ReadPreprocessor(PKW,f,sStream,&PPD))
									StoreDirective(PD.nLine,PKW,&PPD);
								else
									Ret=RFR_ERROR;

								PD.nLine=PPD.nLine;
							}							
						}
					}
					else
					{
						std::string s;
						PREP_KEYWORDS PKW=GetKeyword(p+nSymbolPos,s);

						sStream.seekg(nSymbolPos);

						if (PKW==PKW_NONE)
						{
							m_pOwner->ErrorLn(PD.nLine,EERR_PREP_INVALID,s.c_str());
							continue;
						}
						else
						if (!m_aIfBlocks.size() || m_aIfBlocks.back().IsAllow() ||
								(PKW>=PKW_ifdef && PKW<=PKW_endif))
						{
							STokenStream sPrep;
							SProcessData PPD;

							sStream.seekg(s.length()+1,std::ios_base::cur);
							PPD.pStream=&sPrep;
							PPD.nLine=PD.nLine;

							if (ReadPreprocessor(PKW,PKW==PKW_if?0:f,sStream,&PPD))
							{
								if (!ProcessKeyword(PKW,sPrep,sStream))
									Ret=RFR_ERROR;
								else
								{
									if (m_sIncludeFile.length())
									{
										std::string s=m_sIncludeFile,sDir;
										m_sIncludeFile="";
										READ_FILE_RESULT RRes;

										if (nSectorBeginingLine!=PD.nLine)
											m_aSourceSectors.push_back(std::make_pair(nSectorBeginingLine,PD.nLine));
										nSectorBeginingLine=PD.nLine+1;

										if (!m_bIncludeAbs)
										{
											sDir=sCompleteFN;
											sDir.resize(sDir.rfind('\\')+1);
										}

										if ((RRes=ReadFile(s.c_str(),m_bIncludeAbs?0:sDir.c_str(),rDest,pFH,nLevel+1))==RFR_OK)
											((TATokens &)rDest.getTokens()).pop_back();
										else
										{
											if (RRes==RFR_NOFILE)
												m_pOwner->ErrorLn(sPrep.getTokens()[0].nLine,EERR_NO_FILE,s.c_str());

											Ret=RFR_ERROR;
										}
									}

									if (m_sPragma.length())
									{
										PREP_PRAGMAS PP=GetPragma(m_sPragma);
										m_sPragma="";

										if (PP==PP_once)
											bFinish=bWasFile;
										else
											ExecPragma(PP,sPrep,rDest);
									}
								}
							}
							else
								Ret=RFR_ERROR;

							PD.nLine=PPD.nLine;
						}
					}
				}
			}

			int nAllT=(int)PD.pStream->getTokens().size();

			PD.nCurBlockingChar=0;
			PD.aBlockingRanges.clear();
		//Regular reading
			if (!m_aIfBlocks.size() || m_aIfBlocks.back().IsAllow())
			while (!bFinish && Ret && (C=sStream.get()) && !sStream.eof())
			{
				PD.nCurBlockingChar=(int)sStream.tellg()-1;
				CTokenMan::PROCESS_RESULT pRes=m_pTMan->process(C,&PD);
				
				if (!pRes)
				{
					Ret=RFR_ERROR;
					break;
				}

				if (pRes==CTokenMan::PRES_INTERRUPTED_UNPROC)
					sStream.unget();

				if (pRes==CTokenMan::PRES_INTERRUPTED_PROC || pRes==CTokenMan::PRES_INTERRUPTED_UNPROC)
				{
				//Macro substitution
					if (m_tLastStopKeyword==ETN_ID && m_pLastStopMacro)
					{
						Ret=(READ_FILE_RESULT)InsertMacro(PD,m_bLastIf?0:f,sStream);

						if (m_bLastIf)
							sStream.str(sStream.str()+" ");
					}

					m_pLastStopMacro=0;
					m_tLastStopKeyword=ETN_NONE;
				}
			}
			else
				sStream.setstate(std::ios::eofbit);


			if (f->eof())
			{
				CTokenMan::PROCESS_RESULT pRes=m_pTMan->process(0,&PD);

				if (pRes==CTokenMan::PRES_INTERRUPTED_PROC || pRes==CTokenMan::PRES_INTERRUPTED_UNPROC)
				{
				//Macro substitution
					if (m_tLastStopKeyword==ETN_ID && m_pLastStopMacro)
					{
						Ret=(READ_FILE_RESULT)InsertMacro(PD,m_bLastIf?0:f,sStream);

						if (m_bLastIf)
							sStream.str(sStream.str()+" ");
					}

					m_pLastStopMacro=0;
					m_tLastStopKeyword=ETN_NONE;
				}
				else
				if (!pRes)
					Ret=RFR_ERROR;
			}


			if (m_bLastIf)
			{
				SComValue val;

				if (PD.pStream->getTokens().size()==nAllT)
					m_aIfBlocks.back().Disallow();
				else
				{
					if(ParseIfExpression((TToken *)&PD.pStream->getTokens()[nAllT],(int)PD.pStream->getTokens().size()-nAllT,val))
					{
						if (!(int)val)
							m_aIfBlocks.back().Disallow();
					}
					else				
						m_pOwner->ErrorLn(PD.nLine,EERR_CONST_EXPECTED);
				}

				PD.pStream->reduceTokens(nAllT);
			}
		}

		if (f->eof() && m_aIfBlocks.size() && !nLevel)
			m_pOwner->ErrorLn(PD.nLine,EERR_EOF);


		pFH->CloseFileBIN(f);
		//fclose(f);
		TToken &rT=rDest.addToken();
		rT.T=ETN_NONE;
		rT.sText=0;
		rT.nLine=PD.nLine;

		if (nErrCnt!=m_pOwner->GetErrorsCnt())
			Ret=RFR_ERROR;
	}

	if (nSectorBeginingLine!=PD.nLine)
		m_aSourceSectors.push_back(std::make_pair(nSectorBeginingLine,PD.nLine));

	m_pLastStopMacro=0;
	m_tLastStopKeyword=ETN_NONE;
	m_pTMan->reset();

	m_sOpenedFiles.erase(sCompleteFN);

	return Ret;
}

int CPreprocessor::InsertMacro(SProcessData &PD,std::istream *f,std::stringstream &sStream)
{
	char C;
	READ_FILE_RESULT Ret=RFR_OK;
	CMacro *pMacro=dynamic_cast<CMacro *>(m_pLastStopMacro);
	int nArgsCount=pMacro?pMacro->GetArgsCount():0;
	bool bVarArgs=pMacro?pMacro->IsVarArgs():false;

	STokenStream sArgs;
	SProcessData PPD;

	PPD.pStream=&sArgs;
	PPD.nLine=PD.nLine;

	if (ReadMacroArgs(f,sStream,&PPD,nArgsCount,bVarArgs))
	{
		std::vector<TATokens> aaTokens,aaExpandedTokens;
		std::string sText;
		bool bUnrollMacro=true;

		GetArgs(sArgs,aaTokens);
		aaExpandedTokens.resize(aaTokens.size());

		for (size_t n=0;n<aaTokens.size();++n)
			ExpandArg(aaTokens[n],aaExpandedTokens[n]);

		if (!m_pLastStopMacro->BuildString(aaTokens.size()?&aaTokens[0]:0,aaExpandedTokens.size()?&aaExpandedTokens[0]:0,
											(int)aaTokens.size(),sText))
		{
			if (sArgs.getTokens().size()<=1 && dynamic_cast<CMacro *>(m_pLastStopMacro) &&
					dynamic_cast<CMacro *>(m_pLastStopMacro)->GetArgsCount())
				bUnrollMacro=false;
			else
				m_pOwner->ErrorLn(PD.nLine,EERR_MACRO_ARGS,m_pLastStopMacro->getName().c_str(),(char *)aaTokens.size());
		}

		if (bUnrollMacro)
		{
			if (PPD.nLine>PD.nLine)
			{
				PD.nCurBlockingChar=0;
				PD.aBlockingRanges.clear();
			}

			for (auto &pair:	PD.aBlockingRanges)
			{
				pair.first.first-=(int)sStream.tellg();
				pair.first.second-=(int)sStream.tellg();

				if (pair.first.second>=-1)
					pair.first.second+=(int)sText.length();
			}							

			if (sText.length())
				PD.aBlockingRanges.push_back(std::make_pair(std::make_pair(0,(int)sText.length()-1),m_pLastStopMacro->getName()));

			while ((C=sStream.get()) && !sStream.eof())
				sText+=C;

			sStream.str(sText);
			sStream.clear();
		}
		else
		{
			TToken *pT=&PD.pStream->addToken();	

			pT->T=ETN_ID;
			pT->nLine=PD.nLine;
			pT->sText=m_pLastStopMacro->getName().c_str();
		}
	}
	else
		Ret=RFR_ERROR;
	PD.nLine=PPD.nLine;

	return Ret;
}

bool CPreprocessor::ReadPreprocessor(PREP_KEYWORDS PKW,std::istream *f,std::stringstream &rsStream,SProcessData *pDest)
{
	bool bRet=true,bFinish=false;
	char p[FILE_STR_SIZE];

	if (PKW==PKW_include)
		m_pPrepLexer->enableState(ETN_PREP_FILE_STR);
	else
		m_pPrepLexer->disableState(ETN_PREP_FILE_STR);

	m_pPrepLexer->reset();

	do{
		char C;

		while (bRet && (C=rsStream.get()) && !rsStream.eof())
		{
			CTokenMan::PROCESS_RESULT PRes=m_pPrepLexer->process(C,pDest);
			
			if (!PRes)
			{
				bRet=false;
				break;
			}
			else
			if (pDest->pStream->getTokens().size() && pDest->pStream->getTokens().back().T==ETN_PREP_NEWLINE_STOP)
			{
				_ASSERTE(PRes==CTokenMan::PRES_PROCESSED);
				((TATokens &)pDest->pStream->getTokens()).pop_back();
				bFinish=true;
				break;
			}
		}

		if (!bFinish)
		{
			if (!f || f->eof())//feof(f))
			{
				m_pPrepLexer->process(0,pDest);
				bFinish=true;
			}
			else
			{
				f->getline(p,sizeof(p)-1);
				if (!f->eof())
					strcat_s(p,"\n");
				//if (!fgets(p,sizeof(p),f))
					//p[0]=0;

				rsStream.str(p);
				rsStream.clear();
				pDest->nLine++;
								
				CTState **apStates;
				if (m_pPrepLexer->getActiveStates(&apStates)!=1 || (apStates[0]->getID()!=ETN_COMMENT_BLOCK))
				{
					if (p[0]=='#')
						m_pOwner->ErrorLn(pDest->nLine,EERR_PREP_IN_DEF);
				}
			}
		}
	}while (!bFinish && bRet);

	TToken &rT=pDest->pStream->addToken();
	rT.T=ETN_NONE;
	rT.sText=0;
	rT.nLine=pDest->nLine;

	return bRet;
}

bool CPreprocessor::ProcessKeyword(PREP_KEYWORDS PKW,STokenStream &rSrc,std::stringstream &rsStream)
{
	switch (PKW)
	{
		case PKW_define:return ProcessDefine(rSrc);
			break;
		case PKW_undef:return ProcessUndef(rSrc);
			break;

		case PKW_if:return ProcessIf(rSrc,rsStream);
			break;
		case PKW_elif:return ProcessElif(rSrc,rsStream);
			break;

		case PKW_ifdef:
		case PKW_ifndef:return ProcessIfdef(rSrc,PKW==PKW_ifdef);
			break;

		case PKW_endif:return ProcessEndif(rSrc);
			break;
		case PKW_else:return ProcessElse(rSrc);
			break;

		case PKW_include:return ProcessInclude(rSrc);
			break;

		case PKW_pragma:return ProcessPragma(rSrc);
			break;
	}

	return true;
}

const std::string &CPreprocessor::GetTokenComment(EXP_TOKEN T)
{
	_ASSERTE(T<ETN_SIZE);
	return m_asTokenComment[T];
}

const CPreprocessor::STokenDesc &CPreprocessor::GetTokenDesc(EXP_TOKEN T)
{
	_ASSERTE(T<ETN_SIZE);
	return m_aTokenDesc[T];
}

bool CPreprocessor::ReadMacroArgs(std::istream *f,std::stringstream &rsStream,SProcessData *pDest,int nArgsCount,bool bVarArgs)
{
	bool bRet=true,bFinish=(nArgsCount==0 && !bVarArgs);
	char p[FILE_STR_SIZE],C=0;
	int nCBLevel=0;
	std::string sSpaces;
		
	//Skip spaces till '(' or something else
	while (!bFinish)
	{
		while (!rsStream.eof() && (C=rsStream.get()) && (unsigned char)C<=(unsigned char)' ')
			sSpaces+=C;

		
		if (rsStream.eof())
		{
			if (f && !f->eof())
			{
				f->getline(p,sizeof(p)-1);
				if (!f->eof())
					strcat_s(p,"\n");

				rsStream.str(p);
				rsStream.clear();
				pDest->nLine++;
			}
			else
				bFinish=true;
		}
		else
			bFinish=true;
	}	

	if (C!='(')
	{		
		if (!rsStream.eof() && (nArgsCount || bVarArgs))
		{
			rsStream.clear();
			rsStream.unget();

			if (sSpaces.length())
			if (!rsStream.tellg())
			{
				while ((C=rsStream.get()) && !rsStream.eof())
					sSpaces+=C;

				rsStream.str(sSpaces);
				rsStream.clear();
			}
			else			
				rsStream.unget();
		}

		TToken &rT=pDest->pStream->addToken();
		rT.T=ETN_NONE;
		rT.sText=0;
		rT.nLine=pDest->nLine;
		return true;
	}


	bFinish=false;
	m_pMacroLexer->reset();

	do{
		char C;

		while (bRet && (C=rsStream.get()) && !rsStream.eof())
		{
			size_t uBeforeTokensSize=pDest->pStream->getTokens().size();
			CTokenMan::PROCESS_RESULT PRes=m_pMacroLexer->process(C,pDest);

			if (!PRes)
			{
				bRet=false;
				break;
			}
			else
			if (pDest->pStream->getTokens().size() && !m_pMacroLexer->getActiveStates(0) &&
				pDest->pStream->getTokens().back().T>=ETN_PREP_MBRACEC_OPEN &&
				pDest->pStream->getTokens().back().T<=ETN_PREP_MBRACEC_CLOSE)
			{
				if (uBeforeTokensSize!=pDest->pStream->getTokens().size())
				{
				const TToken &rTLast=pDest->pStream->getTokens().back();

				if (rTLast.T==ETN_PREP_MBRACEC_OPEN)
					nCBLevel++;
				if (rTLast.T==ETN_PREP_MBRACEC_CLOSE)
					nCBLevel--;
				}

				if (nCBLevel<0)
				{
					_ASSERTE(PRes==CTokenMan::PRES_PROCESSED);
					((TATokens &)pDest->pStream->getTokens()).pop_back();
					bFinish=true;
					break;
				}
			}
		}

		if (!bFinish)
		{
			if (!f || f->eof())//feof(f))
			{
				m_pMacroLexer->process(0,pDest);
				bFinish=true;
			}
			else
			{
				f->getline(p,sizeof(p)-1);
				if (!f->eof())
					strcat_s(p,"\n");
//				if (!fgets(p,sizeof(p),f))
	//				p[0]=0;

				rsStream.str(p);
				rsStream.clear();
				pDest->nLine++;

				if (p[0]=='#')
				{
					_ASSERTE(false);
				}
			}
		}
	}while (!bFinish && bRet);

	TToken &rT=pDest->pStream->addToken();
	rT.T=ETN_NONE;
	rT.sText=0;
	rT.nLine=pDest->nLine;

	return bRet;
}

void CPreprocessor::ExpandArg(const TATokens &src,TATokens &dest)
{
	static STokenStream TStream;
	dest=src;

	for (int n=(int)dest.size()-1;n>=0;--n)	
	{
		TToken &rT=dest[n];
		//bool bExpanded=false;
		int m=n+1;

		if (rT.T==ETN_PREP_ID && m_mMacros.find(rT.sText)!=m_mMacros.end())
		{
			CBasicMacro *pBM=m_mMacros.find(rT.sText)->second.get();
			CMacro *pMacro=dynamic_cast<CMacro *>(pBM);
			STokenStream AllArgs;
			std::vector<TATokens> aaTokens;
			bool bArgsMatch;
			
			if (!pMacro || (!pMacro->GetArgsCount() && !pMacro->IsVarArgs()))
				bArgsMatch=true;
			else
			if (n<(int)src.size()-1 && src[n+1].T==ETN_PREP_MBRACEC_OPEN)
			{
				int nLevel=1;

				for (m=n+2;m<src.size() && nLevel;++m)
				{
					TToken &rT=dest[m];
					if (rT.T==ETN_PREP_MBRACEC_OPEN)
						nLevel++;
					else
					if (rT.T==ETN_PREP_MBRACEC_CLOSE)
						nLevel--;
					
					if (!aaTokens.size() && nLevel==1)
						aaTokens.resize(1);

					if (nLevel==1 && rT.T==ETN_PREP_COMMA)
						aaTokens.resize(aaTokens.size()+1);
					else
					if (nLevel>0)
						aaTokens.back().push_back(rT);
				}

				if (pMacro->IsVarArgs())
					bArgsMatch=(aaTokens.size()>=pMacro->GetArgsCount());
				else
				bArgsMatch=(aaTokens.size()==pMacro->GetArgsCount());
			}
			else
				bArgsMatch=false;

			if (bArgsMatch)
			{
				std::string s;

				if (pBM->BuildString(aaTokens.size()?&aaTokens[0]:0,0,(int)aaTokens.size(),s))
				{					
					SProcessData PD(&TStream);
					std::stringstream sStream("("+s+")");
					TStream.reduceTokens(0);

					if (ReadMacroArgs(0,sStream,&PD,1,false) && TStream.getTokens().size()>1)
					{
						dest.erase(dest.begin()+n,dest.begin()+m);
						for (int t=0;t<(int)TStream.getTokens().size()-1;++t)
						{
							dest.insert(dest.begin()+n+t,TStream.getTokens()[t]);
						}

						//bExpanded=true;
					}
				}
			}
		}
	}
}

void CPreprocessor::GetArgs(STokenStream &rSrc,std::vector<TATokens> &raaDest)
{
	TToken *pT;
	TATokens aT;
	
	do
	{
		int nCBLevel=0,nBLevel=0;
		aT.clear();

		while ((pT=rSrc.nextToken()) && pT->T==ETN_PREP_SPACE);

		if (pT->T!=ETN_NONE)
		{
			rSrc.revert();

			int nLastNotSpace=-1;
			while ((pT=rSrc.nextToken()) && (pT->T!=ETN_PREP_COMMA || nCBLevel || nBLevel) && pT->T!=ETN_NONE)
			{
				if (pT->T==ETN_PREP_MBRACEC_OPEN)
					nCBLevel++;
				if (pT->T==ETN_PREP_MBRACEC_CLOSE)
					nCBLevel--;

				if (pT->T==ETN_PREP_MBLOCK_OPEN)
					nCBLevel++;
				if (pT->T==ETN_PREP_MBLOCK_CLOSE)
					nCBLevel--;

				if (pT->T!=ETN_PREP_SPACE)
					nLastNotSpace=(int)aT.size();
				aT.push_back(*pT);
			}

			aT.resize(nLastNotSpace+1);
			raaDest.push_back(aT);
		}
	}while (pT->T!=ETN_NONE);
}

bool CPreprocessor::ProcessDefine(STokenStream &rSrc)
{
	TToken *pT=rSrc.nextToken();
	if (pT->T==ETN_PREP_SPACE)
	{
		pT=rSrc.nextToken();
		if (pT->T==ETN_PREP_ID)
		{
			TToken *ptID=pT;
			
			pT=rSrc.nextToken(false);
			if (pT->T==ETN_PREP_BRACEC_OPEN || pT->T==ETN_PREP_SPACE ||
				pT->T==ETN_NONE)
			{
				TAStrings asArgs;
				bool bVarArgs=false;

				if (pT->T!=ETN_NONE)
				{
					//while ((pT=rSrc.nextToken())->T==ETN_PREP_SPACE);
					pT=rSrc.nextToken();

					if (pT->T==ETN_PREP_BRACEC_OPEN)
					{
						while ((pT=rSrc.nextToken())->T==ETN_PREP_SPACE);
										
						while (pT->T!=ETN_PREP_BRACEC_CLOSE)
						{
							if ((pT->T==ETN_PREP_ID || pT->T==ETN_PREP_VAARG) && !bVarArgs)
							{
							if (pT->T==ETN_PREP_ID)
								asArgs.push_back(pT->sText);
								else
									bVarArgs=true;								

								while ((pT=rSrc.nextToken())->T==ETN_PREP_SPACE);

								if (pT->T!=ETN_PREP_COMMA && pT->T!=ETN_PREP_BRACEC_CLOSE)
								{
									m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
									return false;
								}

								if (pT->T==ETN_PREP_COMMA)
									while ((pT=rSrc.nextToken())->T==ETN_PREP_SPACE);
							}
							else
							{
								m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
								return false;
							}
						}


						while ((pT=rSrc.nextToken())->T==ETN_PREP_SPACE);
					}					
					rSrc.revert();
					if (rSrc.nextToken()->T!=ETN_PREP_SPACE)
						rSrc.revert();
				}

				
				

				m_mMacros[ptID->sText]=std::unique_ptr<CBasicMacro>(new CMacro(this,ptID->sText,&rSrc.getTokens()[rSrc.getPos()],
																	(int)rSrc.getTokens().size()-1-rSrc.getPos(),asArgs,bVarArgs));

				return true;
			}
		}
		else
		if (pT->T==ETN_NONE)
		{
			m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
			return false;
		}
	}
	else
	if (pT->T==ETN_NONE)
	{
		m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
		return false;
	}

	m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
	return false;
}


bool CPreprocessor::ProcessUndef(STokenStream &rSrc)
{
	TToken *pT=rSrc.nextToken();
	if (pT->T==ETN_PREP_SPACE)
	{
		pT=rSrc.nextToken();
		if (pT->T==ETN_PREP_ID)
		{
			TToken *ptID=pT;
			
			auto it=m_mMacros.find(ptID->sText);
			if (it!=m_mMacros.end())
				m_mMacros.erase(it);

			return true;
		}
		else
		if (pT->T==ETN_NONE)
		{
			m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
			return false;
		}
	}
	else
	if (pT->T==ETN_NONE)
		return true;

	m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
	return false;
}

bool CPreprocessor::ProcessIfdef(STokenStream &rSrc,bool bTrue)
{
	TToken *pT=rSrc.nextToken();
	if (pT->T==ETN_PREP_SPACE)
	{
		pT=rSrc.nextToken();
		if (pT->T==ETN_PREP_ID)
		{
			TToken *ptID=pT;
			
			auto it=m_mMacros.find(ptID->sText);
			m_aIfBlocks.push_back(SIfBlock(m_aIfBlocks.size()?m_aIfBlocks.back().IsAllow():true,(it!=m_mMacros.end())==bTrue));

			return true;
		}
		else
		if (pT->T==ETN_NONE)
		{
			m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
			return false;
		}
	}
	else
	if (pT->T==ETN_NONE)
		return true;

	m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
	return false;
}

bool CPreprocessor::ProcessIf(STokenStream &rSrc,std::stringstream &rsStream,bool bAutoEnd)
{
	std::string s;
	int pos=0;
	
	while (rSrc.nextToken()->T==ETN_PREP_SPACE);	
	
	rSrc.revert();
	while (rSrc.nextToken(false))
	{
		TToken *pT=rSrc.nextToken(true);
		
		if (pT->T==ETN_PREP_DEFINED)
		{
			pT->T=ETN_PREP_SPACE;	//Replace defined keyword with SPACE token
			pT=rSrc.nextToken(true);

			if (pT->T==ETN_PREP_SPACE || pT->T==ETN_PREP_BRACEC_OPEN)
			{
				bool bOpenedBrace=pT->T==ETN_PREP_BRACEC_OPEN;

				pT=rSrc.nextToken(true);
				if (pT->T==ETN_PREP_ID)
				{
					auto it=m_mMacros.find(pT->sText);
					if (it==m_mMacros.end())	//Replace macro ID with int value
						pT->sText=rSrc.storeString("0");
					else
						pT->sText=rSrc.storeString("1");
					pT->T=ETN_PREP_REST;

					if (bOpenedBrace)
					{
						pT=rSrc.nextToken(true);

						if (pT->T!=ETN_PREP_BRACEC_CLOSE)
						{
							m_pOwner->ErrorLn(pT->nLine,EERR_MISSING_CHAR,(char *)')');
							return false;
						}
					}
				}
				else
				{
					m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
					return false;
				}
			}
			else
			{
				m_pOwner->ErrorLn(pT->nLine,EERR_MISSING_CHAR,(char *)'(');
				return false;
			}
		}
		else
		if (pT->T==ETN_PREP_ID)
		{
			auto it=m_mMacros.find(pT->sText);
			if (it==m_mMacros.end())
			{
				pT->sText=rSrc.storeString("0");
				pT->T=ETN_PREP_REST;
			}
		}
	}


	for (const TToken &T:	rSrc.getTokens())
	{		
		if (GetTokenDesc(T.T).bString)
			s+=T.sText;
		else
			s+=GetTokenComment(T.T);
	}

	s+="\r\n";
	rsStream.str(s);
	rsStream.clear();

	m_aIfBlocks.push_back(SIfBlock(m_aIfBlocks.size()?m_aIfBlocks.back().IsAllow():true,true,bAutoEnd));
	m_bLastIf=true;
	return true;
}

bool CPreprocessor::ProcessElif(STokenStream &rSrc,std::stringstream &rsStream)
{
	if (!m_aIfBlocks.size() || m_aIfBlocks.back().Toggle())
	{
		m_pOwner->ErrorLn(rSrc.getTokens()[0].nLine,EERR_UNEXPECTED_STR,"#elif");
		return false;
	}

	return ProcessIf(rSrc,rsStream,true);
}

bool CPreprocessor::ProcessEndif(STokenStream &rSrc)
{
	if (m_aIfBlocks.size())
	{
		while (m_aIfBlocks.back().IsAutoEnd())
			m_aIfBlocks.pop_back();

		if (m_aIfBlocks.size())
		{
			m_aIfBlocks.pop_back();
			return true;
		}
		else
			m_pOwner->ErrorLn(rSrc.getTokens()[0].nLine,EERR_UNEXPECTED_STR,"#elif");
	}
	else
		m_pOwner->ErrorLn(rSrc.getTokens()[0].nLine,EERR_UNEXPECTED_STR,"#endif");

	return false;
}

bool CPreprocessor::ProcessElse(STokenStream &rSrc)
{
	if (!m_aIfBlocks.size() || m_aIfBlocks.back().Toggle())
	{
		m_pOwner->ErrorLn(rSrc.getTokens()[0].nLine,EERR_UNEXPECTED_STR,"#else");
		return false;
	}

	return true;
}

bool CPreprocessor::ProcessInclude(STokenStream &rSrc)
{
	TToken *pT=rSrc.nextToken();
	if (pT->T==ETN_PREP_SPACE)
	{
		pT=rSrc.nextToken();
		if (pT->T==ETN_PREP_STR || pT->T==ETN_PREP_FILE_STR)
		{
			TToken *ptStr=pT;
			std::string sFN=ptStr->sText+1;
			
			sFN.resize(sFN.size()-1);
			
			m_sIncludeFile=sFN;
			m_bIncludeAbs=(pT->T==ETN_PREP_FILE_STR);

			return true;
		}
		else
		if (pT->T==ETN_NONE)
		{
			m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_FILE_EXPECTED);
			return false;
		}
	}
	else
	if (pT->T==ETN_NONE)
		return true;

	m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
	return false;
}

bool CPreprocessor::ProcessPragma(STokenStream &rSrc)
{
	TToken *pT=rSrc.nextToken();
	if (pT->T==ETN_PREP_SPACE)
	{
		pT=rSrc.nextToken();
		if (pT->T==ETN_PREP_ID)
		{
			TToken *ptID=pT;

			m_sPragma=ptID->sText;

			return true;
		}
		else
		if (pT->T==ETN_NONE)
		{
			m_pOwner->ErrorLn(pT->nLine,EERR_MACRO_ID_EXPECTED);
			return false;
		}
	}
	else
	if (pT->T==ETN_NONE)
		return true;

	m_pOwner->ErrorLn(pT->nLine,EERR_UNEXPECTED_TOKEN,(char *)pT);
	return false;
}

CPreprocessor::PREP_PRAGMAS CPreprocessor::GetPragma(const std::string &sSrc)
{
	const char *p=sSrc.c_str();
	std::string sPragma;
	
	while ((*p>='a' && *p<='z') || (*p>='0' && *p<='9') || *p=='_')
		++p;

	sPragma=sSrc.substr(0,p-sSrc.c_str());
	
	auto it=m_mNamedPrepPragmas.find(sPragma);	
	if (it!=m_mNamedPrepPragmas.end())
		return it->second;

	return PP_NONE;
}

bool CPreprocessor::ExecPragma(PREP_PRAGMAS PP,STokenStream &rSrcArgs,STokenStream &rOutStream)
{
	switch (PP)
	{
		case PP_far_extern:return ProcessPragmaBeginEnd(PP,rSrcArgs,rOutStream);
			break;
	}

	return true;
}

bool CPreprocessor::ProcessPragmaBeginEnd(PREP_PRAGMAS PP,STokenStream &rSrcArgs,STokenStream &rOutStream)
{
	TToken *pT=rSrcArgs.nextToken();
	if (pT && pT->T==ETN_PREP_SPACE)
	{
		pT=rSrcArgs.nextToken();
		if (pT && pT->T==ETN_PREP_ID)
		{
			if (!strcmp(pT->sText,"begin"))
			{
				return AddPragmaScope(PP,(int)rOutStream.getTokens().size(),true)!=0;
			}
			else
			if (!strcmp(pT->sText,"end"))
			{
				return AddPragmaScope(PP,(int)rOutStream.getTokens().size(),false)!=0;
			}
		}
	}

	return false;
}

std::string CPreprocessor::GetShortFileName(const std::string &sSrc,int nSlashes)
{
	int pos=(int)sSrc.length()-1;

	while (nSlashes-->0 && pos>=0 && (pos=(int)sSrc.rfind('\\',pos))!=-1)
		pos--;
	
	return sSrc.substr(pos+1);
}

unsigned char HexVal(char C)
{
	return C<='9'?C-'0':C-'a'+10;
}

void CPreprocessor::decode(const char *sSrc,std::string &rsDest)
{
	size_t n,uLen=strlen(sSrc);
	_ASSERTE(uLen>>1<<1==uLen);

	rsDest="";

	for (n=0;n<uLen;n+=2)
	{
		char C=HexVal(sSrc[n])<<4 | HexVal(sSrc[n+1]);
		rsDest+=C;
	}
}

void CPreprocessor::InitDefaultMacros()
{
	if (m_mMacros.size())
		return;

	m_mMacros["__LINE__"]=std::unique_ptr<CBasicMacro>(new CLineMacro(this,"__LINE__"));
	m_mMacros["__FILE__"]=std::unique_ptr<CBasicMacro>(new CFileMacro(this,"__FILE__"));
	m_mMacros["__DATE__"]=std::unique_ptr<CBasicMacro>(new CDateMacro(this,"__DATE__"));
	m_mMacros["__TIME__"]=std::unique_ptr<CBasicMacro>(new CTimeMacro(this,"__TIME__"));

	for (auto &pair:	m_mDefinitions)
	if (m_mMacros.find(pair.first)==m_mMacros.end())	
		m_mMacros[pair.first]=std::unique_ptr<CBasicMacro>(new CTextMacro(this,pair.first.c_str(),pair.second.c_str()));
}

unsigned long CPreprocessor::GetDefinitionsHash()
{
	std::string s;
	for (auto &pair:	m_mDefinitions)
		s+=pair.first+"="+pair.second+";";

	return s.length()?(unsigned long)std::hash<std::string>()(s):0;
}




bool CPreprocessor::ParseIfExpression(TToken *aT,int nAllT,SComValue &rRet)
{
	rRet=m_pOwner->ParseConstExpression(aT,nAllT);
	return rRet.type()>CV_NULL;
}


void CPreprocessor::StoreDirective(int nLine,PREP_KEYWORDS PKW,SProcessData *pDest)
{
	std::string s;
	const TATokens &aTokens=pDest->pStream->getTokens();
	
	s="#"+m_asPrepKWName[PKW];

	for (size_t n=0;n<aTokens.size();++n)
	{		
		const TToken *pT=&aTokens[n];
		if (pT->T!=ETN_NONE)
		{
			_ASSERTE(pT->T>ETN_PREP_FIRST && pT->T<ETN_PREP_LAST);


			auto &TDesc=GetTokenDesc(pT->T);
			if (TDesc.bString)
				s+=pT->sText;
			else
			switch (pT->T)
			{
				case ETN_PREP_HASH:s+='#';
					break;
				case ETN_PREP_DOUBLEHASH:s+="##";
					break;

				default:s+=GetTokenComment(pT->T);
			}
		}
	}

	m_aKeepDirective.emplace_back(std::make_pair(nLine,s));
}