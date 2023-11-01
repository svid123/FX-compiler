#include "stdafx.h"
#include "FileHandler.h"


#include <minmax.h>
#include <windows.h>
#include <algorithm>
#include <fstream>

#include <sys/stat.h>
#include <time.h>



std::istream *CFileHandler::OpenFileBIN(const char *sFN)
{
	std::ifstream *pFile=new std::ifstream();
	pFile->open(sFN,std::ios_base::in | std::ios_base::binary);
	if (pFile->fail())
	{
		delete pFile;
		pFile=0;
	}

	return pFile;
}
void CFileHandler::CloseFileBIN(std::istream *pFile)
{
	if (pFile)
		delete (std::ifstream *)pFile;
}

unsigned long CFileHandler::GetChangeDHMS(const char *sFileName)
{
	unsigned long uRet=0;

	FILE *f=0;
	fopen_s(&f,sFileName,"rb");
	if (f)
	{
		struct stat st;
		if (!fstat(_fileno(f),&st))
		{
			struct tm T;
			gmtime_s(&T,&st.st_mtime);
			uRet=T.tm_sec | T.tm_min<<8 | T.tm_hour<<16 | T.tm_mday<<24;
		}

		fclose(f);
	}

	return uRet;
}
