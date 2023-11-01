#include "stdafx.h"

#include "PState.h"
#include "ExpParser.h"

#include <minmax.h>

#include <algorithm> 

//#define STATES_SORT

CPState::CPState(CExpParser *pOwner,PSTATE_MODIFIER m,const char *sName)
{
	m_pOwner=pOwner;
	m_Mod=m;
	if (sName)
		m_sName=sName;
	m_pOwner->onNew(this);
	m_nID=m_pOwner->getNextRuleID(m_sName);
}

CPState::~CPState()
{
	m_pOwner->onDelete(this);
}

int CPState::getStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,const std::string *psSrcStrChars,
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


CLinkPState::CLinkPState(CExpParser *pOwner,PSTATE_MODIFIER m,const char *sName):
		CPState(pOwner,m,sName)
{
}
CLinkPState::~CLinkPState()
{
}

bool CLinkPState::validateRef()
{
	if (m_pRef.expired())
	{
		PPState pS=m_pOwner->getRule(m_sName);
		_ASSERTE(pS);
		if (!pS)
		{
			char p[256];
			sprintf_s(p,"Referenced rule '%s' not found",m_sName.c_str());
			m_pOwner->log(p,true);
			return false;
		}

		m_pRef=pS;
	}

	return true;
}

int CLinkPState::process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	if (!validateRef())
		return -1;

	return m_pRef.lock()->process(pT0,anTokens,nAllT,aStates);
}

int CLinkPState::getStartingToken()
{
	if (!validateRef())
		return 0;

	return m_pRef.lock()->getStartingToken();
}




CTokenPState::CTokenPState(CExpParser *pOwner,PSTATE_MODIFIER m,int nT):
		CPState(pOwner,m,"")
{
	m_nToken=nT;
}
CTokenPState::~CTokenPState()
{
}
int CTokenPState::process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	if (nAllT && anTokens[0]==m_nToken)
		return 1;
	else
		return -1;
}
int CTokenPState::getStartingToken()
{
	if (m_Mod==PSM_ONEMORE || m_Mod==PSM_NONE)
		return m_nToken;
	else
		return 0;
}






CTokenRangePState::CTokenRangePState(CExpParser *pOwner,PSTATE_MODIFIER m,const std::string &sFormat):
		CPState(pOwner,m,"")
{
	int pos=0,p;
	std::pair<short,short> MinMax;
	std::string s;

	do
	{
		int pos0=pos;
		pos=(int)sFormat.find(',',pos);
		if (pos!=-1)
		{
			s=sFormat.substr(pos0,pos-pos0);
			pos++;
		}
		else
			s=sFormat.substr(pos0);

		if (s.length())
		{
			if ((p=(int)s.find('-'))!=-1)
			{
				MinMax.first=pOwner->getNextTokenID(s.substr(0,p));
				MinMax.second=pOwner->getNextTokenID(s.substr(p+1));
			}
			else	
				MinMax.first=MinMax.second=pOwner->getNextTokenID(s);

			m_aTokenRanges.push_back(MinMax);
		}
	}while (pos!=-1 && pos<(int)sFormat.length());
}
CTokenRangePState::~CTokenRangePState()
{
}
int CTokenRangePState::getStartingToken()
{
	if ((m_Mod==PSM_ONEMORE || m_Mod==PSM_NONE) && m_aTokenRanges.size()==1 &&
		m_aTokenRanges[0].first==m_aTokenRanges[0].second)
		return m_aTokenRanges[0].first;
	else
		return 0;
}
int CTokenRangePState::process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	if (nAllT)
	{
		int nT=anTokens[0];
		int n;

		for (n=0;n<(int)m_aTokenRanges.size() && (nT<m_aTokenRanges[n].first || nT>m_aTokenRanges[n].second);++n);
		
		return n<(int)m_aTokenRanges.size()?1:-1;
	}
	else
		return -1;
}







CGroupPState::CGroupPState(CExpParser *pOwner,PSTATE_MODIFIER m):
	CPState(pOwner,m,0),m_bSorted(false)
{
}

CGroupPState::CGroupPState(CExpParser *pOwner,const char *sName,PSTATE_MODIFIER m,const std::string &rsFormat,const std::string *pStringChars):
	CPState(pOwner,m,sName),m_bSorted(false)
{
	size_t pos=0;
	std::string s,sSC;
	std::vector<bool> abOR;
	bool bOR;
	int nAllOR=0;

	_ASSERTE(!pStringChars || pStringChars->size()==rsFormat.size());
	

	while (pos<rsFormat.length())
	{
		CPState *pD=0;
		PSTATE_MODIFIER DM=PSM_NONE;
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
			DM=CPState::parseMod(rsFormat[pos+1]);
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
			case '{':{
						//PPState pNew(new CLinkPState(m_pOwner,PSM_NONE,s.c_str()));
						//pD=new CGroupPState(m_pOwner,DM,&pNew,1,false);
						pD=new CLinkPState(m_pOwner,DM,s.c_str());
						/*PPState pR=m_pOwner->getRule(s);
						if (pR)
						{
						//	PPState pNew(pR->createInstance());
							//pNew->copy(pR.get());
							
							//pD=new CGroupPState(m_pOwner,DM,&pNew,1,false);
						}*/
				}break;
			case '(':pD=new CGroupPState(m_pOwner,0,DM,s,sSC.length()?&sSC:0);
				break;
			case '[':pD=new CTokenRangePState(m_pOwner,DM,s);
				break;

			default:;			
		}

		_ASSERTE(pD);

		if (pD)
		{
			m_apState.push_back(PPState(pD));
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
				CGroupPState *pGD=new CGroupPState(m_pOwner,PSM_NONE,&m_apState[n],int(m-n+1),true);

				m_apState.erase(m_apState.begin()+n,m_apState.begin()+m);
				abOR.erase(abOR.begin()+n,abOR.begin()+m);

				m_apState[n]=PPState(pGD);
				abOR[n]=false;
			}

			pos=n;
		}

		nAllOR=0;
	}

	m_bOR=(nAllOR && nAllOR>=(int)m_apState.size()-1);
	m_nRep=0;
	m_nCurState=0;
	/*
	m_abStateEnabled.resize(m_apState.size());
	for (size_t n=0;n<m_abStateEnabled.size();++n)
		m_abStateEnabled[n]=true;
		*/
	nestModStates();
}

CGroupPState::CGroupPState(CExpParser *pOwner,PSTATE_MODIFIER m,PPState *apSrc,int nAllG,bool bOR):
	CPState(pOwner,m,0),m_bSorted(false)
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
	/*m_abStateEnabled.resize(m_apState.size());
	
	for (size_t n=0;n<m_abStateEnabled.size();++n)
		m_abStateEnabled[n]=true;
		*/
	nestModStates();
}

CGroupPState::~CGroupPState()
{
	m_apState.clear();
	//for (size_t n=0;n<m_apState.size();++n)
		//delete m_apState[n];
}

int CGroupPState::getStartingToken()
{
	if (!m_apState.size() || m_bOR || (m_Mod!=PSM_NONE && m_Mod!=PSM_ONEMORE))
		return 0;

	return m_apState[0]->getStartingToken();
}

void CGroupPState::sortStates()
{
#ifndef STATES_SORT
	return;
#endif

	if (m_bSorted)
		return;

	m_bSorted=true;

	for (auto &ptr:	m_apState)
	if (dynamic_cast<CGroupPState *>(ptr.get()))
		((CGroupPState *)ptr.get())->sortStates();

	if (!m_bOR)
		return;

	int n,m;

	for (n=0;n<(int)m_apState.size();++n)
		m_aSortedStates.push_back(std::make_pair(m_apState[n]->getStartingToken(),n));

	n=0;
	while (n<(int)m_apState.size())
	{
		SStatesChunk SC;
		SC.nStart=n;
		SC.bSorted=m_aSortedStates[n].first!=0;

		for (m=n+1;m<(int)m_apState.size() && (m_aSortedStates[m].first!=0)==SC.bSorted;++m);

		SC.nCount=m-n;
		m_aSChunks.push_back(SC);

		if (SC.bSorted && SC.nCount>1)
		for (int a=SC.nStart;a<SC.nStart+SC.nCount-1;++a)
		for (int b=a+1;b<SC.nStart+SC.nCount;++b)
		if (m_aSortedStates[a].first>m_aSortedStates[b].first)
		{
			auto tmp=m_aSortedStates[a];
			m_aSortedStates[a]=m_aSortedStates[b];
			m_aSortedStates[b]=tmp;
		}

		n=m;
	}
}

void CGroupPState::nestModStates()
{
	if (!m_bOR)
		return;

	size_t n;
	for (n=0;n<m_apState.size();++n)
	{
		PPState pD=m_apState[n];

		if (pD->getMod())
			m_apState[n]=PPState(new CGroupPState(m_pOwner,/*pD->getMod()==PSM_SCAN?PSM_SCAN:*/PSM_NONE,&pD,1,false));
	}
}

int CGroupPState::process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	_ASSERTE(nAllT);

	if (m_bOR)
	{
		int nRet=0;
		if (m_aSortedStates.size())
			nRet=processORChunks(pT0,anTokens,nAllT,aStates);
		else
			nRet=processOR(pT0,anTokens,nAllT,aStates);

		if (nRet>0)
		{
			if (m_sName.length())
			{
				aStates.resize(aStates.size()+1);
				SExpRuleState &rRS=aStates.back();
				rRS.pRule=this;

				m_pOwner->onSuccessRule(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),
												int(anTokens-pT0),nRet);

				aStates.pop_back();
			}
		}

		return nRet;
	}

	int *pEnterT=anTokens;
	int nOldAllT=nAllT;
	int nPrevRes=0;
	size_t n;
	bool bWasPush=false;

	if (m_sName.length())
	{
		bWasPush=true;
		aStates.resize(aStates.size()+1);
		SExpRuleState &rRS=aStates.back();
		rRS.pRule=this;
	}


	for (n=0;n<m_apState.size();++n)
	{
		CPState *pS=m_apState[n].get();
		int nRep=0;
		int *nStateT0=anTokens;
		int nStateAllT=nAllT;
		int nRes=0;

		if (bWasPush)
			aStates.back().nState=(int)n;

		do
		{
			nRes=pS->process(pT0,anTokens,nAllT,aStates);
			
			_ASSERTE(nRes);

			if (nRes>=0)
			{
				anTokens+=nRes;
				nAllT-=nRes;
			}
			else
			{
				if (nRes<-1)
				{
					int nAdvance=-(nRes+2);
					anTokens+=nAdvance;
					nAllT-=nAdvance;
				}
				//int nnn=pS->getMod();
				if (pS->getMod()==PSM_BACKSTATE && nPrevRes<-1 && n>=1)
				{
					nPrevRes=0;
					n-=2;
					break;
				}
				else
				{
					/*if (nRes==-2)
					{
						if (bWasPush)
							aStates.resize(aStates.size()-1);

						return nRes;
					}
					*/
					_ASSERTE(pS->getMod()!=PSM_BACKSTATE);
					if (pS->getMod()==PSM_NONE || (pS->getMod()==PSM_ONEMORE && !nRep))
					{
						if (nRes<-1)
							nRes=-2-(nOldAllT-nAllT);

						if (m_sName.length() && anTokens>pEnterT)
						{
							if (m_pOwner->onErrorRule(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),	int(anTokens-pT0),	nRes<-1))							
								nRes=-2-(nOldAllT-nAllT);							
						}

						if (bWasPush)
							aStates.resize(aStates.size()-1);

						return nRes;
					}
					else
						break;
				}
			}

			nRep++;
		}while (pS->getMod()!=PSM_NONE && pS->getMod()!=PSM_ZEROONE && nAllT && pS->getMod()!=PSM_BACKSTATE);
	
		if (bWasPush && nRep)
			m_pOwner->onSuccessRuleState(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),int(nStateT0-pT0),nStateAllT-nAllT);

		nPrevRes=nRes;
	}

	if (bWasPush)
	{
		m_pOwner->onSuccessRule(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),
										int(pEnterT-pT0),nOldAllT-nAllT);
	
		aStates.resize(aStates.size()-1);
	}

	return nOldAllT-nAllT;
}


int CGroupPState::processOR(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	int nOldAllT=nAllT;
	size_t n;
	int *pEnterT=anTokens;
	int nLowestErr=-1;
	bool bWasPush=false;

	if (m_sName.length())
	{
		bWasPush=true;
		aStates.resize(aStates.size()+1);
		SExpRuleState &rRS=aStates.back();
		rRS.pRule=this;
	}

	for (n=0;n<m_apState.size() && nLowestErr>=-1;++n)
	{
		CPState *pS=m_apState[n].get();
		int nRep=0;
		bool bErr=false;

		if (bWasPush)
			aStates.back().nState=(int)n;

		do
		{
			int nRes=pS->process(pT0,anTokens,nAllT,aStates);

			if (nRes>=0)
			{
				anTokens+=nRes;
				nAllT-=nRes;
			}
			else
			{
				nLowestErr=min(nLowestErr,nRes);

				if (nLowestErr<-1)
					nLowestErr=nLowestErr-(nOldAllT-nAllT);


				if (pS->getMod()==PSM_NONE || (pS->getMod()==PSM_ONEMORE && !nRep))				
					bErr=true;

				break;
			}

			nRep++;
		}while (pS->getMod()!=PSM_NONE && pS->getMod()!=PSM_ZEROONE);

		if (!bErr)
		{
			if (bWasPush)
			{
				m_pOwner->onSuccessRuleState(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),
														int(pEnterT-pT0),nOldAllT-nAllT);

				aStates.resize(aStates.size()-1);
			}

			return nOldAllT-nAllT;
		}
	}

	if (bWasPush)
		aStates.resize(aStates.size()-1);

	return nLowestErr;
}


int CGroupPState::processORChunks(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)
{
	int nOldAllT=nAllT;
	size_t n;
	int *pEnterT=anTokens;
	int nLowestErr=-1;
	bool bWasPush=false;

	if (m_sName.length())
	{
		bWasPush=true;
		aStates.resize(aStates.size()+1);
		SExpRuleState &rRS=aStates.back();
		rRS.pRule=this;
	}

	for (SStatesChunk &SC:	m_aSChunks)
	{
		size_t end=SC.nStart+SC.nCount;

		if (SC.bSorted)
		{
			int nFirstToken=pEnterT[0];
			auto it=std::lower_bound(m_aSortedStates.begin()+SC.nStart,m_aSortedStates.begin()+end,std::make_pair(nFirstToken,-1));

			if (it!=m_aSortedStates.begin()+end && it->first==nFirstToken)
			for (n=it-m_aSortedStates.begin();n<end && nLowestErr>=-1 && m_aSortedStates[n].first==nFirstToken;++n)
			{
				CPState *pS=m_apState[m_aSortedStates[n].second].get();
				int nRep=0;
				bool bErr=false;

				if (bWasPush)
					aStates.back().nState=(int)m_aSortedStates[n].second;

				do
				{
					int nRes=pS->process(pT0,anTokens,nAllT,aStates);

					if (nRes>=0)
					{
						anTokens+=nRes;
						nAllT-=nRes;
					}
					else
					{
						nLowestErr=min(nLowestErr,nRes);

						if (nLowestErr<-1)
							nLowestErr=nLowestErr-(nOldAllT-nAllT);


						if (pS->getMod()==PSM_NONE || (pS->getMod()==PSM_ONEMORE && !nRep))				
							bErr=true;

						break;
					}

					nRep++;
				}while (pS->getMod()!=PSM_NONE && pS->getMod()!=PSM_ZEROONE);

				if (!bErr)
				{
					if (bWasPush)
					{
						m_pOwner->onSuccessRuleState(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),
																int(pEnterT-pT0),nOldAllT-nAllT);

						aStates.resize(aStates.size()-1);
					}

					return nOldAllT-nAllT;
				}
			}
		}
		else
		for (n=SC.nStart;n<end && nLowestErr>=-1;++n)
		{
			CPState *pS=m_apState[m_aSortedStates[n].second].get();
			int nRep=0;
			bool bErr=false;

			if (bWasPush)
				aStates.back().nState=(int)m_aSortedStates[n].second;

			do
			{
				int nRes=pS->process(pT0,anTokens,nAllT,aStates);

				if (nRes>=0)
				{
					anTokens+=nRes;
					nAllT-=nRes;
				}
				else
				{
					nLowestErr=min(nLowestErr,nRes);

					if (nLowestErr<-1)
						nLowestErr=nLowestErr-(nOldAllT-nAllT);


					if (pS->getMod()==PSM_NONE || (pS->getMod()==PSM_ONEMORE && !nRep))				
						bErr=true;

					break;
				}

				nRep++;
			}while (pS->getMod()!=PSM_NONE && pS->getMod()!=PSM_ZEROONE);

			if (!bErr)
			{
				if (bWasPush)
				{
					m_pOwner->onSuccessRuleState(this,	aStates.size()?&aStates[0]:0,(int)aStates.size(),
															int(pEnterT-pT0),nOldAllT-nAllT);

					aStates.resize(aStates.size()-1);
				}

				return nOldAllT-nAllT;
			}
		}

		if (nLowestErr<-1)
			break;
	}

	if (bWasPush)
		aStates.resize(aStates.size()-1);

	return nLowestErr;
}
