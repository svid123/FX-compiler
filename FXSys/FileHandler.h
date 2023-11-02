#pragma once

#include <map>
#include <unordered_set>
#include <unordered_map>
#include <sstream>

class CFileHandler
{
public:

	virtual std::istream *OpenFileBIN(const char *sFN);
	virtual void CloseFileBIN(std::istream *pFile);
	virtual unsigned long GetChangeDHMS(const char *sFileName);
};

