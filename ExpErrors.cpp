#include "stdafx.h"

#include "Expressions.h"
#include "ExpErrors.h"
#include <minmax.h>
#include <windows.h>
#include <algorithm>

std::unordered_map<EXP_ERRORS,std::string> CExpErrors::m_mErrorStrings;

CExpErrors::CExpErrors()
{
	if (!m_mErrorStrings.size())
	{
#define DEF_ERR(code,text) m_mErrorStrings[EERR_##code]=text;
#include "Errors.inc"
#undef DEF_ERR
	}
}

CExpErrors::~CExpErrors()
{
}

std::string CExpErrors::formatError(EXP_ERRORS e,const char *str0,const char *str1)
{
	char p[4096]="";
	auto it=m_mErrorStrings.find(e);
	
	if (it!=m_mErrorStrings.end())
	{
		std::string sFMT=it->second;
		const char *asStr[2]={str0,str1};
		std::string asTempStr[2];

		int tpos=(int)sFMT.find('%');
		if (tpos!=-1)
		{
			int n=0;
			//tpos=(int)sFMT.find('%');

			while (tpos!=-1 && n<_countof(asStr))
			{
				char &rC=sFMT[tpos+1];

				if (rC=='t')
				{
					CExpCompiler::TToken *pT=(CExpCompiler::TToken *)asStr[n];
					if (pT->sText && (pT->T<ETN_CONST_FIRST || pT->T>ETN_CONST_LAST))
						asStr[n]=pT->sText;
					else
						asStr[n]=m_mTokenComments[pT->T].c_str();					

					rC='s';
				}
				else
				if (rC=='s')
				{
					const char *pos=strchr(asStr[n],'@');
					if (pos)
					{						
						asTempStr[n].resize(pos-asStr[n]);
						strncpy_s((char *)asTempStr[n].c_str(),asTempStr[n].length()+1,asStr[n],asTempStr[n].length());
						
						asStr[n]=asTempStr[n].c_str();
					}
				}


				tpos=(int)sFMT.find('%',tpos+1);
				++n;
			}
		}

		sprintf_s(p,sFMT.c_str(),	asStr[0],asStr[1]);
	}

	return p;
}

void CExpErrors::addTokenComment(int T,const char *sComment)
{
	m_mTokenComments.insert(std::make_pair(T,sComment));
}