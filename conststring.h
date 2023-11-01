#pragma once

#include <string>
#include <functional>

template <class TChar>
using TBasicConstString=std::basic_string<TChar, std::char_traits<TChar>, std::allocator<TChar>>;

template <class TChar>
class constbasic_string	//const string with hash. Protects against string const_cast<..>
{
	typedef TBasicConstString<TChar> TSelfStr;
	typedef constbasic_string<TChar> TSelf;

	TSelfStr m_sStr;
	size_t m_uHash;


public:
	constbasic_string(const TSelfStr &rsSrc)
	{
		m_sStr=rsSrc;
		m_uHash=std::hash<TSelfStr>()(rsSrc);
	}
	constbasic_string(const TChar *sSrc)
	{
		if (sSrc)
			m_sStr=sSrc;

		m_uHash=std::hash<TSelfStr>()(m_sStr);
	}

	constbasic_string(const TSelf &rSrc)
	{
		m_sStr=rSrc.m_sStr;
		m_uHash=rSrc.m_uHash;
	}

	constbasic_string()
	{
		m_uHash=std::hash<TSelfStr>()(m_sStr);
	}

	const TChar &operator[](int n)const
	{
		return m_sStr[n];
	}

	const TSelfStr &operator =(const TSelfStr &rsSrc)
	{
		m_sStr=rsSrc;
		m_uHash=std::hash<TSelfStr>()(m_sStr);

		return rsSrc;
	}


	TSelf &operator =(const TSelf &rSrc)
	{
		m_sStr=rSrc.m_sStr;
		m_uHash=rSrc.m_uHash;

		return *this;
	}

	TSelf &operator =(const TChar *s)
	{
		m_sStr=s;
		m_uHash=std::hash<TSelfStr>()(m_sStr);

		return *this;
	}

	size_t hash()const
	{
		return m_uHash;
	}

	operator const TSelfStr &()const
	{
		return m_sStr;
	}
	/*
	operator const TChar *()const
	{
		return m_sStr.c_str();
	}
	*/
	const TSelfStr *operator ->()const
	{
		return &m_sStr;
	}



	bool operator >(const TSelf &rSrc)const
	{
		return m_sStr>rSrc.m_sStr;
	}
	bool operator <(const TSelf &rSrc)const
	{
		return m_sStr<rSrc.m_sStr;
	}
	bool operator ==(const TSelf &rSrc)const
	{
		if (rSrc.m_uHash!=m_uHash)
			return false;
		return rSrc.m_sStr==m_sStr;
	}
	bool operator !=(const TSelf &rSrc)const
	{
		if (rSrc.m_uHash!=m_uHash)
			return true;
		return rSrc.m_sStr!=m_sStr;
	}
	bool operator ==(const TSelfStr &rSrc)const
	{
		return rSrc==m_sStr;
	}
};

template<class TChar>
bool operator ==(const TBasicConstString<TChar> &rSrcL,const constbasic_string<TChar> &rSrcR)
{
	return (const std::string &)rSrcR==rSrcL;
}


template <class TChar>
class std::hash<constbasic_string<TChar>>
{
public:
	size_t operator()(const constbasic_string<TChar> &rSrc)const
	{
		return rSrc.hash();
	}
};

typedef constbasic_string<char> conststring;
