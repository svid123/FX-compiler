#include "stdafx.h"

#include "CodeDependence.h"

#include <fstream>
#include <crtdbg.h>
#include <minmax.h>

using namespace fx;


bool SCodeDependence::Serialize(SSerializerIOContext &io)
{
	io<<sFileName;
	io<<uChangeDHMS;
	io<<bRelPath;

	_ASSERTE(sFileName.find(':')==-1);

	return true;
}

/*
bool SCodeDependence::UpdateFileChange(const std::string &sBaseDir)
{
	uChangeDHMS=0;

	std::string sFN;
	if (!bRelPath)
		sFN=sFileName;
	else
		sFN=MakePathFileName(sBaseDir,sFileName.c_str());
	

	FILE *f=fopen(sFN.c_str(),"rb");
	if (f)
	{
		struct stat st;
		if (!fstat(_fileno(f),&st))
		{
			struct tm *pT=gmtime(&st.st_mtime);
			uChangeDHMS=pT->tm_sec | pT->tm_min<<8 | pT->tm_hour<<16 | pT->tm_mday<<24;
		}

		fclose(f);
		return true;
	}

	return false;
}
*/

std::string SCodeDependence::ConvertFileNameRelative(const std::string &sBaseFN,const std::string &sFN)
{
	std::string sRet=sFN;
	size_t pos=0,end_=min(sFN.length(),sBaseFN.length()),n;
			
	for (n=0;n<end_ && sRet[n]==sBaseFN[n];++n)
	if (sRet[n]=='\\')
		pos=n+1;
						
	//pos=sRet.rfind('\\',pos)+1;
	sRet.erase(0,pos);

	while ((pos=sBaseFN.find('\\',pos))!=-1)
	{
		sRet.insert(0,"..\\");
		pos++;
	}

	return sRet;
}


std::string SCodeDependence::MakePathFileName(const std::string &sDir,const char *sFN)
{
	std::string s(sFN);
	int pos=0;

	CorrectFileName(s,false);

	if (s.length()<=1 || s[1]!=':')
		s=sDir+s;

	
	pos=0;
	do
	{
		int pos0=pos;
		pos=(int)s.find('\\',pos);

		if (pos!=-1)
		{
			if (pos-pos0==1 && s[pos0]=='.')
				s.erase(pos0,pos-pos0+1);				
			else
			if (pos-pos0==2 && s[pos0]=='.' && s[pos0+1]=='.')
			{
				if (pos0>=2)
				{
					if (s[pos0-2]==':')
					{
						s.erase(pos0,pos-pos0+1);
						pos=pos0;
					}
					else
					{
						int rpos=(int)s.rfind('\\',pos0-2);

						if (s.substr(rpos+1,pos0-1-(rpos+1))!="..")
						{
							s.erase(rpos+1,pos-rpos);
							pos=rpos+1;
						}
						else
							pos++;
					}
				}
				else
				{
					pos++;
					//s.erase(0,pos+1);
					//pos=0;
				}
			}
			else
				pos++;
		}
	}while (pos!=-1 && pos<(int)s.length());

	_strlwr_s((char *)s.c_str(),s.length()+1);
	return s;
}

void SCodeDependence::CorrectFileName(std::string &s,bool bLowercase)
{
	size_t pos;

	for (size_t n=0;n<s.length();++n)
	if (s[n]=='/')
		s[n]='\\';

	while ((pos=s.find("\\\\"))!=-1)
		s.replace(pos,2,1,'\\');

	if (bLowercase)
		_strlwr_s((char *)s.c_str(),s.length()+1);

	if (s[0]=='\\')
		s.erase(0,1);
}

