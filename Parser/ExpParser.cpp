#include "stdafx.h"

#include "ExpParser.h"
#include "PState.h"

#include <string>


#define LOG_ERROR(char_ptr_format,...)\
{\
	char sTemp[256];\
	sprintf_s(sTemp,sizeof(sTemp),char_ptr_format,__VA_ARGS__);\
	m_pHandler->logParser(sTemp,true);\
	_ASSERTE(false);\
}\

#define LOG(char_ptr_format,...)\
{\
	char sTemp[256];\
	sprintf_s(sTemp,sizeof(sTemp),char_ptr_format,__VA_ARGS__);\
	m_pHandler->logParser(sTemp,false);\
}\

CExpParser::CExpParser(CParseHandler *pHandler,FILE *f)
{
	m_pHandler=pHandler;
	if (!m_pHandler)
		m_pHandler=&m_LocalHandler;

	init(f);
}

CExpParser::CExpParser(CParseHandler *pHandler,const char *sRules)
{
	m_pHandler=pHandler;
	if (!m_pHandler)
		m_pHandler=&m_LocalHandler;
		

	init(sRules);
}

CExpParser::~CExpParser(void)
{
	m_mRules.clear();
	m_pRoot=PPState();

	_ASSERTE(!m_sAllocatedStates.size());
}

void CExpParser::onNew(CPState *pS)
{
#ifdef _DEBUG
	m_sAllocatedStates.insert(pS);
#endif
}
void CExpParser::onDelete(CPState *pS)
{
#ifdef _DEBUG
	_ASSERTE(m_sAllocatedStates.find(pS)!=m_sAllocatedStates.end());
	m_sAllocatedStates.erase(m_sAllocatedStates.find(pS));
#endif
}

void CExpParser::initByLine(const char *sLn)
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
			std::string sName,sComment;
			int pos=CPState::getStringSeparated(sLine,0,&sName,&sLineStrChars,'=');
			int posc;

			if ((posc=(int)sName.find(','))!=-1)
			{
				sComment=sName.substr(posc+1);
				sName.resize(posc);
			}

			if (!sName.length())
			{
				LOG_ERROR("Error: undefined rule name, %s\n",sLine.c_str());
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
			if (m_mRules.find(sName)==m_mRules.end())
			{
				std::string s=sLineStrChars.substr(pos+1);
				PPState pS=PPState(new CGroupPState(this,sName.c_str(),PSM_NONE,sLine.substr(pos+1),&s));
				//if (sComment.length())
					//pS->setComment(sComment.c_str());

				if (!m_pRoot)
					m_pRoot=pS;

				m_mRules.insert(std::make_pair(sName,pS));
			}
			else
			{
				LOG_ERROR("Error: duplicate rule name, %s\n",sLine.c_str())
			}
		}
	}		
}

void CExpParser::init(FILE *f)
{
	char p[256];

	while (!feof(f))
	{
		if (!fgets(p,sizeof(p),f))
			p[0]=0;
		
		initByLine(p);
	}

	for (auto &pair:	m_mRules)
	if (dynamic_cast<CGroupPState *>(pair.second.get()))
		((CGroupPState *)pair.second.get())->sortStates();
}

void CExpParser::init(const char *sRules)
{
	size_t sz=strlen(sRules),n;
	char p[512];

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

	
	for (auto &pair:	m_mRules)
	if (dynamic_cast<CGroupPState *>(pair.second.get()))
		((CGroupPState *)pair.second.get())->sortStates();
}

void CExpParser::log(const char *sText,bool bError)
{
	m_pHandler->logParser(sText,bError);
}

PPState CExpParser::getRule(const std::string &sName)
{
	TMRules::iterator it=m_mRules.find(sName);

	return it!=m_mRules.end()?it->second:PPState();
}

void CExpParser::formatStr(std::string &rStr,size_t pos)
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

bool CExpParser::checkBraces(const std::string &sStr,const std::string &sSCStr,const char *sBraces)
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

int CExpParser::getNextRuleID(const std::string &sStateName)
{
	return m_pHandler->getNextRuleID(sStateName);
}
int CExpParser::getNextTokenID(const std::string &sStateName)
{
	return m_pHandler->getNextTokenID(sStateName);
}

void CExpParser::reset()
{
}

int CExpParser::process(int *aTokens,int nAllT)
{
	if (m_pRoot)
	{
		TARulesStates aStates;
		int nRes=m_pRoot->process(aTokens,aTokens,nAllT,aStates);
		return nRes;
	}

	return -1;
}

void CExpParser::onSuccessRuleState(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT)
{
	m_pHandler->onSuccessRuleState(pTState,aStatesStack,nAllS,nStartTokenNum,nAllT);
}

void CExpParser::onSuccessRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT)
{
	m_pHandler->onSuccessRule(pTState,aStatesStack,nAllS,nStartTokenNum,nAllT);
}
bool CExpParser::onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nErrTokenNum,bool bProcessed)
{
	return m_pHandler->onErrorRule(pTState,aStatesStack,nAllS,nErrTokenNum,bProcessed);
}
