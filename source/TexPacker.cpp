#include "dtex/TexPacker.h"

#include <texpack.h>

#include <limits>

namespace dtex
{

TexPacker::TexPacker(size_t width, size_t height, size_t capacity)
{
	const size_t max_int = static_cast<size_t>(std::numeric_limits<int>::max());
	if (width == 0 || height == 0 || capacity == 0 ||
		width > max_int || height > max_int || capacity > max_int) {
		return;
	}
	m_tp = texpack_create(static_cast<int>(width), static_cast<int>(height),
		static_cast<int>(capacity));
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

	const size_t max_int = static_cast<size_t>(std::numeric_limits<int>::max());
	if (!m_tp || width == 0 || height == 0 || width > max_int || height > max_int) {
        return ret;
    }

	auto pos = texpack_add(m_tp, static_cast<int>(width),
		static_cast<int>(height), rotate);
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
