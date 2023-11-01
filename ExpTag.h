#pragma once

#include <string>
#include <unordered_map>
#include "ComValue.h"


	
class CExpTag
{
	std::string m_sName;
public:

	CExpTag(const std::string &sName);
	virtual ~CExpTag();

	const std::string &GetName(){return m_sName;}
};




class CExpEnum:	public CExpTag
{
	typedef std::unordered_map<std::string,int> TMID;
	
	TMID m_mID;
	int m_nCurrentConst;
	
public:
	CExpEnum(const std::string &sName);
	virtual ~CExpEnum();

	bool AddID(const std::string &sIDName,int *pnConst);
	int *GetIDConst(const std::string &sIDName);
};