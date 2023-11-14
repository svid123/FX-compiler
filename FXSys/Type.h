#pragma once

#include <string>
#include <memory>
#include <vector>

#include "ComValue.h"

namespace fx
{

class CBaseType;

typedef std::shared_ptr<CBaseType> PBaseType;
enum TYPE_QUALIFIERS_BIT
{
	TQB_STATIC=0,
	TQB_CONST,
	TQB_VOLATILE,
	TQB_UNIFORM,
	TQB_EXTERN,

	TQB_SIZE
};

class CBaseType
{
protected:
	std::string m_sName;
	PBaseType m_pBase;

public:
	CBaseType(const char *sName,PBaseType pBase=PBaseType()):m_sName(sName),m_pBase(pBase)
	{
	}

	CBaseType()
	{
	}

	virtual ~CBaseType()
	{
	}

	PBaseType GetBase() const
	{
		return m_pBase;
	}

	const std::string &GetName()const{return m_sName;}

	virtual bool IsSame(const CBaseType *pSrc) const
	{
		if ((m_pBase.get()==0)!=(pSrc->m_pBase.get()==0))
			return false;

		if (m_pBase && pSrc->m_pBase)
			return m_pBase->IsSame(pSrc->m_pBase.get());

		return true;
	}

	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const=0;
};

class CTypedef:	public CBaseType
{	
	int m_nDimSize;
	unsigned int m_uTypeQualifiers;
public:
	CTypedef(const char *sname,int nDimSize,PBaseType pBase,unsigned int uTypeQ=0):CBaseType(sname,pBase),
				m_uTypeQualifiers(uTypeQ),m_nDimSize(nDimSize)
	{
	}

	int GetDims() const
	{
		return m_nDimSize;
	}

	unsigned int GetTypeQualifiers()
	{
		return m_uTypeQualifiers;
	}

	virtual bool IsSame(const CBaseType *pSrc) const override
	{
		if (__super::IsSame(pSrc))
		{
			const CTypedef *pT=dynamic_cast<const CTypedef *>(pSrc);

			if (pT)
				return m_nDimSize==pT->m_nDimSize;
		}

		return false;
	}

	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const override
	{
		if (puTypeQualifiers)
			*puTypeQualifiers|=m_uTypeQualifiers;

		if (panRetDimSize && m_nDimSize)
			panRetDimSize->push_back(m_nDimSize);
		
		return m_pBase->Unroll(panRetDimSize,puTypeQualifiers);
	}
};

class CPrimitiveType:	public CBaseType
{
	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const override
	{
		return this;
	}
public:
	CPrimitiveType(const char *name):CBaseType(name)
	{
	}
};

class CVectorType:	public CTypedef
{
	CV_TYPE m_Type;
	char m_nDimsX,m_nDimsY;

public:
	CVectorType(const char *sname,int T,char dimsx,char dimsy=0):m_Type((CV_TYPE)T),m_nDimsX(dimsx),m_nDimsY(dimsy),
		CTypedef(sname,(dimsy?dimsy:dimsx),
				(dimsy?PBaseType(new CTypedef("",dimsx,std::make_shared<CPrimitiveType>(""))):PBaseType(new CPrimitiveType(""))))
	{
	}

	CVectorType():m_Type(CV_NULL),m_nDimsX(0),m_nDimsY(0),CTypedef("",0,0)
	{
	}

	CV_TYPE GetType()const{return m_Type;}
	char GetDimsX()const{return m_nDimsX;}
	char GetDimsY()const{return m_nDimsY;}
	/*
	virtual bool IsSame(const CBaseType *pSrc) const override
	{
		if (__super::IsSame(pSrc))
		{
			const CVectorType *pVT=dynamic_cast<const CVectorType *>(pSrc);

			if (pVT)
				return m_Type==pVT->m_Type && m_nDimsX==pVT->m_nDimsX && m_nDimsY==pVT->m_nDimsY;
		}

		return false;
	}
	
	*/
	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const override
	{
		return this;
	}	
};


}