#include "dtex/TexPacker.h"

#include <texpack.h>

namespace dtex3
{

TexPacker::TexPacker(size_t width, size_t height, size_t capacity)
{
	m_tp = texpack_create(width, height, capacity);
}

TexPacker::~TexPacker()
{
	if (m_tp) {
		texpack_release(m_tp);
	}
}

bool TexPacker::Add(size_t width, size_t height, bool rotate, Rect& ret)
{
	if (m_tp) {
		auto pos = texpack_add(m_tp, width, height, rotate);
		if (pos) {
			ret.xmin = pos->r.xmin;
			ret.ymin = pos->r.ymin;
			ret.xmax = pos->r.xmax;
			ret.ymax = pos->r.ymax;
			return true;
		}
	}

	return false;
}

void TexPacker::Clear()
{
	if (m_tp) {
		texpack_clear(m_tp);
	}
}

}