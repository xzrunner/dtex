#include "dtex/TexPacker.h"

#include <texpack.h>

namespace dtex
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

Quad TexPacker::Add(size_t width, size_t height, bool rotate)
{
    Quad ret;

    if (!m_tp) {
        return ret;
    }

	auto pos = texpack_add(m_tp, width, height, rotate);
    if (!pos) {
        return ret;
    }

    ret.rot = pos->is_rotated;

    ret.rect.xmin = pos->r.xmin;
    ret.rect.ymin = pos->r.ymin;
    ret.rect.xmax = pos->r.xmax;
    ret.rect.ymax = pos->r.ymax;

	return ret;
}

void TexPacker::Clear()
{
	if (m_tp) {
		texpack_clear(m_tp);
	}
}

}