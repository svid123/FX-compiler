#include "stdafx.h"

#include "ParseHandler.h"
//#include "common/Expressions/Parser/TState.h"

#include <stdio.h>

int CParseHandler::m_nGID=0;

int CParseHandler::getNextRuleID(const std::string &sStateName)
{
	return m_nGID++;
}
int CParseHandler::getNextTokenID(const std::string &sStateName)
{
	return m_nGID++;
}
void CParseHandler::logParser(const char *sText,bool bError)
{	
	printf(sText);
}

void CParseHandler::onSuccessRuleState(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nStartTokenNum,int nAllT)
{
}
void CParseHandler::onSuccessRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNumFirst,int nAllT)
{
	//printf("'%s': %s\n",pTState->getName().c_str(),pTState->getAcceptedChars().c_str());
}

bool CParseHandler::onErrorRule(CPState *pTState,SExpRuleState *aStatesStack,int nAllS,int nTokenNum,bool bProcessed)
{
	return false;
	//printf("Err '%s': %s, '%c'\n",pTState->getName().c_str(),pTState->getAcceptedChars().c_str(),nErrChar);
}
