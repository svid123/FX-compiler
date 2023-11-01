#pragma once


#include <string>

class CPState;
struct SExpRuleState;

class CParseHandler
{
	static int m_nGID;
public:
	virtual void onSuccessRuleState(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT);
	virtual void onSuccessRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNumFirst,int nAllT);
	virtual bool onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNum,bool bProcessed);

	virtual void logParser(const char *sText,bool bError);
	virtual int getNextRuleID(const std::string &sStateName);
	virtual int getNextTokenID(const std::string &sStateName);
};

