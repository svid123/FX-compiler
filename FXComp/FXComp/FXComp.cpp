// FXComp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include "FXSys/Compile.h"
#include "FXSys/FXCode.h"
#include <direct.h>

#include "FXSys/CodeDependence.h"
#include "dxc/dxcapi.h"

std::string g_sDir;
std::string g_sIncludeDir;
std::vector<std::tuple<std::string,std::string,bool>> g_asFiles;
std::vector<std::string> g_asDefinitions;

DWORD g_uFlags=0;
bool g_bShowText=false;
std::string g_sLogFileName;

static const char *g_asOpt[][2]={{"/?","\t\tprint this message\n\n"},								
							{"/I","<include>\tadditional include path\n\n"},

							{"/Od","\t\tdisable optimizations\n"},
							{"/Op","\t\tdisable preshaders\n"},
							{"/O0","\t\toptimization level 0.  1 is default\n"},
							{"/O1","\t\toptimization level 1\n"},
							{"/O2","\t\toptimization level 2\n"},
							{"/O3","\t\toptimization level 3\n"},
							{"/WX","\t\ttreat warnings as errors\n"},
							{"/Vd","\t\tdisable validation\n"},
							{"/Zi","\t\tenable debugging information\n"},
							{"/Zpr","\t\tpack matrices in row-major order\n"},
							{"/Zpc","\t\tpack matrices in column-major order\n\n"},

							{"/Gpp","\t\tforce partial precision\n"},
							{"/Gfa","\t\tavoid flow control constructs\n"},
							{"/Gfp","\t\tprefer flow control constructs\n"},
							{"/Ges","\t\tenable strict mode\n"},
							{"/Gec","\t\tenable backwards compatibility mode\n"},
							{"/Gis","\t\tforce IEEE strictness\n\n"},

							{"/Fo","<file>\toutput object file\n"},
							{"/Fc","<file>\toutput assembly code listing file\n"},
							{"/l","<file>\toutput log file\n"},
							{"/t","\t\toutput source text\n"},
							{"/D","<name>[=<value>]\tset definition"}};

void PrintHelp()
{
	int n;
	printf("Usage: fxcomp <options> <files>\n\n");

	for (n=0;n<_countof(g_asOpt);++n)
		printf("%s%s",g_asOpt[n][0],g_asOpt[n][1]);
}

void ProcessArg(const char *sArg)
{
	int n;
	std::string s(sArg);

	for (n=0;n<_countof(g_asOpt) && strncmp(sArg,g_asOpt[n][0],strlen(g_asOpt[n][0]));++n);

	s=s.substr(strlen(g_asOpt[n][0]));

	switch (n)
	{
		case 0:PrintHelp();
			break;

		case 1:g_sIncludeDir=SCodeDependence::MakePathFileName(g_sDir,s.c_str());
			break;

		case 2:g_uFlags|=D3DCOMPILE_SKIP_OPTIMIZATION;
			break;
		case 3:g_uFlags|=D3DCOMPILE_NO_PRESHADER;
			break;

		case 4:g_uFlags|=D3DCOMPILE_OPTIMIZATION_LEVEL0;
			break;
		case 5:g_uFlags|=D3DCOMPILE_OPTIMIZATION_LEVEL1;
			break;
		case 6:g_uFlags|=D3DCOMPILE_OPTIMIZATION_LEVEL2;
			break;
		case 7:g_uFlags|=D3DCOMPILE_OPTIMIZATION_LEVEL3;
			break;

		case 8:g_uFlags|=D3DCOMPILE_WARNINGS_ARE_ERRORS;
			break;
		case 9:g_uFlags|=D3DCOMPILE_SKIP_VALIDATION;
			break;
		case 10:g_uFlags|=D3DCOMPILE_DEBUG;
			break;
		case 11:g_uFlags|=D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
			break;
		case 12:g_uFlags|=D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
			break;

		case 13:g_uFlags|=D3DCOMPILE_PARTIAL_PRECISION;
			break;
		case 14:g_uFlags|=D3DCOMPILE_AVOID_FLOW_CONTROL;
			break;
		case 15:g_uFlags|=D3DCOMPILE_PREFER_FLOW_CONTROL;
			break;
		case 16:g_uFlags|=D3DCOMPILE_ENABLE_STRICTNESS;
			break;
		case 17:g_uFlags|=D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
			break;
		case 18:g_uFlags|=D3DCOMPILE_IEEE_STRICTNESS;
			break;

		case 19:if (g_asFiles.size())
				{
					std::get<1>(g_asFiles.back())=s;
					std::get<2>(g_asFiles.back())=true;
				}
			break;
		case 20:if (g_asFiles.size())
				{
					std::get<1>(g_asFiles.back())=s;
					std::get<2>(g_asFiles.back())=false;
				}
			break;

		case 21:g_sLogFileName=s;
			break;

		case 22:g_bShowText=true;
			break;

		case 23:g_asDefinitions.emplace_back(s);				
			break;
	}
}

void Disassemble(SFXCode &src,std::string &dest)
{
	static const char *asShaderName[SFXPass::FXS_SIZE]={"VertexShader","PixelShader","GeometryShader","HullShader","DomainShader","ComputeShader"};
	std::vector<std::tuple<std::string,int,int,int>> aParamRanges;
	char p[32];
	dest="";

	for (auto &pair:	src.mTech)
	{
		dest+="technique "+pair.first+"\n{\n";
		for (SFXPassGroup &rPG:	pair.second.aPassG)
		{
			dest+="\tpass_group "+rPG.sName+"\n\t{\n";

			aParamRanges.resize(rPG.mParamRange.size());
			for (auto &pair:	rPG.mParamRange)
			{
				std::get<0>(aParamRanges[pair.second.uIndex])=pair.first;
				std::get<1>(aParamRanges[pair.second.uIndex])=pair.second.nMin;
				std::get<2>(aParamRanges[pair.second.uIndex])=pair.second.nMax;
				std::get<3>(aParamRanges[pair.second.uIndex])=pair.second.nMin;
			}


			for (std::unique_ptr<SFXPass> &rpPass:	rPG.apPass)
			{
				dest+="\t\tpass(";
				for (auto &tp:	aParamRanges)
				{
					dest+=std::get<0>(tp)+"=";
					_itoa_s(std::get<3>(tp),p,10);
					dest+=p;
					if (size_t(&tp-&aParamRanges[0])<aParamRanges.size()-1)
						dest+=", ";
				}				
				dest+=")\n\t\t{\n";

				for (int n=0;n<SFXPass::FXS_SIZE;++n)
				if (rpPass->aShaders[n].size())
				{
					dest+="\t\t\t";
					dest+=asShaderName[n];
					dest+="\n\t\t\t{\n";

					std::string sDisass;
					
					ID3DBlob *pText=0;
					D3DDisassemble(&rpPass->aShaders[n][0],rpPass->aShaders[n].size(),0,0,&pText);
					if (pText)
					{
						sDisass=(char *)pText->GetBufferPointer();
						pText->Release();
					}

					if (!sDisass.length())
					{
						IDxcCompiler3 *pCompiler=0;
						IDxcResult *pRes=0;
						IDxcBlobUtf8 *pBlob=0;
						
						DxcCreateInstance(CLSID_DxcCompiler,IID_PPV_ARGS(&pCompiler));

						DxcBuffer sourceBuffer={};
						sourceBuffer.Ptr=&rpPass->aShaders[n][0];
						sourceBuffer.Size=rpPass->aShaders[n].size();
						HRESULT hr=pCompiler->Disassemble(&sourceBuffer,IID_PPV_ARGS(&pRes));
						_ASSERTE(hr==S_OK);
						
						if (pRes)
						{
							hr=pRes->GetOutput(DXC_OUT_DISASSEMBLY,IID_PPV_ARGS(&pBlob),nullptr);
							_ASSERTE(hr==S_OK);

							sDisass=(char *)pBlob->GetBufferPointer();
							pBlob->Release();
							pRes->Release();
						}
						pCompiler->Release();
					}

					if (sDisass.length())
					{
						const char *sText=sDisass.c_str(),*sEOL;

						while (sText[0])
						{
							dest+="\t\t\t\t";
							sEOL=strchr(sText,'\n');
							if (!sEOL)
							{
								dest+=sText;
								sText+=strlen(sText);
							}
							else
							{
								dest+=std::string(sText,sEOL-sText+1);
								sText=sEOL+1;
							}
						}

						
					}

					dest+="\t\t\t}\n\n";
				}				

				dest+="\t\t}\n\n";

				bool bOverflow=true;
				int nParam=0;
				while(bOverflow && nParam<(int)aParamRanges.size())
				{
					int &rVal=std::get<3>(aParamRanges[nParam]);
					rVal++;

					if (rVal>std::get<2>(aParamRanges[nParam]))
					{
						rVal=std::get<1>(aParamRanges[nParam]);
						nParam++;
						bOverflow=true;
					}
					else
						bOverflow=false;					
				}
			}
			dest+="\t}\n\n";
		}

		dest+="}\n";
	}
}

int main(int argc, char* argv[])
{
	SFXCode Code;
	int pos;
	std::string s;
	char p[1024]="";
	
	_getcwd(p,sizeof(p));
	g_sDir=p;
	g_sDir+='\\';	

	s=argv[0];
	if ((pos=(int)s.rfind('\\'))!=-1)
	{
		s.resize(pos+1);

		if (s.length()>=2 && s[1]==':')
			g_sDir=s;
		else
			g_sDir=SCodeDependence::MakePathFileName(g_sDir,s.c_str());
	}
	else
		_strlwr_s((char *)g_sDir.c_str(),g_sDir.length()+1);
		

	for (int n=1;n<argc;++n)
	{
		char *arg=argv[n];

		if (arg[0]=='/')
			ProcessArg(arg);
		else
			g_asFiles.push_back(std::make_tuple(arg,"",false));
	}

	std::string sOutput;
	std::string sSrc,sOut;
	bool bBIN;
	
	for (size_t n=0;n<g_asFiles.size();++n)
	{
		std::tie(sSrc,sOut,bBIN)=g_asFiles[n];
		
		printf("Compiling: %s..\n\n",sSrc.c_str());
		sOutput="";
		bool bRes=FXCompile(sSrc.c_str(),g_sIncludeDir.c_str(),g_uFlags,g_asDefinitions.size()?&g_asDefinitions[0]:0,int(g_asDefinitions.size()),
							Code,sOutput,0,g_sLogFileName.c_str());

		if (sOutput.length())
			printf("%s\n==========\n",sOutput.c_str());
		
		if (bRes)
		{
			if (sOut.length())	//Recover Upper case in file name
			{
				std::string sName=sOut;
				sOut=SCodeDependence::MakePathFileName(g_sDir,sOut.c_str());

				if ((pos=(int)sName.rfind('\\'))!=-1)
					sName=sName.substr(pos+1);

				_ASSERTE(sOut.length()>=sName.length());
				if (sOut.length()>=sName.length())
					strncpy_s((char *)sOut.c_str()+(sOut.length()-sName.length()),sName.length()+1,
							sName.c_str(),sName.length());
			}

			if (!bBIN)
			{
				Disassemble(Code,sOutput);

				if (sOut.length())
				{
					FILE *f=0;
					fopen_s(&f,sOut.c_str(),"wb");

					if (f)
					{
						fprintf(f,"%s",sOutput.c_str());
						fclose(f);
					}
				}
				else
					printf("%s\n",sOutput.c_str());
			}
			else
			{
				Code.Save(sOut.c_str());
				Code.Clear();
				/*
				std::ifstream f(sOut.c_str(),std::ios_base::in | std::ios_base::binary);
				Code.Load(f,sOut.c_str(),false);
				Code.Clear();*/
			}
			/*
			{
			std::ifstream f(sOut.c_str(),std::ifstream::in | std::ifstream::binary);
			if (!f.eof())
			{
				//sOut=sOut.substr(sOut.rfind('\\')+1);
				Code.Load(f,sOut.c_str(),false);
				Code.bCompiled;
			}
			}

			{
			std::ifstream f(sOut.c_str(),std::ifstream::in | std::ifstream::binary);
			if (!f.eof())
			{
				//sOut=sOut.substr(sOut.rfind('\\')+1);
				Code.Load(f,sOut.c_str(),false);
				Code.bCompiled;
			}
			}
			*/
			if (g_bShowText && Code.bCompiled)
			{
				std::string s;
				Code.FormatSource(s,true);
				printf("Source text:\n%s\n\n",s.c_str());
			}

			printf("compilation succeeded;\n");
		}
	}
	/*
	std::string sErr;
	FXCompile("bkgr.fx","",D3DCOMPILE_DEBUG | D3DCOMPILE_OPTIMIZATION_LEVEL3,Code,sErr);

	if (Code.bCompiled)
	{
		printf("%s\n",Code.sOutput.c_str());
		//Code.Save("a.bin");
	}
	else
	{
		printf("====Errors====\n%s\n==========\n",sErr.c_str());
	}
	*/
}
