#pragma once

#include "dtex/Utility.h"

#include <unirender/noncopyable.h>

struct texpack;

namespace dtex
{

class TexPacker : ur::noncopyable
{
public:
	TexPacker(size_t width, size_t height, size_t capacity);
	~TexPacker();

    Quad Add(size_t width, size_t height, bool rotate);

	void Clear();

private:
	texpack* m_tp = nullptr;

}; // TexPacker

}