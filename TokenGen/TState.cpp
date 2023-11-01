#include "stdafx.h"

#include "TState.h"
#include "TokenMan.h"

#include <minmax.h>

//#include "SChar.h"


CTState::CTState(CTokenMan *pOwner,TSTATE_MODIFIER m,const char *sName)
{
	if (sName)
		m_sName=sName;
	m_pOwner=pOwner;
	m_Mod=m;
	m_bComplete=false;
	m_bSuccess=false;

	m_nID=m_pOwner->getNextID(m_sName);

	m_pOwner->onNew(this);
}
CTState::~CTState()
{
	m_pOwner->onDelete(this);
}

int CTState::getStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,const std::string *psSrcStrChars,
								char nSeparator,const char *sBraces)
{
	int n,m,nLevel=0;
	int sz=(int)strlen(sBraces);

	if (psDest)
		*psDest="";

	for (n=nPos;n<(int)rsSrc.length() && 
				(nLevel || (psSrcStrChars && (*psSrcStrChars)[n]!=' ') || rsSrc[n]!=nSeparator);++n)
	{
		char C=rsSrc[n];

		if (!psSrcStrChars || (*psSrcStrChars)[n]==' ')
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


CCharTState::CCharTState(CTokenMan *pOwner,TSTATE_MODIFIER m,char c):CTState(pOwner,m,0)
{
	m_nC=c;
}
CCharTState::~CCharTState()
{
}

void CCharTState::copy(const CTState *pSrc)
{
	__super::copy(pSrc);
	*this=*(CCharTState *)pSrc;
};

bool CCharTState::getFirstChars(std::string &rsRet)
{
	if (rsRet.find(m_nC)==-1)
		rsRet+=m_nC;

	return true;
}

bool CCharTState::process(char C)
{
	_ASSERTE(!m_bComplete);

	if (C==m_nC)
	{
		m_sResult+=C;
		m_bSuccess=true;
	}

	m_bComplete=true;
	return true;
}

CGroupTState::CGroupTState(CTokenMan *pOwner,TSTATE_MODIFIER m):
	CTState(pOwner,m,0)
{
}

CGroupTState::CGroupTState(CTokenMan *pOwner,const char *sName,TSTATE_MODIFIER m,const std::string &rsFormat,const std::string *pStringChars):
	CTState(pOwner,m,sName)
{
	size_t pos=0;
	std::string s,sSC;
	std::vector<bool> abOR;
	bool bOR;
	int nAllOR=0;

	_ASSERTE(!pStringChars || pStringChars->size()==rsFormat.size());
	

	while (pos<rsFormat.length())
	{
		CTState *pD=0;
		TSTATE_MODIFIER DM=TSM_NONE;
		size_t pos0=pos;
		char C=rsFormat[pos];
		char CStr=pStringChars?(*pStringChars)[pos]:' ';

		bOR=false;
		s=C;
		sSC="";
		if (CStr==' ')
		{
			if (C=='(')
				pos=getStringSeparated(rsFormat,(int)pos+1,&s,pStringChars,')',"()");
			else
			if (C=='{')
				pos=getStringSeparated(rsFormat,(int)pos+1,&s,pStringChars,'}',"{}");
			else
			if (C=='[')
				pos=getStringSeparated(rsFormat,(int)pos+1,&s,pStringChars,']',"[]");

			if (pStringChars)
				sSC=pStringChars->substr(pos0+1,pos-pos0-1);
		}
		else
			C=0;

		if (!pStringChars || (*pStringChars)[pos+1]==' ')
			DM=CTState::parseMod(rsFormat[pos+1]);
		if (DM)
			pos++;

		if (!pStringChars || (*pStringChars)[pos+1]==' ')
		if (rsFormat[pos+1]=='|')
		{
			bOR=true;
			pos++;
		}

		switch (C)
		{
			case '(':pD=new CGroupTState(m_pOwner,0,DM,s,sSC.length()?&sSC:0);
				break;
			case '{':{
						PTState pR=m_pOwner->getRule(s);
						if (pR)
						{
							PTState pNew(pR->createInstance());
							pNew->copy(pR.get());
							
							pD=new CGroupTState(m_pOwner,DM,&pNew,1,false);
						}						
					}
				break;
			case '[':pD=new CCharRangeTState(m_pOwner,DM,s,sSC.length()?&sSC:0);
				break;

			default:pD=new CCharTState(m_pOwner,DM,s[0]);

		}

		_ASSERTE(pD);

		if (pD)
		{
			m_apState.push_back(PTState(pD));
			abOR.push_back(bOR);
			nAllOR+=int(bOR);
		}

		pos++;
	}

	if (nAllOR && abOR.back())
	{
		nAllOR--;
		abOR.back()=false;
	}

	if (nAllOR && nAllOR<(int)m_apState.size()-1)
	{
		size_t n,m,pos=0;		

		while (pos<abOR.size())
		{
			for (n=pos;n<abOR.size() && !abOR[n];++n);
			for (m=n;m<abOR.size() && abOR[m];++m);

			if (m>n)
			{
				CGroupTState *pGD=new CGroupTState(m_pOwner,TSM_NONE,&m_apState[n],int(m-n+1),true);

				m_apState.erase(m_apState.begin()+n,m_apState.begin()+m);
				abOR.erase(abOR.begin()+n,abOR.begin()+m);

				m_apState[n]=PTState(pGD);
				abOR[n]=false;
			}

			pos=n;
		}

		nAllOR=0;
	}

	m_bOR=(nAllOR && nAllOR>=(int)m_apState.size()-1);
	m_nRep=0;
	m_nCurState=0;

	m_abStateEnabled.resize(m_apState.size());
	for (size_t n=0;n<m_abStateEnabled.size();++n)
		m_abStateEnabled[n]=true;

	wrapORGroups();
}

CGroupTState::CGroupTState(CTokenMan *pOwner,TSTATE_MODIFIER m,PTState *apSrc,int nAllG,bool bOR):
	CTState(pOwner,m,0)
{
	for (int n=0;n<nAllG;++n)
	{
		_ASSERTE(apSrc[n]);

		if (apSrc[n])
			m_apState.push_back(apSrc[n]);
	}
	

	m_bOR=bOR;
	m_nRep=0;
	m_nCurState=0;
	m_abStateEnabled.resize(m_apState.size());
	
	for (size_t n=0;n<m_abStateEnabled.size();++n)
		m_abStateEnabled[n]=true;

	wrapORGroups();
}

CGroupTState::~CGroupTState()
{
	m_apState.clear();
	//for (size_t n=0;n<m_apState.size();++n)
		//delete m_apState[n];
}

void CGroupTState::copy(const CTState *pSrc)
{
	__super::copy(pSrc);

	*this=*(CGroupTState *)pSrc;
	size_t n;
	for (n=0;n<m_apState.size();++n)
	{
		PTState pS(m_apState[n]->createInstance());
		pS->copy(m_apState[n].get());

		m_apState[n]=pS;
	}
}

void CGroupTState::reset()
{
	__super::reset();
	
	m_nCurState=0;
	m_nRep=0;

	for (size_t n=0;n<m_apState.size();++n)
	{
		m_apState[n]->reset();
		m_abStateEnabled[n]=true;
	}
}

bool CGroupTState::getFirstChars(std::string &rsRet)
{
	if (!m_apState.size())
		return false;

	if (m_bOR)
	{
		size_t n;
		std::string s;

		for (n=0;n<m_apState.size();++n)
		{
			CTState *pS=m_apState[n].get();

			if ((pS->getMod()==TSM_ZEROONE || pS->getMod()==TSM_ZEROMORE || pS->getMod()==TSM_SCAN) || !pS->getFirstChars(s))
				return false;
		}

		for (n=0;n<s.length();++n)
		if (rsRet.find(s[n])==-1)
			rsRet+=s[n];

		return true;
	}
	else
	{
		CTState *pS=m_apState[0].get();

		if ((pS->getMod()==TSM_ZEROONE || pS->getMod()==TSM_ZEROMORE || pS->getMod()==TSM_SCAN) || !pS->getFirstChars(rsRet))
			return false;
		else
			return true;
	}
}

bool CGroupTState::process(char C)
{
	_ASSERTE(!m_bComplete);

	if (m_bOR)
		return processOR(C);

	//std::string s;
	bool bProc=false;

	while(!bProc && m_nCurState<(int)m_apState.size())
	{
		//int nPrevDecl=m_nCurState;
		CTState *pD=m_apState[m_nCurState].get();

		bProc=pD->process(C);


		if (pD->isComplete())
		{
			if (pD->isSuccess())
			{
				m_nRep++;

				switch (pD->getMod())
				{					
					case TSM_ZEROONE:
					case TSM_SCAN:
					case TSM_NONE:{
									m_sResult+=pD->getAcceptedChars();
									m_nCurState++;
									m_nRep=0;
								 }break;

					case TSM_ONEMORE:
					case TSM_ZEROMORE:{										
										m_sResult+=pD->getAcceptedChars();
										pD->reset();
									}break;
				}
			}
			else
			{
				switch (pD->getMod())
				{
					case TSM_SCAN:
					case TSM_NONE:{
									m_bSuccess=false;
									m_bComplete=true;
									return m_nCurState==0 && bProc;
								 }break;

					case TSM_ONEMORE:{
										if (!m_nRep)
										{
											m_bSuccess=false;
											m_bComplete=true;
											return m_nCurState==0 && bProc;
										}
										else
										{
											m_nCurState++;
											m_nRep=0;
											bProc=false;
										}
									}break;


					case TSM_ZEROONE:{
										m_nCurState++;
										m_nRep=0;
										bProc=false;
									}break;

					case TSM_ZEROMORE:{
										m_nCurState++;
										m_nRep=0;
										bProc=false;
									 }break;
				}
			}
		}

//		_ASSERTE(bProc || nPrevDecl<m_nCurState);
	};



	if (m_nCurState>=(int)m_apState.size())
	{
		m_bComplete=true;
		m_bSuccess=true;
	}

	return bProc || !m_apState.size();
}

bool CGroupTState::processOR(char C)
{
	bool bEntireProc=false;//,bEnabledProc=false;
	int nStatesEnabled=(int)m_abStateEnabled.size();

	for (size_t n=0;n<m_abStateEnabled.size();++n)
	if (m_abStateEnabled[n])
	{
		int cnt=2;
		bool bProc;
		CTState *pD=m_apState[n].get();

		do
		{
			bProc=pD->process(C);

			bEntireProc|=bProc;

			if (pD->isComplete())
				if (pD->isSuccess())
				{
					if (pD->getMod()==TSM_SCAN)
						m_sResult=getIncompleteResult(this);					
					else
						m_sResult=pD->getAcceptedChars();

					m_bSuccess=true;
					m_bComplete=true;
					
					//bEntireProc=bProc;
					
					n=m_abStateEnabled.size();
					if (pD->getMod()!=TSM_SCAN)
						break;
				}
				else
				{
					if (pD->getMod()!=TSM_SCAN)
					{
						m_abStateEnabled[n]=false;
						nStatesEnabled--;
					}
					else
						pD->reset();
				}
		}while (!bProc && pD->getMod()==TSM_SCAN);// && --cnt);
	}
	else
		nStatesEnabled--;

	if (!nStatesEnabled)
	{
		m_bSuccess=false;
		m_bComplete=true;

		//return bEntireProc;
	}
//	else
//		return bEnabledProc;

	return bEntireProc;
}

const std::string &CGroupTState::getIncompleteResult(CTState *pS)
{
	CGroupTState *pGS=dynamic_cast<CGroupTState *>(pS);

	if (pGS && !pGS->isComplete())
	{
		if (pS->getAcceptedChars().length())
			return pS->getAcceptedChars();

		size_t m;
		for (m=0;m<pGS->m_abStateEnabled.size() && (!pGS->m_abStateEnabled[m] || pGS->m_apState[m]->getMod()==TSM_SCAN);++m);
		
		if (m<pGS->m_abStateEnabled.size())		//First enabled state, not TSM_SCAN
			return getIncompleteResult(pGS->m_apState[m].get());
		else
		{
			for (m=0;m<pGS->m_abStateEnabled.size() && !pGS->m_abStateEnabled[m];++m);

			if (m<pGS->m_abStateEnabled.size())		//Any enabled state
				return getIncompleteResult(pGS->m_apState[m].get());
			else
			{
				static std::string sEmpty;
				return sEmpty;
			}
		}
	}
	else
		return pS->getAcceptedChars();
}

void CGroupTState::wrapORGroups()
{
	if (!m_bOR)
		return;

	size_t n;
	for (n=0;n<m_apState.size();++n)
	{
		PTState pD=m_apState[n];

		if (pD->getMod())
			m_apState[n]=PTState(new CGroupTState(m_pOwner,pD->getMod()==TSM_SCAN?TSM_SCAN:TSM_NONE,&pD,1,false));
	}
}


CCharRangeTState::CCharRangeTState(CTokenMan *pOwner,TSTATE_MODIFIER m,const std::string &sFormat,const std::string *psStrChars):
		CTState(pOwner,m,0)
{	
	size_t n;

	std::string *pAnyChar=&m_sAnyChar;
	TARanges *paRanges=&m_aRanges;

	n=0;
	while (n<sFormat.size())
	{
		char C=sFormat[n];
		char StrC=psStrChars?(*psStrChars)[n]:' ';

		if (StrC==' ' && C=='-')
		{
			unsigned char uFirst=0,uLast=255;
			if ((*pAnyChar).length())
			{
				uFirst=(*pAnyChar)[(*pAnyChar).length()-1];
				(*pAnyChar).erase((*pAnyChar).length()-1);
			}

			if (n+1<sFormat.length() && ((sFormat[n+1]!='-' && sFormat[n+1]!='^') || (psStrChars && (*psStrChars)[n+1]=='s')))
			{
				uLast=sFormat[n+1];
				++n;
			}

			if (uFirst || uLast)			
				(*paRanges).push_back(std::make_pair(min(uFirst,uLast),max(uFirst,uLast)));
		}
		else
		if (StrC==' ' && C=='^')
		{
			pAnyChar=&m_sNotAnyChar;
			paRanges=&m_aNotRanges;
		}
		else
			(*pAnyChar)+=C;

		++n;
	}
}

CCharRangeTState::CCharRangeTState(CTokenMan *pOwner,TSTATE_MODIFIER m):CTState(pOwner,m,0)
{

}

CCharRangeTState::~CCharRangeTState()
{
}

void CCharRangeTState::copy(const CTState *pSrc)
{
	__super::copy(pSrc);
	*this=*(CCharRangeTState *)pSrc;
};

bool CCharRangeTState::inRanges(char C,TARanges &raRanges)
{
	size_t n;
	for (n=0;n<raRanges.size() && ((unsigned char)C<raRanges[n].first || (unsigned char)C>raRanges[n].second);++n);

	return n<raRanges.size();
}

bool CCharRangeTState::getFirstChars(std::string &rsRet)
{
	for (size_t n=0;n<m_aRanges.size();++n)
	{
		size_t uMin=m_aRanges[n].first,uMax=m_aRanges[n].second;

		for (size_t m=uMin;m<=uMax;++m)
		{
			char C=(char)m;

			if (!inRanges(C,m_aNotRanges) && m_sNotAnyChar.find(C)==-1 &&
				rsRet.find(C)==-1)	
				rsRet+=C;
		}
	}

	for (size_t n=0;n<m_sAnyChar.length();++n)
	{
		char C=m_sAnyChar[n];

		if (!inRanges(C,m_aNotRanges) && m_sNotAnyChar.find(C)==-1 &&
			rsRet.find(C)==-1)
			rsRet+=C;
	}

	return true;
}

bool CCharRangeTState::process(char C)
{
	_ASSERTE(!m_bComplete);

	if ((inRanges(C,m_aRanges) || m_sAnyChar.find(C)!=-1) &&
		!inRanges(C,m_aNotRanges) && m_sNotAnyChar.find(C)==-1)
	{
		m_sResult+=C;
		m_bSuccess=true;
	}

	m_bComplete=true;
	return true;
}
