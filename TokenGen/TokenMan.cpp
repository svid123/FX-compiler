#include "stdafx.h"

#include "TokenMan.h"
#include "TState.h"

#include <string>

int CTokenHandler::m_nGID=0;

#define LOG_ERROR(char_ptr_format,...)\
{\
	char sTemp[256];\
	sprintf_s(sTemp,sizeof(sTemp),char_ptr_format,__VA_ARGS__);\
	m_pHandler->logTMan(sTemp,true);\
	_ASSERTE(false);\
}\

#define LOG(char_ptr_format,...)\
{\
	char sTemp[256];\
	sprintf_s(sTemp,sizeof(sTemp),char_ptr_format,__VA_ARGS__);\
	m_pHandler->logTMan(sTemp,false);\
}\

CTokenMan::CTokenMan(CTokenHandler *pHandler,FILE *f)
{
	m_pHandler=pHandler;
	if (!m_pHandler)
		m_pHandler=&m_LocalHandler;

	m_uFirstActiveState=0;
	m_pLastSuccess=0;
	m_bLastProcessed=false;

	init(f);
}

CTokenMan::CTokenMan(CTokenHandler *pHandler,const char *sRules)
{
	m_pHandler=pHandler;
	if (!m_pHandler)
		m_pHandler=&m_LocalHandler;

	m_uFirstActiveState=0;
	m_pLastSuccess=0;
	m_bLastProcessed=false;

	init(sRules);
}

CTokenMan::~CTokenMan(void)
{
	m_mRules.clear();
	m_apComplexTokens.clear();
	m_mTokens.clear();

	_ASSERTE(!m_sAllocatedTokens.size());
}

void CTokenMan::onNew(CTState *pS)
{
#ifdef _DEBUG
	m_sAllocatedTokens.insert(pS);
#endif
}
void CTokenMan::onDelete(CTState *pS)
{
#ifdef _DEBUG
	_ASSERTE(m_sAllocatedTokens.find(pS)!=m_sAllocatedTokens.end());
	m_sAllocatedTokens.erase(m_sAllocatedTokens.find(pS));
#endif
}

void CTokenMan::initByLine(const char *sLn)
{
	std::string sLine,s,sLineStrChars;
	size_t len,n;
	std::string sChars;

	len=strlen(sLn);

	//sLineStrChars="";
	//sLine="";
	if (sLn[0]!=';')
	{
		char nStr=0;
		size_t uStrPos=0;

		for (n=0;n<len;++n)
		{
			char C=sLn[n];

			if (!nStr)
			{
				if (C=='"' || C==0x27)
				{
					nStr=C;
					C=0;
					uStrPos=sLine.size();
				}
			}
			else
			{
				if (C==nStr)
				{
					nStr=0;
					C=0;
					formatStr(sLine,uStrPos);
					sLineStrChars.resize(sLine.size());
				}
			}

			if (C && (nStr || (unsigned char)C>(unsigned char)' '))
			{
				sLine+=C;
				sLineStrChars+=nStr?'s':' ';
			}
		}

		_ASSERTE(!nStr);

		if (!checkBraces(sLine,sLineStrChars,"()"))
		{
			LOG_ERROR("Error: unmatched (): %s\n",sLine.c_str());
		}
		else
		if (!checkBraces(sLine,sLineStrChars,"[]"))
		{
			LOG_ERROR("Error: unmatched []: %s\n",sLine.c_str());
		}
		else
		if (!checkBraces(sLine,sLineStrChars,"{}"))
		{
			LOG_ERROR("Error: unmatched {}, %s\n",sLine.c_str());
		}
		else
		if (sLine.length())
		{
			if (sLine[0]=='{' || sLine[0]=='[')
			{
				std::string sName,sComment;
				int pos=CTState::getStringSeparated(sLine,1,&sName,&sLineStrChars,sLine[0]+2)+1;
				int posc;

				if ((posc=(int)sName.find(','))!=-1)
				{
					sComment=sName.substr(posc+1);
					sName.resize(posc);
				}

				if (!sName.length())
				{
					LOG_ERROR("Error: undefined rule/token name, %s\n",sLine.c_str());
				}
				else
				if (pos+1>=(int)sLine.length())
				{
					LOG_ERROR("Error: nothing to define, %s\n",sLine.c_str());
				}
				else
				if (sLine[pos]!='=')
				{
					LOG_ERROR("Error: = expected after '%c', %s\n",sLine[0]+2,sLine.c_str());
				}
				else
				if ((sLine[0]=='{' && m_mRules.find(sName)==m_mRules.end()) || (sLine[0]=='[' && m_mTokens.find(sName)==m_mTokens.end()))
				{
					std::string ss=sLineStrChars.substr(pos+1);
					PTState pS=PTState(new CGroupTState(this,sName.c_str(),TSM_NONE,sLine.substr(pos+1),&ss));
					if (sComment.length())
						pS->setComment(sComment.c_str());

					if (sLine[0]=='{')
						m_mRules.insert(std::make_pair(sName,pS));
					else
					{
						m_mTokens.insert(std::make_pair(sName,pS));

						sChars="";
						if (pS->getFirstChars(sChars))
						{
							for (size_t m=0;m<sChars.length();++m)
							{
								_ASSERTE((unsigned char)sChars[m]<_countof(m_aSimpleTokens));

								TATOKENS &rCharT=m_aSimpleTokens[(unsigned char)sChars[m]];

								if (rCharT.uAllT<_countof(rCharT.apTokens))
									rCharT.apTokens[rCharT.uAllT++]=pS.get();
								else
									_ASSERTE(false);
							}
						}
						else
							m_apComplexTokens.push_back(pS.get());
					}
				}
				else
				{
					if (sLine[0]=='{')
						LOG_ERROR("Error: duplicate rule name, %s\n",sLine.c_str())
					else
						LOG_ERROR("Error: duplicate token name, %s\n",sLine.c_str());
				}
			}
		}
	}		
}

void CTokenMan::init(FILE *f)
{
	char p[256];
	m_pLastSuccess=0;
	m_bLastProcessed=false;

	memset(m_aSimpleTokens,0,sizeof(m_aSimpleTokens));	

	while (!feof(f))
	{
		if (!fgets(p,sizeof(p),f))
			p[0]=0;
		
		initByLine(p);
	}

	resetActiveStates();
}

void CTokenMan::init(const char *sRules)
{
	size_t sz=strlen(sRules),n;
	char p[256];
	m_pLastSuccess=0;
	m_bLastProcessed=false;

	memset(m_aSimpleTokens,0,sizeof(m_aSimpleTokens));	

	n=0;
	while (n<sz)
	{
		const char *sNL=strchr(sRules+n,'\n');
		if (sNL)
		{
			int nLen=int(sNL-sRules-n);
			strncpy_s(p,sRules+n,nLen);
			p[nLen]=0;
			if (nLen && p[nLen-1]=='\r')
				p[nLen-1]=0;
			n=sNL-sRules+1;
		}
		else
		{
			strcpy_s(p,sRules+n);
			n=sz;
		}
		
		initByLine(p);
	}

	resetActiveStates();
}

void CTokenMan::resetActiveStates()
{
	m_uFirstActiveState=0;

	m_apActiveStates.clear();
	for (CTState *pS:	m_apComplexTokens)
	if (m_sDisabledStates.find(pS->getID())==m_sDisabledStates.end())
		m_apActiveStates.push_back(pS);
	/*
	if (!m_apActiveStates.size())
		m_apActiveStates=m_apComplexTokens;
	else
	{
		//_ASSERTE(!m_apComplexTokens.size());
		m_apActiveStates.resize(m_apComplexTokens.size());
		if (m_apComplexTokens.size())
			memcpy(&m_apActiveStates[0],&m_apComplexTokens[0],m_apComplexTokens.size()*sizeof(CTState *));
	}
	*/

	for (size_t n=0;n<m_apActiveStates.size();++n)	
		m_apActiveStates[n]->reset();
}

PTState CTokenMan::getRule(const std::string &sName)
{
	TMRules::iterator it=m_mRules.find(sName);

	return it!=m_mRules.end()?it->second:PTState();
}

void CTokenMan::formatStr(std::string &rStr,size_t pos)
{
	static const char *asPatterns[]={"\\""r","\\""n","\\""t","\\""\\","\\""x"};
	static const char anPValue[]={'\r','\n','\t','\\','x'};

	_ASSERTE(_countof(asPatterns)==_countof(anPValue));
	int n;
	for (n=0;n<_countof(asPatterns);++n)
	{
		const char *sP=asPatterns[n];
		char nReplaceC=anPValue[n];
		size_t uPLen=strlen(sP);


		bool bWas;
		do
		{
			size_t p;

			if (bWas=((p=rStr.find(sP,pos))!=-1))
			{
				char C=nReplaceC;

				if (nReplaceC=='x')
				{
					size_t m;
					char pp[8]={0};

					for (m=p+2;m<rStr.length() && m<p+4 && ((rStr[m]>='0' && rStr[m]<='9') || (rStr[m]>='a' && rStr[m]<='f'));++m)
						pp[m-p-2]=rStr[m];
					pp[m-p-2]=0;

					rStr.erase(p,m-p-1);
					C=(char)strtol(pp,0,16);
				}
				else				
					rStr.erase(p,uPLen-1);				

				rStr[p]=C;
			}
		}while (bWas);
	}
}

CTokenMan::PROCESS_RESULT CTokenMan::process(char C,void *pUserData)
{
	bool bProcessed=false;

	if (!m_mTokens.size())
		return PRES_PROCESSED;

	int nRep=0;

	while (!bProcessed && nRep<2)
	{
		bool bSuccessProc=m_bLastProcessed;//false;
		CTState *pSuccess=m_pLastSuccess,
			*pError=0;//,
			//*pPrevLastSuccess=m_pLastSuccess;
		int nSuccess=0,nError=0;
		size_t n;

		m_pLastSuccess=0;
		m_bLastProcessed=false;
		
		if (!m_uFirstActiveState)
		{
			TATOKENS &rCharT=m_aSimpleTokens[(unsigned char)C];
			if (rCharT.uAllT)
			{
				for (n=0;n<rCharT.uAllT;++n)
				if (m_sDisabledStates.find(rCharT.apTokens[n]->getID())==m_sDisabledStates.end())
				{
					m_apActiveStates.push_back(rCharT.apTokens[n]);
					m_apActiveStates.back()->reset();
				}
			}
		}

		n=m_uFirstActiveState;
		m_uFirstActiveState=m_apActiveStates.size();

		while (n<m_uFirstActiveState)
		{
			CTState *pS=m_apActiveStates[n];
			bool bProc=pS->process(C);
			bProcessed|=bProc;

			if (pS->isComplete())
			{
				//m_apActiveStates.erase(m_apActiveStates.begin()+n);

				if (pS->isSuccess())
				{
					if (!pSuccess || (int)bSuccessProc<=(int)bProc)
					{
						bSuccessProc=bProc;
						pSuccess=pS;
						nSuccess++;
					}
				}
				else
				{
					nError++;
					pError=pS;
				}
			}
			else
				m_apActiveStates.push_back(pS);

			++n;
		}

		



		if (m_uFirstActiveState==m_apActiveStates.size())
		{
			/*if (!pSuccess && pPrevLastSuccess)
			{
				pSuccess=pPrevLastSuccess;
			}*/


			if (pSuccess)
			{
			//	if (nSuccess)
			//		LOG_ERROR("Several tokens suits for pattern: ");

				if (!m_pHandler->onSuccessToken(pSuccess,pUserData))
				{
					reset();
					return bSuccessProc?PRES_INTERRUPTED_PROC:PRES_INTERRUPTED_UNPROC;
				}
			}
			else
			if (pError)
			{
			//	if (nError>1)
			//		LOG_ERROR("Several error tokens for pattern: ");

				m_pHandler->onErrorToken(pError,pUserData,C);
			}

			resetActiveStates();
			nRep++;
		}
		else
		{
			if (nSuccess && pSuccess && bSuccessProc)
			{
				m_pLastSuccess=pSuccess;
				m_bLastProcessed=bSuccessProc;
			}

			nRep=0;
		}
	}

	_ASSERTE(bProcessed);

	return bProcessed?PRES_PROCESSED:PRES_FAIL;
}

bool CTokenMan::checkBraces(const std::string &sStr,const std::string &sSCStr,const char *sBraces)
{
	size_t n;
	int nLevel=0;

	for (n=0;n<sStr.length() && nLevel>=0;++n)
	if (sSCStr[n]==' ')
	{
		char C=sStr[n];

		if (C==sBraces[0])
			nLevel++;
		else
		if (C==sBraces[1])
			nLevel--;
	}

	return nLevel==0;
}

int CTokenMan::getNextID(const std::string &sStateName)
{
	return m_pHandler->getNextStateID(sStateName);
}


void CTokenMan::reset()
{
	m_pLastSuccess=0;
	m_bLastProcessed=false;
	m_apActiveStates.clear();

	resetActiveStates();
}

size_t CTokenMan::getActiveStates(CTState ***papRetStates)
{
	if (m_apActiveStates.size()>m_uFirstActiveState && papRetStates)
		*papRetStates=&m_apActiveStates[m_uFirstActiveState];

	return m_apActiveStates.size()>m_uFirstActiveState?m_apActiveStates.size()-m_uFirstActiveState:0;
}

void CTokenMan::disableState(int nID)
{
	m_sDisabledStates.insert(nID);
}
void CTokenMan::enableState(int nID)
{
	auto it=m_sDisabledStates.find(nID);
	if (it!=m_sDisabledStates.end())
		m_sDisabledStates.erase(it);
}
