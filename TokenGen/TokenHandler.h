#pragma once


#include <string>

class CTState;

class CTokenHandler
{
	static int m_nGID;
public:
	virtual bool onSuccessToken(CTState *pTState,void *pUserData);	//returns false to interrupt parsing
	virtual void onErrorToken(CTState *pTState,void *pUserData,char nErrChar);

	virtual void logTMan(const char *sText,bool bError);
	virtual int getNextStateID(const std::string &sStateName);
};

