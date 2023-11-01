#pragma once

#include <string>
#include <vector>
#include <memory>

class CExpParser;
class CPState;

struct SExpRuleState
{
	CPState *pRule;
	int nState;
	size_t auParam[4];//kf_if: anParam[0] - false jump pos; anParam[1] - true jump pos; anParam[3] - returns count
					//block: anParam[3] - returns count
					//func_def: anParam[0] - bool bReturnsData
					//kw_for: anParam[0] - loop enter pos; anParam[1] - loop out jump pos; anParam[2] - loop skip jump pos; anParam[3] - returns count
					//ERULE_single_init_array: anParam[0] - SArrayItems *
					//ERULE_init_block: anParam[0] - SArrayItems *

	SExpRuleState()
	{
		pRule=0;
		nState=0;
		memset(auParam,0,sizeof(auParam));
	}
};
typedef std::vector<SExpRuleState> TARulesStates;

enum PSTATE_MODIFIER
{
	PSM_NONE=0,

	PSM_ZEROONE,	//?
	PSM_ZEROMORE,	//*
	PSM_ONEMORE,	//+
	PSM_BACKSTATE,	//< step back rule's state in case of this state error
	
	PSM_SIZE
};

class CPState
{
protected:
	PSTATE_MODIFIER m_Mod;
	std::string m_sName;
	CExpParser *m_pOwner;
	int m_nID;

public:
	CPState(CExpParser *pOwner,PSTATE_MODIFIER m,const char *sName);
	virtual ~CPState();

	virtual int process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates)=0;	//returns number of processed tokens, -1 if error (not processed), <-1 if error (processed)

	PSTATE_MODIFIER getMod()
	{
		return m_Mod;
	}

	int getID(){return m_nID;}

	virtual int getStartingToken(){return 0;}
	
	const std::string &getName()const{return m_sName;}

	static PSTATE_MODIFIER parseMod(char C)
	{
		if (C=='?')
			return PSM_ZEROONE;
		else
		if (C=='*')
			return PSM_ZEROMORE;
		else
		if (C=='+')
			return PSM_ONEMORE;
		else
		if (C=='<')
			return PSM_BACKSTATE;

		return PSM_NONE;
	}

	static int getStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,const std::string *psSrcStrChars,
		char nSeparator=',',const char *sLevelBraces="()");

};

typedef std::shared_ptr<CPState> PPState;



class CLinkPState:	public CPState	//Single token
{
	std::weak_ptr<CPState> m_pRef;

	virtual int getStartingToken();

	bool validateRef();
public:
	CLinkPState(CExpParser *pOwner,PSTATE_MODIFIER m,const char *sName);
	~CLinkPState();

	int process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);
};

class CTokenPState:	public CPState	//Single token
{
	int m_nToken;

	virtual int getStartingToken();
public:
	CTokenPState(CExpParser *pOwner,PSTATE_MODIFIER m,int nT);
	~CTokenPState();

	int process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);
};

class CTokenRangePState:	public CPState
{
	std::vector<std::pair<short,short>> m_aTokenRanges;

	virtual int getStartingToken();
public:
	CTokenRangePState(CExpParser *pOwner,PSTATE_MODIFIER m,const std::string &sFormat);
	~CTokenRangePState();

	int process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);
};



class CGroupPState:	public CPState
{
	struct SStatesChunk
	{
		bool bSorted;
		int nStart,nCount;

		SStatesChunk():bSorted(false),nStart(0),nCount(0)
		{
		}

		SStatesChunk(int _start,int _count,bool _sorted):bSorted(_sorted),nStart(_start),nCount(_count)
		{
		}
	};

	std::vector<PPState> m_apState;
	std::vector<SStatesChunk> m_aSChunks;
	std::vector<std::pair<int,int>> m_aSortedStates;
	//std::vector<bool> m_abStateEnabled;
	bool m_bOR,m_bSorted;
	int m_nCurState,m_nRep;
	
	int processORChunks(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);
	int processOR(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);
	void nestModStates();

	virtual int getStartingToken();
public:
	CGroupPState(CExpParser *pOwner,PSTATE_MODIFIER m);
	CGroupPState(CExpParser *pOwner,PSTATE_MODIFIER m,PPState *apSrc,int nAllG,bool bOR);
	CGroupPState(CExpParser *pOwner,const char *sName,PSTATE_MODIFIER m,const std::string &sFormat,const std::string *pStringChars=0);
	~CGroupPState();


	int process(int *pT0,int *anTokens,int nAllT,TARulesStates &aStates);

	void sortStates();
};
