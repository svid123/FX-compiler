#pragma once

#include <string>
#ifndef _GAMING_XBOX
#include <d3dcompiler.h>
#else
//#include <dxcapi_xs.h>
#endif
#include "FXCode.h"
#include "FileHandler.h"

namespace fx
{

extern bool FXCompile(const char *sFXFile,const char *sAdditionalDir,unsigned int uFlags,std::string *asDefs,int nAllDefs,
						SFXCode &dest,std::string &rsErrors,
						CFileHandler *pFH=0,const char *sLogFileName=0);
}