#pragma once


//#include "QuickVector.h"
#include "conststring.h"
#include <crtdbg.h>
#include <memory>


struct SComValue;
//typedef std::shared_ptr<CCBaseType> PCBaseType;


enum CV_TYPE:unsigned char
{
	CV_NULL=0,
	CV_CHAR,
	CV_SHORT,
	CV_INT,
	CV_FLOAT,
	CV_DOUBLE,
	CV_REF,
	
	CV_SIZE,
	CV_TYPE_MASK=15,

	CV_UNSIGNED=32
};

class CExpReference;
struct SExeContext;





//Common value
struct SComValue
{
public:
	long long nData;
	unsigned char uType;

	SComValue():nData(0),uType(CV_NULL)
	{
	}
	SComValue(const int &n):nData(n),uType(CV_INT)
	{
	}
	SComValue(const float &f):nData(*(unsigned long *)&f),uType(CV_FLOAT)
	{
	}
	SComValue(const double &f):nData(*(unsigned long long *)&f),uType(CV_DOUBLE)
	{
	}

	const SComValue &operator =(unsigned long n)
	{		
		nData=n;
		uType=CV_INT | CV_UNSIGNED;
		return *this;
	}
	const SComValue &operator =(long n)
	{
		nData=n;
		uType=CV_INT;
		return *this;
	}
	const SComValue &operator =(unsigned int n)
	{		
		nData=n;
		uType=CV_INT | CV_UNSIGNED;
		return *this;
	}
	const SComValue &operator =(int n)
	{
		nData=n;
		uType=CV_INT;
		return *this;
	}

	const SComValue &operator =(unsigned char n)
	{		
		nData=n;
		uType=CV_CHAR | CV_UNSIGNED;
		return *this;
	}
	const SComValue &operator =(char n)
	{
		nData=n;
		uType=CV_CHAR;
		return *this;
	}
	const SComValue &operator =(bool b)
	{
		nData=b?1:0;
		uType=CV_CHAR | CV_UNSIGNED;
		return *this;
	}

	const SComValue &operator =(unsigned short n)
	{		
		nData=n;
		uType=CV_SHORT | CV_UNSIGNED;
		return *this;
	}
	const SComValue &operator =(short n)
	{
		nData=n;
		uType=CV_SHORT;
		return *this;
	}

	const SComValue &operator =(double f)
	{
		nData=*(unsigned long long *)&f;
		uType=CV_DOUBLE;
		return *this;
	}

	const SComValue &operator =(float f)
	{
		nData=*(unsigned long *)&f;
		uType=CV_FLOAT;
		return *this;
	}

	operator char()const;
	operator short()const;
	operator int()const;
	operator double()const;
	operator float()const;
	explicit operator bool()const;

	operator unsigned char()const
	{
		return (char)*this;
	}
	operator unsigned int()const
	{
		return (int)*this;
	}
	operator unsigned long()const
	{
		return (int)*this;
	}
	operator unsigned short()const
	{
		return (short)*this;
	}


	void cast(CV_TYPE T);
	
	const char *typeName()const;
	
	unsigned char type()const//Type without flags
	{
		return uType & CV_TYPE_MASK;
	}
};


//typedef CQuickVector<SComValue> TAComValues;