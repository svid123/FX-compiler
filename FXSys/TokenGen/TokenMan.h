#pragma once

#include <stdio.h>

#include <string>
#include <map>
#include <set>
#include <vector>
#include <memory>


#include "TokenHandler.h"

class CGroupTState;
class CTState;

typedef std::shared_ptr<CTState> PTState;


class CTokenMan
{
public:
	typedef std::map<std::string,	PTState> TMRules;

private:
	void initByLine(const char *sLn);
	void init(FILE *f);
	void init(const char *sRules);
	typedef struct{
					CTState *apTokens[24];
					unsigned char uAllT;
					}TATOKENS;

	typedef std::vector<PTState> TATokens;
	typedef std::vector<CTState *> TAPStates;
	
	TMRules m_mRules,m_mTokens;
	TAPStates m_apComplexTokens;
	TATOKENS m_aSimpleTokens[256];
	std::set<int> m_sDisabledStates;

	CTokenHandler m_LocalHandler,*m_pHandler;
	std::set<CTState *> m_sAllocatedTokens;
	
	TAPStates m_apActiveStates;
	size_t m_uFirstActiveState;
	
	CTState *m_pLastSuccess;
	bool m_bLastProcessed;

	void formatStr(std::string &rStr,size_t pos);
	void resetActiveStates();

	bool checkBraces(const std::string &sStr,const std::string &sSCStr,const char *sBraces);

public:
	enum PROCESS_RESULT
	{
		PRES_FAIL=0,
		PRES_PROCESSED,
		PRES_INTERRUPTED_UNPROC,
		PRES_INTERRUPTED_PROC,
	};

	CTokenMan(CTokenHandler *pHandler,const char *sRules);
	CTokenMan(CTokenHandler *pHandler,FILE *f);
	~CTokenMan(void);

	PTState getRule(const std::string &sName);
	
	PROCESS_RESULT process(char C,void *pUserData);	//returns true if C is processed

	int getNextID(const std::string &sStateName);

	void reset();

	void onNew(CTState *pS);
	void onDelete(CTState *pS);

	const TMRules &getTokens(){return m_mTokens;}
	size_t getActiveStates(CTState ***papRetStates);

	void disableState(int nID);
	void enableState(int nID);
};
