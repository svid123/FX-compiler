#pragma once

#include <stdio.h>

#include <string>
#include <map>
#include <unordered_map>
#include <set>
#include <vector>
#include <memory>

#include "ParseHandler.h"

class CPState;

typedef std::shared_ptr<CPState> PPState;


class CExpParser
{
public:
	typedef std::unordered_map<std::string,	PPState> TMRules;

private:
	void initByLine(const char *sLn);
	void init(FILE *f);
	void init(const char *sRules);
		
	typedef std::vector<CPState *> TAPStates;
	
	PPState m_pRoot;
	TMRules m_mRules;
	std::set<CPState *> m_sAllocatedStates;
	
	TAPStates m_apActiveStates;
	size_t m_uFirstActiveState;

	CParseHandler m_LocalHandler,*m_pHandler;
	

	void formatStr(std::string &rStr,size_t pos);
	bool checkBraces(const std::string &sStr,const std::string &sSCStr,const char *sBraces);

public:
	CExpParser(CParseHandler *pHandler,const char *sRules);
	CExpParser(CParseHandler *pHandler,FILE *f);
	~CExpParser(void);

	PPState getRule(const std::string &sName);
	void log(const char *sText,bool bError);

	int getNextRuleID(const std::string &sStateName);
	int getNextTokenID(const std::string &sStateName);

	void onSuccessRuleState(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT);
	void onSuccessRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT);
	bool onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nErrTokenNum,bool bProcessed);

	void reset();

	void onNew(CPState *pS);
	void onDelete(CPState *pS);

	int process(int *aTokens,int nAllT);

	const TMRules &getRules(){return m_mRules;}
};
