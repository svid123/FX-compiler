#pragma once

#include <string>
#include <memory>
#include <vector>

#include "ComValue.h"


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

class CVectorType:	public CBaseType
{
	CV_TYPE m_Type;
	char m_nDimsX,m_nDimsY;

public:
	CVectorType(const char *sname,int T,char dimsx,char dimsy=0):m_Type((CV_TYPE)T),m_nDimsX(dimsx),m_nDimsY(dimsy),
		CBaseType(sname)
	{
	}

	CVectorType():m_Type(CV_NULL),m_nDimsX(0),m_nDimsY(0)
	{
	}

	CV_TYPE GetType()const{return m_Type;}
	char GetDimsX()const{return m_nDimsX;}
	char GetDimsY()const{return m_nDimsY;}

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

	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const override
	{
		if (puTypeQualifiers)
			*puTypeQualifiers=0;

		if (panRetDimSize)
		{
			panRetDimSize->clear();
			panRetDimSize->push_back(1);
		}
		return this;
	}
};


class CTypedef:	public CBaseType
{
	typedef std::vector<int> TADimSize;

	TADimSize m_anDimSize;
	unsigned int m_uTypeQualifiers;
public:
	CTypedef(const char *sname,PBaseType pBase,unsigned int uTypeQ=0):CBaseType(sname,pBase),
				m_uTypeQualifiers(uTypeQ)
	{
	}

	void AddDimSize(int n)
	{
		m_anDimSize.push_back(n);
	}

	const TADimSize &GetDims() const
	{
		return m_anDimSize;
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
				return m_anDimSize==pT->m_anDimSize;
		}

		return false;
	}

	virtual const CBaseType *Unroll(std::vector<int> *panRetDimSize=0,unsigned int *puTypeQualifiers=0) const override
	{
		const CBaseType *pRet=0,*pT=this;

		if (puTypeQualifiers)
			*puTypeQualifiers=0;

		if (panRetDimSize)
			panRetDimSize->clear();
		
		while (pT)
		{
			const CTypedef *pTD=dynamic_cast<const CTypedef *>(pT);
			if (pTD)
			{
				if (puTypeQualifiers)
					*puTypeQualifiers|=pTD->m_uTypeQualifiers;

				if (panRetDimSize)
				{
					for (int nSZ:	pTD->m_anDimSize)
						panRetDimSize->push_back(nSZ);
				}
			}

			pRet=pT;
			pT=pT->GetBase().get();
		}

		return pRet;
	}
};