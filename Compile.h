#pragma once

#include <string>
#include <d3dcompiler.h>

#include "FXCode.h"
#include "FileHandler.h"


extern bool FXCompile(const char *sFXFile,const char *sAdditionalDir,unsigned int uFlags,std::string *asDefs,int nAllDefs,
						SFXCode &dest,std::string &rsErrors,
						CFileHandler *pFH=0,const char *sLogFileName=0);
