#pragma once

#include "dtex/Utility.h"

#include <unirender/typedef.h>

#include <vector>

namespace ur {
    class Device;
    class Context;
    class Framebuffer;
    class ShaderProgram;
    class VertexArray;
}

namespace dtex
{

class TexRenderer
{
public:
    TexRenderer(const ur::Device& dev);

	void Draw(ur::Context& ctx, const ur::TexturePtr& src, const Rect& src_r,
		const ur::TexturePtr& dst, const Rect& dst_r, bool rotate);
    void Flush(ur::Context& ctx);

	void ClearTex(ur::Context& ctx, const ur::TexturePtr& tex,
        float xmin, float ymin, float xmax, float ymax) const;
	void ClearAllTex(ur::Context& ctx, const ur::TexturePtr& tex) const;

private:
    void InitVertexArray(const ur::Device& dev);

private:
    struct Vertex
    {
        float pos[2];
        float uv[2];
    };

    struct VertBuffer
    {
        void AddQuad(const float* positions, const float* texcoords);

        void Reserve(size_t idx_count, size_t vtx_count);

        void Clear();

        bool IsEmpty() const { return indices.empty(); }

        std::vector<Vertex>         vertices;
        std::vector<unsigned short> indices;

        unsigned short  curr_index = 0;
        Vertex*         vert_ptr   = nullptr;
        unsigned short* index_ptr  = nullptr;
    };

private:
    std::shared_ptr<ur::ShaderProgram> m_shader = nullptr;

    //Rect m_viewport;
    std::shared_ptr<ur::Framebuffer> m_rt = nullptr;

    ur::TexturePtr m_dst_texture = nullptr;
    ur::TexturePtr m_src_texture = nullptr;
    VertBuffer m_vert_buf;

    std::shared_ptr<ur::VertexArray> m_va = nullptr;

}; // TexRenderer

}