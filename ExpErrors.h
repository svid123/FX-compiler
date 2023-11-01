#pragma once

#include <string>
#include <unordered_map>

#define DEF_ERR(code,text) EERR_##code,
enum EXP_ERRORS
{
	EERR_NONE=0,
#include "Errors.inc"
	EERR_SIZE
};
#undef DEF_ERR


class CExpErrors
{
	static std::unordered_map<EXP_ERRORS,std::string> m_mErrorStrings;

	std::unordered_map<int,std::string> m_mTokenComments;

public:
	CExpErrors();
	~CExpErrors();

	std::string formatError(EXP_ERRORS e,const char *str0,const char *str1);
	void addTokenComment(int T,const char *sComment);
};