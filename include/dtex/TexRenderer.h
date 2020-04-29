#pragma once

#include "dtex/Utility.h"

#include <unirender2/typedef.h>

#include <vector>

namespace ur2 {
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
    TexRenderer(const ur2::Device& dev);

	void Draw(ur2::Context& ctx, const ur2::TexturePtr& src, const Rect& src_r,
		const ur2::TexturePtr& dst, const Rect& dst_r, bool rotate);
    void Flush(ur2::Context& ctx);

	void ClearTex(ur2::Context& ctx, const ur2::TexturePtr& tex,
        float xmin, float ymin, float xmax, float ymax) const;
	void ClearAllTex(ur2::Context& ctx, const ur2::TexturePtr& tex) const;

private:
    void InitVertexArray(const ur2::Device& dev);

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
    std::shared_ptr<ur2::ShaderProgram> m_shader = nullptr;

    //Rect m_viewport;
    std::shared_ptr<ur2::Framebuffer> m_rt = nullptr;

    ur2::TexturePtr m_dst_texture = nullptr;
    ur2::TexturePtr m_src_texture = nullptr;
    VertBuffer m_vert_buf;

    std::shared_ptr<ur2::VertexArray> m_va = nullptr;

}; // TexRenderer

}