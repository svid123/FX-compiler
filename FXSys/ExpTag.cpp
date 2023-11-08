#include "stdafx.h"

#include "ExpTag.h"

using namespace fx;


CExpTag::CExpTag(const std::string &sName):m_sName(sName)
{
}

CExpTag::~CExpTag()
{
}






CExpEnum::CExpEnum(const std::string &sName):CExpTag(sName)
{
	m_nCurrentConst=0;
}
CExpEnum::~CExpEnum()
{
}


bool CExpEnum::AddID(const std::string &sIDName,int *pnConst)
{
	int nConst=pnConst?*pnConst:m_nCurrentConst;
	
	if (m_mID.insert(std::make_pair(sIDName,nConst)).second)
	{
		m_nCurrentConst=nConst+1;
		return true;
	}

	return false;
}

int *CExpEnum::GetIDConst(const std::string &sIDName)
{
	auto it=m_mID.find(sIDName);
	return it!=m_mID.end()?&it->second:0;
}