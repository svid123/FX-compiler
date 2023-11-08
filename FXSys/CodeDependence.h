#pragma once

#include <vector>
#include <set>
#include <memory>
#include <unordered_map>
#include <map>

#include <iostream>

#include "Serializer.h"

namespace fx
{

struct SCodeDependence:	public SSerializable
{
	std::string sFileName;	//File names are relative to base source file (if in same path tree)
	unsigned long uChangeDHMS;	//File change: Day, Hour, Min, Sec
	bool bRelPath;

	SCodeDependence():uChangeDHMS(0),bRelPath(false)
	{
	}

	SCodeDependence(const std::string &sFN):sFileName(sFN),uChangeDHMS(0),bRelPath(false)
	{
	}

	SCodeDependence(const char *sFN):sFileName(sFN),uChangeDHMS(0),bRelPath(false)
	{
	}

	//bool UpdateFileChange(const std::string &sBaseDir);

	bool operator ==(const std::string &s)
	{
		return sFileName==s;
	}
	bool operator ==(const SCodeDependence &src)
	{
		return src.sFileName==sFileName && src.uChangeDHMS==uChangeDHMS;
	}

	virtual bool Serialize(SSerializerIOContext &io);

	static std::string ConvertFileNameRelative(const std::string &sBaseFN,const std::string &sFN);
	static std::string MakePathFileName(const std::string &sDir,const char *sFN);
	static void CorrectFileName(std::string &s,bool bLowercase);
};

typedef std::vector<SCodeDependence> TADependences;

}