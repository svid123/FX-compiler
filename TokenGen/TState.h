#pragma once

#include <string>
#include <vector>
#include <memory>



enum TSTATE_MODIFIER
{
	TSM_NONE=0,

	TSM_ZEROONE,	//?
	TSM_ZEROMORE,	//*
	TSM_ONEMORE,	//+
	
	TSM_SCAN,		//^

	TSM_SIZE
};

class CTokenMan;

class CTState	//Base Token-State
{	
protected:
	TSTATE_MODIFIER m_Mod;
	bool m_bComplete,m_bSuccess;
	std::string m_sResult,
			m_sName,m_sComment;
	CTokenMan *m_pOwner;
	int m_nID;

public:
	CTState(CTokenMan *pOwner,TSTATE_MODIFIER m,const char *sName);
	virtual ~CTState();

	virtual CTState *createInstance()=0;
	virtual void copy(const CTState *pSrc){*this=*pSrc;};

	virtual bool process(char C)=0;	//returns true if character C was processed
	virtual bool isComplete()
	{
		return m_bComplete;
	}
	bool isSuccess()
	{
		return m_bSuccess;
	}
	virtual void reset()
	{
		m_bSuccess=false;
		m_bComplete=false;
		m_sResult="";
	}
	virtual const std::string &getAcceptedChars()
	{
		return m_sResult;
	}

	TSTATE_MODIFIER getMod()
	{
		return m_Mod;
	}

	int getID(){return m_nID;}

	virtual bool getFirstChars(std::string &rsRet)=0;

	const std::string &getName()const{return m_sName;}
	const std::string &getComment()const{return m_sComment;}
	void setComment(const char *sC){m_sComment=sC;}

	static TSTATE_MODIFIER parseMod(char C)
	{
		if (C=='?')
			return TSM_ZEROONE;
		else
		if (C=='*')
			return TSM_ZEROMORE;
		else
		if (C=='+')
			return TSM_ONEMORE;
		else
		if (C=='^')
			return TSM_SCAN;

		return TSM_NONE;
	}
	static int getStringSeparated(const std::string &rsSrc,int nPos,std::string *psDest,const std::string *psSrcStrChars,
		char nSeparator=',',const char *sLevelBraces="()");
};

typedef std::shared_ptr<CTState> PTState;

class CCharTState:	public CTState	//Single character
{
	char m_nC;
	
	virtual bool getFirstChars(std::string &rsRet);
	virtual CTState *createInstance(){return new CCharTState(m_pOwner,getMod(),m_nC);}
	virtual void copy(const CTState *pSrc);
public:
	CCharTState(CTokenMan *pOwner,TSTATE_MODIFIER m,char Ñ);
	~CCharTState();

	bool process(char C);
};

class CCharRangeTState:	public CTState
{
	typedef std::pair<unsigned char,unsigned char> TMinMax;
	typedef std::vector<TMinMax> TARanges;

	std::string m_sAnyChar,m_sNotAnyChar;
	TARanges m_aRanges,m_aNotRanges;

	bool inRanges(char C,TARanges &raRanges);

	virtual bool getFirstChars(std::string &rsRet);
	virtual CTState *createInstance(){return new CCharRangeTState(m_pOwner,getMod());}
	virtual void copy(const CTState *pSrc);
public:
	//Format: A; A-Ca-cK-l; a-A^za-b'-'
	CCharRangeTState(CTokenMan *pOwner,TSTATE_MODIFIER m,const std::string &sFormat,const std::string *psStrChars);
	CCharRangeTState(CTokenMan *pOwner,TSTATE_MODIFIER m);
	~CCharRangeTState();

	bool process(char C);
};


class CGroupTState:	public CTState
{	
	std::vector<PTState> m_apState;
	std::vector<bool> m_abStateEnabled;
	bool m_bOR;
	int m_nCurState,m_nRep;
	
	bool processOR(char C);
	void wrapORGroups();

	virtual bool getFirstChars(std::string &rsRet);
	virtual CTState *createInstance(){return new CGroupTState(m_pOwner,getMod());}
	virtual void copy(const CTState *pSrc);

	const std::string &getIncompleteResult(CTState *pS);
public:
	CGroupTState(CTokenMan *pOwner,TSTATE_MODIFIER m);
	CGroupTState(CTokenMan *pOwner,TSTATE_MODIFIER m,PTState *apSrc,int nAllG,bool bOR);
	CGroupTState(CTokenMan *pOwner,const char *sName,TSTATE_MODIFIER m,const std::string &sFormat,const std::string *pStringChars=0);
	~CGroupTState();


	bool process(char C);
	void reset();
};
