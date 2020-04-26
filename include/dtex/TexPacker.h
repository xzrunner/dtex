#pragma once

#include "dtex/Utility.h"

#include <boost/noncopyable.hpp>

struct texpack;

namespace dtex3
{

class TexPacker : boost::noncopyable
{
public:
	TexPacker(size_t width, size_t height, size_t capacity);
	~TexPacker();

	bool Add(size_t width, size_t height, bool rotate, Rect& ret);

	void Clear();

private:
	texpack* m_tp = nullptr;

}; // TexPacker

}