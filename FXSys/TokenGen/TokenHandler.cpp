#include "stdafx.h"

#include "TokenHandler.h"
#include "TState.h"

#include <stdio.h>


int CTokenHandler::getNextStateID(const std::string &sStateName)
{
	return m_nGID++;
}
void CTokenHandler::logTMan(const char *sText,bool bError)
{	
	printf(sText);
}
bool CTokenHandler::onSuccessToken(CTState *pTState,void *pUserData)
{
	printf("'%s': %s\n",pTState->getName().c_str(),pTState->getAcceptedChars().c_str());
	return true;
}

void CTokenHandler::onErrorToken(CTState *pTState,void *pUserData,char nErrChar)
{
	printf("Err '%s': %s, '%c'\n",pTState->getName().c_str(),pTState->getAcceptedChars().c_str(),nErrChar);
}
