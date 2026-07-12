#pragma once
#ifndef CATA_SRC_RENDER_3D_GPU_H
#define CATA_SRC_RENDER_3D_GPU_H

#if defined(TILES)

#include "sdl_wrappers.h"

// The depth-buffered SDL_GPU scene pass needs SDL3's GPU API and the gpu
// render driver; there is no SDL2 counterpart.
#if SDL_MAJOR_VERSION >= 3

#include <cstdint>
#include <map>
#include <vector>

#include "render_3d.h"

/**
 * Depth-buffered SDL_GPU pass for the block_3d world renderer
 * (doc/3D_ROADMAP.md Phase 4): renders the pre-packed scene vertex stream
 * through a real graphics pipeline — GPU depth testing instead of
 * painter's sorting — with a sun shadow-map pass in front, into a
 * renderer texture the 2D pipeline then composites like any other.
 *
 * Requires the SDL3 "gpu" render driver (the SDL_GPUTexture behind a
 * renderer texture is reachable only there). On any other driver, or on
 * any GPU failure, render() returns null and the caller keeps the raster
 * path; failures disable the pass until the renderer's GPU device changes
 * (renderer recreation), mirroring cata_shader's session discipline.
 *
 * Object lifetimes: everything is owned by the renderer's GPU device.
 * When the device pointer changes the old handles are already destroyed
 * by SDL, so they are abandoned rather than released. Never destroyed at
 * process exit either — the device dies first.
 */
class scene_gpu_pass
{
    public:
        scene_gpu_pass() = default;
        ~scene_gpu_pass() = default;
        scene_gpu_pass( const scene_gpu_pass & ) = delete;
        scene_gpu_pass &operator=( const scene_gpu_pass & ) = delete;

        /** A run of main-pass vertices drawn with one atlas texture (null = flat color). */
        struct run {
            SDL_Texture *tex = nullptr;
            int begin = 0;
            int count = 0;
        };

        /**
         * Cheap per-frame gate: true when the renderer runs the gpu driver
         * and the pass is not disabled on its current device. A true here
         * does not guarantee render() succeeds — it can still fail and
         * fall back — but a false means don't bother building the streams.
         */
        bool likely_available( const SDL_Renderer_Ptr &renderer ) const {
            if( !renderer ) {
                return false;
            }
            SDL_GPUDevice *const device = SDL_GetGPURendererDevice( renderer.get() );
            return device && ( device != device_ || !disabled_ );
        }

        /**
         * Render one frame: shadow pass over shadow_verts (light-space
         * x, y, depth triples; skipped and far-cleared when
         * shadow_pass_wanted is false), then the depth-tested main pass
         * over main_verts in run order. Returns the scene texture to
         * composite, sized (view_w, view_h), or null when the GPU lane is
         * unavailable — the caller must then draw the CPU path instead.
         */
        SDL_Texture *render( const SDL_Renderer_Ptr &renderer, int view_w, int view_h,
                             const std::vector<render_3d::gpu_vtx> &main_verts,
                             const std::vector<run> &runs,
                             const std::vector<float> &shadow_verts,
                             bool shadow_pass_wanted, bool ssao_wanted );

    private:
        bool ensure_device_objects();
        bool ensure_scene_target( const SDL_Renderer_Ptr &renderer, int w, int h );
        bool upload_vertices( SDL_GPUCommandBuffer *cmd, const void *data, uint32_t bytes,
                              SDL_GPUBuffer *&buf, uint32_t &capacity,
                              SDL_GPUTransferBuffer *&transfer, uint32_t &transfer_capacity );
        SDL_GPUTexture *unwrap( SDL_Texture *tex );
        /** Forget every handle without touching SDL (device died with the renderer). */
        void abandon_all();
        /** Release device objects on a still-live device, then disable. */
        void release_and_disable();

        SDL_GPUDevice *device_ = nullptr;
        bool disabled_ = false;

        SDL_GPUGraphicsPipeline *main_pipeline_ = nullptr;
        SDL_GPUGraphicsPipeline *shadow_pipeline_ = nullptr;
        SDL_GPUGraphicsPipeline *ssao_pipeline_ = nullptr;
        SDL_GPUSampler *atlas_sampler_ = nullptr;
        SDL_GPUSampler *shadow_sampler_ = nullptr;
        SDL_GPUTexture *shadow_map_ = nullptr;
        SDL_GPUTexture *shadow_depth_ = nullptr;
        SDL_GPUTexture *scene_depth_ = nullptr;
        SDL_GPUTexture *depth_linear_ = nullptr; // R32F view depth, AO source
        SDL_Texture *scene_tex_ = nullptr;
        SDL_GPUTexture *scene_gpu_ = nullptr; // unwrapped from scene_tex_, not owned
        SDL_GPUTexture *white_gpu_ = nullptr; // owned pure-GPU 1x1 white
        SDL_GPUBuffer *main_vbuf_ = nullptr;
        SDL_GPUBuffer *shadow_vbuf_ = nullptr;
        SDL_GPUTransferBuffer *main_xfer_ = nullptr;
        SDL_GPUTransferBuffer *shadow_xfer_ = nullptr;
        uint32_t main_vbuf_cap_ = 0;
        uint32_t shadow_vbuf_cap_ = 0;
        uint32_t main_xfer_cap_ = 0;
        uint32_t shadow_xfer_cap_ = 0;
        int scene_w_ = 0;
        int scene_h_ = 0;
        SDL_GPUTextureFormat depth_format_ = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
};

#endif // SDL_MAJOR_VERSION >= 3

#endif // TILES

#endif // CATA_SRC_RENDER_3D_GPU_H
