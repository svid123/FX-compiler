#include "stdafx.h"

#include "ComValue.h"
#include "Expressions.h"
#include <minmax.h>
#include <windows.h>



SComValue::operator bool()const
{
	return nData!=0;
}
SComValue::operator char()const
{
	unsigned char T=uType & CV_TYPE_MASK;

	if (T==CV_FLOAT)
		return (char)(*(float *)&nData);
	else
	if (T==CV_DOUBLE)
		return (char)(*(double *)&nData);
	else
		return (char)nData;
}
SComValue::operator short()const
{
	unsigned char T=uType & CV_TYPE_MASK;

	if (T==CV_FLOAT)
		return (short)(*(float *)&nData);
	else
	if (T==CV_DOUBLE)
		return short(*(double *)&nData);
	else
		return (short)nData;
}
SComValue::operator int()const
{
	unsigned char T=uType & CV_TYPE_MASK;
	
	if (T==CV_FLOAT)
		return (int)(*(float *)&nData);
	else
	if (T==CV_DOUBLE)
		return int(*(double *)&nData);
	else
		return (int)nData;
}

SComValue::operator double()const
{
	unsigned char T=uType & CV_TYPE_MASK;
	
	if (T==CV_FLOAT)
		return (double)(*(float *)&nData);
	else
	if (T==CV_DOUBLE)
		return *(double *)&nData;
	else
		return (double)nData;
}

SComValue::operator float()const
{
	unsigned char T=uType & CV_TYPE_MASK;
	
	if (T==CV_FLOAT)
		return *(float *)&nData;
	else
	if (T==CV_DOUBLE)
		return (float)*(double *)&nData;
	else
		return (float)nData;
}



void SComValue::cast(CV_TYPE TT)
{
	unsigned char uT=uType & (CV_TYPE_MASK | CV_UNSIGNED);

	if (uT==TT)
		return;
	
	if (TT & CV_UNSIGNED)
	switch (TT & CV_TYPE_MASK)
	{
		case CV_CHAR:nData=(unsigned char)*this;
					break;
		case CV_SHORT:nData=(unsigned short)*this;
					break;
		case CV_INT:nData=(unsigned int)*this;
					break;
		case CV_DOUBLE:{
						double f=*this;
						(*this)=f;
						}break;
		case CV_FLOAT:{
						float f=*this;
						(*this)=f;
						}break;

		default:nData=0;_ASSERTE(false);
	}
	else
	switch (TT)
	{
		case CV_CHAR:nData=(char)*this;
					break;
		case CV_SHORT:nData=(short)*this;
					break;
		case CV_INT:nData=(int)*this;
					break;
		case CV_DOUBLE:{
						double f=*this;
						(*this)=f;
						}break;
		case CV_FLOAT:{
						float f=*this;
						(*this)=f;
						}break;

		default:nData=0;_ASSERTE(false);
	}
	uType=TT;
}

const char *SComValue::typeName()const
{
	const char *sRet="";

	unsigned char uT=uType & CV_TYPE_MASK;
	switch (uT)
	{
		case CV_NULL:sRet="NULL";
			break;
		case CV_CHAR:sRet=(uType & CV_UNSIGNED)?"unsigned char":"char";
			break;
		case CV_SHORT:sRet=(uType & CV_UNSIGNED)?"unsigned short":"short";
			break;
		case CV_INT:sRet=(uType & CV_UNSIGNED)?"unsigned int":"int";
			break;
		case CV_FLOAT:sRet="float";
			break;
		case CV_DOUBLE:sRet="double";
			break;
	}

	return sRet;
}

