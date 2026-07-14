#include "render_3d_gpu.h"

#if defined(TILES) && SDL_MAJOR_VERSION >= 3

#include <cstring>

#include "cata_shader.h"
#include "debug.h"

static constexpr int SHADOW_MAP_SIZE = 1024;

void scene_gpu_pass::abandon_all()
{
    main_pipeline_ = nullptr;
    shadow_pipeline_ = nullptr;
    ssao_pipeline_ = nullptr;
    bloom_pipeline_ = nullptr;
    atlas_sampler_ = nullptr;
    shadow_sampler_ = nullptr;
    linear_sampler_ = nullptr;
    shadow_map_ = nullptr;
    shadow_depth_ = nullptr;
    scene_depth_ = nullptr;
    depth_linear_ = nullptr;
    emissive_ = nullptr;
    scene_tex_ = nullptr;
    scene_gpu_ = nullptr;
    white_gpu_ = nullptr;
    main_vbuf_ = nullptr;
    shadow_vbuf_ = nullptr;
    main_xfer_ = nullptr;
    shadow_xfer_ = nullptr;
    main_vbuf_cap_ = 0;
    shadow_vbuf_cap_ = 0;
    main_xfer_cap_ = 0;
    shadow_xfer_cap_ = 0;
    scene_w_ = 0;
    scene_h_ = 0;
}

void scene_gpu_pass::release_and_disable()
{
    if( device_ ) {
        if( main_pipeline_ ) {
            SDL_ReleaseGPUGraphicsPipeline( device_, main_pipeline_ );
        }
        if( shadow_pipeline_ ) {
            SDL_ReleaseGPUGraphicsPipeline( device_, shadow_pipeline_ );
        }
        if( ssao_pipeline_ ) {
            SDL_ReleaseGPUGraphicsPipeline( device_, ssao_pipeline_ );
        }
        if( bloom_pipeline_ ) {
            SDL_ReleaseGPUGraphicsPipeline( device_, bloom_pipeline_ );
        }
        if( atlas_sampler_ ) {
            SDL_ReleaseGPUSampler( device_, atlas_sampler_ );
        }
        if( shadow_sampler_ ) {
            SDL_ReleaseGPUSampler( device_, shadow_sampler_ );
        }
        if( linear_sampler_ ) {
            SDL_ReleaseGPUSampler( device_, linear_sampler_ );
        }
        if( shadow_map_ ) {
            SDL_ReleaseGPUTexture( device_, shadow_map_ );
        }
        if( shadow_depth_ ) {
            SDL_ReleaseGPUTexture( device_, shadow_depth_ );
        }
        if( scene_depth_ ) {
            SDL_ReleaseGPUTexture( device_, scene_depth_ );
        }
        if( depth_linear_ ) {
            SDL_ReleaseGPUTexture( device_, depth_linear_ );
        }
        if( emissive_ ) {
            SDL_ReleaseGPUTexture( device_, emissive_ );
        }
        if( main_vbuf_ ) {
            SDL_ReleaseGPUBuffer( device_, main_vbuf_ );
        }
        if( shadow_vbuf_ ) {
            SDL_ReleaseGPUBuffer( device_, shadow_vbuf_ );
        }
        if( main_xfer_ ) {
            SDL_ReleaseGPUTransferBuffer( device_, main_xfer_ );
        }
        if( shadow_xfer_ ) {
            SDL_ReleaseGPUTransferBuffer( device_, shadow_xfer_ );
        }
        if( white_gpu_ ) {
            SDL_ReleaseGPUTexture( device_, white_gpu_ );
        }
        if( scene_tex_ ) {
            SDL_DestroyTexture( scene_tex_ );
        }
    }
    abandon_all();
    disabled_ = true;
}

SDL_GPUTexture *scene_gpu_pass::unwrap( SDL_Texture *tex )
{
    if( !tex ) {
        return nullptr;
    }
    const SDL_PropertiesID props = SDL_GetTextureProperties( tex );
    return static_cast<SDL_GPUTexture *>(
               SDL_GetPointerProperty( props, SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER, nullptr ) );
}

bool scene_gpu_pass::ensure_device_objects()
{
    if( main_pipeline_ ) {
        return true;
    }
    depth_format_ =
        SDL_GPUTextureSupportsFormat( device_, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
                                      SDL_GPU_TEXTURETYPE_2D,
                                      SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET )
        ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_D16_UNORM;

    cata_shader::shader main_vs = cata_shader::shader::load_stage(
                                      device_, "scene_3d.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0 );
    cata_shader::shader main_fs = cata_shader::shader::load_stage(
                                      device_, "scene_3d.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0 );
    cata_shader::shader shadow_vs = cata_shader::shader::load_stage(
                                        device_, "scene_3d_shadow.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0 );
    cata_shader::shader shadow_fs = cata_shader::shader::load_stage(
                                        device_, "scene_3d_shadow.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0 );
    cata_shader::shader ssao_vs = cata_shader::shader::load_stage(
                                      device_, "scene_ssao.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0, 0 );
    cata_shader::shader ssao_fs = cata_shader::shader::load_stage(
                                      device_, "scene_ssao.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0 );
    // The bloom pass reuses the SSAO fullscreen-triangle vertex stage.
    cata_shader::shader bloom_fs = cata_shader::shader::load_stage(
                                       device_, "scene_bloom.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0 );
    if( !main_vs.is_valid() || !main_fs.is_valid() ||
        !shadow_vs.is_valid() || !shadow_fs.is_valid() ||
        !ssao_vs.is_valid() || !ssao_fs.is_valid() || !bloom_fs.is_valid() ) {
        DebugLog( D_ERROR, DC_ALL )
                << "scene_gpu_pass: shader artifacts unavailable; GPU scene pass disabled";
        return false;
    }

    // Main pipeline: the packed gpu_vtx layout, alpha blending matching
    // SDL_BLENDMODE_BLEND, depth test + write, plus a second color target
    // storing view depth for the screen-space AO pass.
    {
        SDL_GPUVertexBufferDescription vb{};
        vb.slot = 0;
        vb.pitch = sizeof( render_3d::gpu_vtx );
        vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attrs[6] = {};
        attrs[0].location = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attrs[0].offset = offsetof( render_3d::gpu_vtx, x );
        attrs[1].location = 1;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        attrs[1].offset = offsetof( render_3d::gpu_vtx, r );
        attrs[2].location = 2;
        attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[2].offset = offsetof( render_3d::gpu_vtx, u );
        attrs[3].location = 3;
        attrs[3].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[3].offset = offsetof( render_3d::gpu_vtx, lu );
        attrs[4].location = 4;
        attrs[4].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        attrs[4].offset = offsetof( render_3d::gpu_vtx, vd );
        attrs[5].location = 5;
        attrs[5].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        attrs[5].offset = offsetof( render_3d::gpu_vtx, emit );

        SDL_GPUColorTargetDescription colors[3] = {};
        // Target 0: the renderer's ARGB8888 texture is BGRA to the GPU.
        colors[0].format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        colors[0].blend_state.enable_blend = true;
        colors[0].blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        colors[0].blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        colors[0].blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        colors[0].blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        colors[0].blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        colors[0].blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        // Target 1: view depth, written opaque (no blend).
        colors[1].format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        // Target 2: emissive mask, written opaque.
        colors[2].format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = main_vs.get();
        info.fragment_shader = main_fs.get();
        info.vertex_input_state.vertex_buffer_descriptions = &vb;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = attrs;
        info.vertex_input_state.num_vertex_attributes = 6;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.target_info.color_target_descriptions = colors;
        info.target_info.num_color_targets = 3;
        info.target_info.has_depth_stencil_target = true;
        info.target_info.depth_stencil_format = depth_format_;
        main_pipeline_ = SDL_CreateGPUGraphicsPipeline( device_, &info );
    }
    // SSAO pipeline: fullscreen triangle (no vertex input), multiplicative
    // blend onto the scene color, no depth.
    if( main_pipeline_ ) {
        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        color.blend_state.enable_blend = true;
        // result.rgb = dst.rgb * src.rgb (AO factor), alpha preserved.
        color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_COLOR;
        color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = ssao_vs.get();
        info.fragment_shader = ssao_fs.get();
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        ssao_pipeline_ = SDL_CreateGPUGraphicsPipeline( device_, &info );
    }
    // Bloom pipeline: fullscreen (no vertex input), additive blend onto the
    // scene color.
    if( ssao_pipeline_ ) {
        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        color.blend_state.enable_blend = true;
        // result.rgb = dst.rgb + src.rgb (glow); alpha preserved.
        color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = ssao_vs.get();
        info.fragment_shader = bloom_fs.get();
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        bloom_pipeline_ = SDL_CreateGPUGraphicsPipeline( device_, &info );
    }
    // Shadow pipeline: light-space float3 in, nearest light depth out.
    if( main_pipeline_ ) {
        SDL_GPUVertexBufferDescription vb{};
        vb.slot = 0;
        vb.pitch = sizeof( float ) * 3;
        vb.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        SDL_GPUVertexAttribute attr{};
        attr.location = 0;
        attr.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attr.offset = 0;

        SDL_GPUColorTargetDescription color{};
        color.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;

        SDL_GPUGraphicsPipelineCreateInfo info{};
        info.vertex_shader = shadow_vs.get();
        info.fragment_shader = shadow_fs.get();
        info.vertex_input_state.vertex_buffer_descriptions = &vb;
        info.vertex_input_state.num_vertex_buffers = 1;
        info.vertex_input_state.vertex_attributes = &attr;
        info.vertex_input_state.num_vertex_attributes = 1;
        info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        info.target_info.color_target_descriptions = &color;
        info.target_info.num_color_targets = 1;
        info.target_info.has_depth_stencil_target = true;
        info.target_info.depth_stencil_format = depth_format_;
        shadow_pipeline_ = SDL_CreateGPUGraphicsPipeline( device_, &info );
    }

    // Samplers.
    if( shadow_pipeline_ ) {
        SDL_GPUSamplerCreateInfo sinfo{};
        // Nearest + clamp for the manual PCF taps on the shadow map (the
        // shader compares raw depths, so filtering must not blend them).
        sinfo.min_filter = SDL_GPU_FILTER_NEAREST;
        sinfo.mag_filter = SDL_GPU_FILTER_NEAREST;
        sinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sinfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sinfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sinfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        shadow_sampler_ = SDL_CreateGPUSampler( device_, &sinfo );
        // Bilinear + clamp for the tileset atlas: pixel-art sprites and
        // textured tops scale up in the axonometric view, so nearest sampling
        // left them blocky and edge-aliased. Linear smooths the scaling; the
        // block_3d backend insets every atlas UV by half a texel
        // (uv_from_src) so a linear tap never bleeds into a neighbouring
        // sprite in the packed sheet.
        sinfo.min_filter = SDL_GPU_FILTER_LINEAR;
        sinfo.mag_filter = SDL_GPU_FILTER_LINEAR;
        atlas_sampler_ = SDL_CreateGPUSampler( device_, &sinfo );
        // Bilinear + clamp for the bloom blur taps, so each tap averages a
        // 2x2 emissive neighborhood.
        linear_sampler_ = SDL_CreateGPUSampler( device_, &sinfo );
    }

    // Shadow map (R32F light depth) and its own depth buffer.
    if( atlas_sampler_ && shadow_sampler_ && linear_sampler_ ) {
        SDL_GPUTextureCreateInfo tinfo{};
        tinfo.type = SDL_GPU_TEXTURETYPE_2D;
        tinfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        tinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tinfo.width = SHADOW_MAP_SIZE;
        tinfo.height = SHADOW_MAP_SIZE;
        tinfo.layer_count_or_depth = 1;
        tinfo.num_levels = 1;
        shadow_map_ = SDL_CreateGPUTexture( device_, &tinfo );
        tinfo.format = depth_format_;
        tinfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        shadow_depth_ = SDL_CreateGPUTexture( device_, &tinfo );
    }

    if( !main_pipeline_ || !shadow_pipeline_ || !ssao_pipeline_ || !bloom_pipeline_ ||
        !atlas_sampler_ || !shadow_sampler_ || !linear_sampler_ ||
        !shadow_map_ || !shadow_depth_ ) {
        DebugLog( D_ERROR, DC_ALL )
                << "scene_gpu_pass: GPU object creation failed: " << SDL_GetError();
        return false;
    }
    return true;
}

bool scene_gpu_pass::ensure_scene_target( const SDL_Renderer_Ptr &renderer,
        const int w, const int h )
{
    if( !white_gpu_ ) {
        // 1x1 white texture standing in for "no atlas" on flat-colored
        // runs. A pure SDL_GPU texture uploaded through our own command
        // buffer: renderer-texture uploads only reach the GPU when the
        // renderer submits at present time, which is after this pass runs.
        SDL_GPUTextureCreateInfo winfo{};
        winfo.type = SDL_GPU_TEXTURETYPE_2D;
        winfo.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        winfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        winfo.width = 1;
        winfo.height = 1;
        winfo.layer_count_or_depth = 1;
        winfo.num_levels = 1;
        white_gpu_ = SDL_CreateGPUTexture( device_, &winfo );
        SDL_GPUTransferBufferCreateInfo xinfo{};
        xinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xinfo.size = 4;
        SDL_GPUTransferBuffer *xfer = SDL_CreateGPUTransferBuffer( device_, &xinfo );
        SDL_GPUCommandBuffer *cmd = white_gpu_ && xfer
                                    ? SDL_AcquireGPUCommandBuffer( device_ ) : nullptr;
        if( cmd ) {
            const uint32_t white = 0xFFFFFFFFu;
            void *mapped = SDL_MapGPUTransferBuffer( device_, xfer, false );
            if( mapped ) {
                std::memcpy( mapped, &white, 4 );
                SDL_UnmapGPUTransferBuffer( device_, xfer );
                SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass( cmd );
                SDL_GPUTextureTransferInfo src{};
                src.transfer_buffer = xfer;
                SDL_GPUTextureRegion dst{};
                dst.texture = white_gpu_;
                dst.w = 1;
                dst.h = 1;
                dst.d = 1;
                SDL_UploadToGPUTexture( copy, &src, &dst, false );
                SDL_EndGPUCopyPass( copy );
            }
            if( !SDL_SubmitGPUCommandBuffer( cmd ) || !mapped ) {
                cmd = nullptr;
            }
        }
        if( xfer ) {
            SDL_ReleaseGPUTransferBuffer( device_, xfer );
        }
        if( !cmd ) {
            return false;
        }
    }
    if( scene_tex_ && scene_w_ == w && scene_h_ == h ) {
        return true;
    }
    if( scene_tex_ ) {
        SDL_DestroyTexture( scene_tex_ );
        scene_tex_ = nullptr;
        scene_gpu_ = nullptr;
    }
    if( scene_depth_ ) {
        SDL_ReleaseGPUTexture( device_, scene_depth_ );
        scene_depth_ = nullptr;
    }
    if( depth_linear_ ) {
        SDL_ReleaseGPUTexture( device_, depth_linear_ );
        depth_linear_ = nullptr;
    }
    if( emissive_ ) {
        SDL_ReleaseGPUTexture( device_, emissive_ );
        emissive_ = nullptr;
    }
    scene_tex_ = SDL_CreateTexture( renderer.get(), SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_TARGET, w, h );
    if( scene_tex_ ) {
        // Composite by overwrite: the scene fills its viewport entirely.
        SDL_SetTextureBlendMode( scene_tex_, SDL_BLENDMODE_NONE );
        SDL_SetTextureScaleMode( scene_tex_, SDL_SCALEMODE_NEAREST );
        scene_gpu_ = unwrap( scene_tex_ );
    }
    SDL_GPUTextureCreateInfo dinfo{};
    dinfo.type = SDL_GPU_TEXTURETYPE_2D;
    dinfo.format = depth_format_;
    dinfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    dinfo.width = static_cast<Uint32>( w );
    dinfo.height = static_cast<Uint32>( h );
    dinfo.layer_count_or_depth = 1;
    dinfo.num_levels = 1;
    scene_depth_ = SDL_CreateGPUTexture( device_, &dinfo );
    // View-depth target sampled by the SSAO pass.
    dinfo.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    dinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    depth_linear_ = SDL_CreateGPUTexture( device_, &dinfo );
    // Emissive mask sampled by the bloom pass.
    dinfo.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    emissive_ = SDL_CreateGPUTexture( device_, &dinfo );
    if( !scene_gpu_ || !scene_depth_ || !depth_linear_ || !emissive_ ) {
        return false;
    }
    scene_w_ = w;
    scene_h_ = h;
    return true;
}

bool scene_gpu_pass::upload_vertices( SDL_GPUCommandBuffer *cmd, const void *data,
                                      const uint32_t bytes, SDL_GPUBuffer *&buf,
                                      uint32_t &capacity, SDL_GPUTransferBuffer *&transfer,
                                      uint32_t &transfer_capacity )
{
    if( bytes == 0 ) {
        return true;
    }
    if( bytes > capacity ) {
        if( buf ) {
            SDL_ReleaseGPUBuffer( device_, buf );
            buf = nullptr;
        }
        SDL_GPUBufferCreateInfo binfo{};
        binfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        binfo.size = bytes * 2; // headroom against per-frame churn
        buf = SDL_CreateGPUBuffer( device_, &binfo );
        capacity = buf ? binfo.size : 0;
    }
    if( bytes > transfer_capacity ) {
        if( transfer ) {
            SDL_ReleaseGPUTransferBuffer( device_, transfer );
            transfer = nullptr;
        }
        SDL_GPUTransferBufferCreateInfo xinfo{};
        xinfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        xinfo.size = bytes * 2;
        transfer = SDL_CreateGPUTransferBuffer( device_, &xinfo );
        transfer_capacity = transfer ? xinfo.size : 0;
    }
    if( !buf || !transfer ) {
        return false;
    }
    void *mapped = SDL_MapGPUTransferBuffer( device_, transfer, true );
    if( !mapped ) {
        return false;
    }
    std::memcpy( mapped, data, bytes );
    SDL_UnmapGPUTransferBuffer( device_, transfer );
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass( cmd );
    if( !copy ) {
        return false;
    }
    SDL_GPUTransferBufferLocation src{};
    src.transfer_buffer = transfer;
    SDL_GPUBufferRegion dst{};
    dst.buffer = buf;
    dst.size = bytes;
    SDL_UploadToGPUBuffer( copy, &src, &dst, true );
    SDL_EndGPUCopyPass( copy );
    return true;
}

SDL_Texture *scene_gpu_pass::render( const SDL_Renderer_Ptr &renderer,
                                     const int view_w, const int view_h,
                                     const std::vector<render_3d::gpu_vtx> &main_verts,
                                     const std::vector<run> &runs,
                                     const std::vector<float> &shadow_verts,
                                     const bool shadow_pass_wanted, const bool ssao_wanted,
                                     const bool bloom_wanted )
{
    if( !renderer || view_w <= 0 || view_h <= 0 || main_verts.empty() ) {
        return nullptr;
    }
    SDL_GPUDevice *const device = SDL_GetGPURendererDevice( renderer.get() );
    if( !device ) {
        // Not the gpu render driver; nothing to release (never created).
        return nullptr;
    }
    if( device != device_ ) {
        // Renderer was recreated: SDL already destroyed the old device and
        // everything we created on it. Forget and start over.
        abandon_all();
        device_ = device;
        disabled_ = false;
    }
    if( disabled_ ) {
        return nullptr;
    }
    if( !ensure_device_objects() || !ensure_scene_target( renderer, view_w, view_h ) ) {
        release_and_disable();
        return nullptr;
    }

    // Order our command buffer after any batched renderer work that could
    // touch the textures we sample or overwrite.
    SDL_FlushRenderer( renderer.get() );

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer( device_ );
    if( !cmd ) {
        release_and_disable();
        return nullptr;
    }
    const bool draw_shadows = shadow_pass_wanted && !shadow_verts.empty();
    bool ok = upload_vertices( cmd, main_verts.data(),
                               static_cast<uint32_t>( main_verts.size() * sizeof( render_3d::gpu_vtx ) ),
                               main_vbuf_, main_vbuf_cap_, main_xfer_, main_xfer_cap_ );
    if( ok && draw_shadows ) {
        ok = upload_vertices( cmd, shadow_verts.data(),
                              static_cast<uint32_t>( shadow_verts.size() * sizeof( float ) ),
                              shadow_vbuf_, shadow_vbuf_cap_, shadow_xfer_, shadow_xfer_cap_ );
    }

    // Shadow pass — or a pure clear to "no occluder anywhere" when the sun
    // is down, so the main pass always has a defined map to sample.
    if( ok ) {
        SDL_GPUColorTargetInfo color{};
        color.texture = shadow_map_;
        color.clear_color = SDL_FColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        color.load_op = SDL_GPU_LOADOP_CLEAR;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = shadow_depth_;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( cmd, &color, 1, &depth );
        if( pass ) {
            if( draw_shadows ) {
                SDL_BindGPUGraphicsPipeline( pass, shadow_pipeline_ );
                SDL_GPUBufferBinding vb{};
                vb.buffer = shadow_vbuf_;
                SDL_BindGPUVertexBuffers( pass, 0, &vb, 1 );
                SDL_DrawGPUPrimitives( pass,
                                       static_cast<Uint32>( shadow_verts.size() / 3 ), 1, 0, 0 );
            }
            SDL_EndGPURenderPass( pass );
        } else {
            ok = false;
        }
    }

    // Main pass: depth-tested scene into the composite texture, with view
    // depth to a second target for the AO pass.
    if( ok ) {
        SDL_GPUColorTargetInfo colors[3] = {};
        colors[0].texture = scene_gpu_;
        colors[0].clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        colors[0].load_op = SDL_GPU_LOADOP_CLEAR;
        colors[0].store_op = SDL_GPU_STOREOP_STORE;
        colors[1].texture = depth_linear_;
        // Far sentinel so background reads as "no geometry" in the AO pass.
        colors[1].clear_color = SDL_FColor{ -1.0e9f, 0.0f, 0.0f, 0.0f };
        colors[1].load_op = SDL_GPU_LOADOP_CLEAR;
        colors[1].store_op = SDL_GPU_STOREOP_STORE;
        colors[2].texture = emissive_;
        colors[2].clear_color = SDL_FColor{ 0.0f, 0.0f, 0.0f, 1.0f };
        colors[2].load_op = SDL_GPU_LOADOP_CLEAR;
        colors[2].store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPUDepthStencilTargetInfo depth{};
        depth.texture = scene_depth_;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( cmd, colors, 3, &depth );
        if( pass ) {
            SDL_BindGPUGraphicsPipeline( pass, main_pipeline_ );
            SDL_GPUBufferBinding vb{};
            vb.buffer = main_vbuf_;
            SDL_BindGPUVertexBuffers( pass, 0, &vb, 1 );
            for( const run &r : runs ) {
                if( r.count <= 0 ) {
                    continue;
                }
                SDL_GPUTexture *atlas = r.tex ? unwrap( r.tex ) : white_gpu_;
                if( !atlas ) {
                    continue;
                }
                SDL_GPUTextureSamplerBinding samplers[2] = {};
                samplers[0].texture = atlas;
                samplers[0].sampler = atlas_sampler_;
                samplers[1].texture = shadow_map_;
                samplers[1].sampler = shadow_sampler_;
                SDL_BindGPUFragmentSamplers( pass, 0, samplers, 2 );
                SDL_DrawGPUPrimitives( pass, static_cast<Uint32>( r.count ), 1,
                                       static_cast<Uint32>( r.begin ), 0 );
            }
            SDL_EndGPURenderPass( pass );
        } else {
            ok = false;
        }
    }

    // Screen-space AO: a fullscreen pass reads the view-depth target and
    // multiplies contact-shadow darkening into the scene color.
    if( ok && ssao_wanted ) {
        SDL_GPUColorTargetInfo color{};
        color.texture = scene_gpu_;
        color.load_op = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( cmd, &color, 1, nullptr );
        if( pass ) {
            SDL_BindGPUGraphicsPipeline( pass, ssao_pipeline_ );
            SDL_GPUTextureSamplerBinding sampler{};
            sampler.texture = depth_linear_;
            sampler.sampler = shadow_sampler_;
            SDL_BindGPUFragmentSamplers( pass, 0, &sampler, 1 );
            SDL_DrawGPUPrimitives( pass, 3, 1, 0, 0 );
            SDL_EndGPURenderPass( pass );
        } else {
            ok = false;
        }
    }

    // Bloom: a fullscreen pass blurs the emissive mask and adds the glow
    // onto the scene color.
    if( ok && bloom_wanted ) {
        SDL_GPUColorTargetInfo color{};
        color.texture = scene_gpu_;
        color.load_op = SDL_GPU_LOADOP_LOAD;
        color.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass( cmd, &color, 1, nullptr );
        if( pass ) {
            SDL_BindGPUGraphicsPipeline( pass, bloom_pipeline_ );
            SDL_GPUTextureSamplerBinding sampler{};
            sampler.texture = emissive_;
            sampler.sampler = linear_sampler_;
            SDL_BindGPUFragmentSamplers( pass, 0, &sampler, 1 );
            SDL_DrawGPUPrimitives( pass, 3, 1, 0, 0 );
            SDL_EndGPURenderPass( pass );
        } else {
            ok = false;
        }
    }

    if( !SDL_SubmitGPUCommandBuffer( cmd ) || !ok ) {
        DebugLog( D_ERROR, DC_ALL )
                << "scene_gpu_pass: frame failed: " << SDL_GetError();
        release_and_disable();
        return nullptr;
    }
    return scene_tex_;
}

#endif // TILES && SDL3
