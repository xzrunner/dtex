#pragma once

#include "dtex/Utility.h"

#include <unirender/typedef.h>

namespace dtex
{

class TexBufNode
{
public:
    TexBufNode(uint64_t key, const ur::TexturePtr& dst_tex, const Quad& dst_pos);

    const float* GetTexcoords() const { return m_texcoords; }

    uint64_t Key() const { return m_key; }

private:
    uint64_t m_key;

    ur::TexturePtr m_dst_tex;

    float m_texcoords[8];

}; // TexBufNode

}