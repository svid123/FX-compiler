#include "stdafx.h"
#include "Compile.h"

#include "Expressions.h"
#include "Preprocessor.h"


bool FXCompile(const char *sFXFile,const char *sAdditionalDir,unsigned int uFlags,std::string *asDefs,int nAllDefs,
				SFXCode &dest,std::string &rsErrors,CFileHandler *pFH,const char *sLogFileName)
{
	//CExpCompiler::setHINSTANCE((size_t)GetModuleHandle(0));
	CExpCompiler comp(sLogFileName);
	int nErrs=comp.GetErrorsCnt();
	
	comp.SetFileHandler(pFH);
	comp.GetPreprocessor()->SetAdditionalDir(sAdditionalDir);
	comp.ClearLog();

	comp.Compile(sFXFile,"",dest,asDefs,nAllDefs,uFlags);
	comp.CheckErrors(&rsErrors);

	return dest.bCompiled;
}