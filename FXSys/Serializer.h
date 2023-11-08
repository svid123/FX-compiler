#pragma once

#include <stdio.h>
#include <fstream>
#include <vector>
#include <map>
#include <unordered_map>
#include <crtdbg.h>



struct SSerializable
{
	struct SSerializerIOContext
	{
		FILE *fOut;
		std::istream *pIn;
		int nRNDPos;
		bool bErrors;

		SSerializerIOContext(FILE *f,int nrnd=0):fOut(f),pIn(0),nRNDPos(nrnd),bErrors(false)
		{
		}
		SSerializerIOContext(std::istream &in,int nrnd=0):fOut(0),pIn(&in),nRNDPos(nrnd),bErrors(false)
		{
		}

		void SaveString(const std::string &s);
		std::string LoadString();

		bool WriteMode()
		{
			return fOut!=0;
		}

		template<class T>
		SSerializerIOContext &operator <<(T &v)
		{
			if (std::is_convertible<decltype(&v),SSerializable *>())
				bErrors|=!((SSerializable &)v).Serialize(*this);
			else
				bErrors|=!SSerializable::StreamValue_(*this,v);

			return *this;
		}
	};










	template <class TCont>
	static bool StreamValue_(SSerializerIOContext &io,TCont &v)
	{
		if (io.fOut)
		{
			fwrite(&v,sizeof(v),1,io.fOut);

			return true;
		}
		else
		{
			if (!io.pIn->fail())
				io.pIn->read((char *)&v,sizeof(v));

			return !io.pIn->fail();
		}
	}


	template <class TCont>
	static bool StreamValue_(SSerializerIOContext &io,const TCont &v)
	{
		if (io.fOut)
		{
			fwrite(&v,sizeof(v),1,io.fOut);

			return true;
		}
		else
		{
			_ASSERTE(!"Can not read into 'const' field");
			return false;
		}
	}

	template <>
	static bool StreamValue_<std::string>(SSerializerIOContext &io,std::string &v)
	{
		if (io.fOut)
			io.SaveString(v);
		else
			v=io.LoadString();
		return true;
	};

	template <>
	static bool StreamValue_<std::string>(SSerializerIOContext &io,const std::string &v)
	{
		if (io.fOut)
		{
			io.SaveString(v);
			return true;
		}
		else
		{
			_ASSERTE(!"Can not read into 'const' field");
			return false;
		}
	};

	template <class T1>
	static bool StreamValue_(SSerializerIOContext &io,std::unique_ptr<T1> &v)
	{
		if (io.WriteMode())
			_ASSERTE(v.get());
		else
		{
			_ASSERTE(!v.get());
			v=std::make_unique<T1>();
		}

		io<<*v;
		return true;
	};
	

	template <class T1,class T2>
	static bool StreamValue_(SSerializerIOContext &io,std::pair<T1,T2> &v)
	{		
		io<<v.first;
		io<<v.second;

		return true;
	}

	template <class T1>
	static bool StreamValue_(SSerializerIOContext &io,std::vector<T1> &v)
	{
		unsigned int sz=(unsigned int)v.size();
		io<<sz;

		if (!io.WriteMode())
			v.resize(sz);

		for (auto &val:	v)
			io<<val;

		return true;
	}

	
	template <class T1,class T2>
	static bool StreamValue_(SSerializerIOContext &io,std::map<T1,T2> &v)
	{
		unsigned int sz=(unsigned int)v.size();
		
		io<<sz;

		if (io.WriteMode())
		for (auto &pair:	v)
		{
			io<<pair.first;
			io<<pair.second;
		}
		else
		while (sz--)
		{
			std::pair<T1,T2> pair;
			io<<pair.first;
			io<<pair.second;
			v.insert(pair);
		}

		return true;
	}

	template <class T1,class T2>
	static bool StreamValue_(SSerializerIOContext &io,std::unordered_map<T1,T2> &v)
	{
		unsigned int sz=(unsigned int)v.size();
		
		io<<sz;

		if (io.WriteMode())
		for (auto &pair:	v)
		{
			io<<pair.first;
			io<<pair.second;
		}
		else
		while (sz--)
		{
			std::pair<T1,T2> pair;
			io<<pair.first;
			io<<pair.second;
			v.insert(std::move(pair));
		}

		return true;
	}

	
	virtual bool Serialize(SSerializerIOContext &io)=0;
};

