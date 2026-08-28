#pragma once

#include "dtex/Utility.h"

#include <unirender/typedef.h>

#include <cstdint>
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

	bool Draw(ur::Context& ctx, const ur::TexturePtr& src, const Rect& src_r,
		const ur::TexturePtr& dst, const Rect& dst_r, bool rotate);
    bool Flush(ur::Context& ctx);
    bool HasPending() const;
    void DiscardPending();
#ifdef DTEX_ENABLE_TEST_SEAMS
    void FailNextFlush(int count);
    bool ConsumeFailNextFlush();
    void FailNextGpuFlushForTest(int count);
#endif

	bool ClearTex(ur::Context& ctx, const ur::TexturePtr& tex,
        float xmin, float ymin, float xmax, float ymax) const;
	bool ClearAllTex(ur::Context& ctx, const ur::TexturePtr& tex) const;

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
        bool AddQuad(const float* positions, const float* texcoords);

        void Clear();

        bool IsEmpty() const { return indices.empty(); }

        std::vector<Vertex>         vertices;
        std::vector<std::uint32_t>  indices;
    };

private:
    std::shared_ptr<ur::ShaderProgram> m_shader = nullptr;

    //Rect m_viewport;
    std::shared_ptr<ur::Framebuffer> m_rt = nullptr;

    ur::TexturePtr m_dst_texture = nullptr;
    ur::TexturePtr m_src_texture = nullptr;
    VertBuffer m_vert_buf;

    std::shared_ptr<ur::VertexArray> m_va = nullptr;
#ifdef DTEX_ENABLE_TEST_SEAMS
    int m_fail_next_flush = 0;
    int m_fail_next_gpu_flush = 0;
#endif

}; // TexRenderer

}
