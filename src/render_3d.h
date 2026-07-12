#pragma once
#ifndef CATA_SRC_RENDER_3D_H
#define CATA_SRC_RENDER_3D_H

#include <array>
#include <cstdint>
#include <vector>

/**
 * Pure math for the block_3d world renderer (doc/3D_ROADMAP.md Phase 3):
 * a fixed axonometric camera projecting map cells to screen space, and
 * flat-shaded block geometry emitted as triangles.  Deliberately free of
 * SDL types so it compiles in every build flavor and the projection and
 * mesh emission are unit-testable without a renderer.
 */
namespace render_3d
{

struct rgba {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

/**
 * A screen-space triangle vertex; triples of these form triangles.  u/v are
 * normalized texture coordinates, meaningful only when the batch is drawn
 * with a texture; color modulates the texture.  d is the fractional view
 * depth (world x + y + z at the vertex, in cells/blocks relative to the
 * view center): larger is nearer the camera.  The 2D triangle path ignores
 * it; the GPU scene pass uses it for depth testing and for recovering the
 * world position behind each vertex (see world_from_projected).
 */
struct vtx {
    float x = 0.0f;
    float y = 0.0f;
    rgba c;
    float u = 0.0f;
    float v = 0.0f;
    float d = 0.0f;
};

/** Normalized source rectangle of a sprite within its atlas sheet. */
struct sprite_uv {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

struct fpoint {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * Fixed axonometric camera.  All scale derives from tile_width (the
 * tileset's current, zoom-adjusted tile width); origin is the screen
 * pixel that the view-center cell's north corner projects to.
 */
struct camera {
    int tile_width = 32;
    int origin_x = 0;
    int origin_y = 0;

    int half_w() const {
        return tile_width / 2;
    }
    int quarter_w() const {
        return tile_width / 4;
    }
    /**
     * Pixel height of one full z-level block.  Must stay tile_width / 2:
     * that makes the view direction exactly (1, 1, 1) in cell units,
     * which is what makes depth_key() an exact painter's ordering.
     */
    int block_h() const {
        return tile_width / 2;
    }
};

/**
 * Project a position relative to the view center.  (dx, dy) are cell
 * lattice coordinates — cell (dx, dy) spans corners (dx, dy)..(dx+1, dy+1)
 * — and dz_blocks is z-levels above the center, including fractional
 * in-level height.
 */
fpoint project( const camera &cam, float dx, float dy, float dz_blocks );

/**
 * Inverse of project() on the center z-level at height plane_h: the
 * fractional cell coordinates whose projection is the given screen point.
 * The containing cell is the floor of the results.  Used for mouse
 * picking.
 */
void unproject( const camera &cam, float sx, float sy, float plane_h,
                float &fdx, float &fdy );

/**
 * Painter's-order depth of cell (dx, dy, dz) relative to the view center;
 * larger is nearer the camera.  Exact for axis-aligned unit blocks because
 * block_h == tile_width / 2: blocks with equal keys can never overlap on
 * screen, and any occluder has a strictly larger key.
 */
inline int depth_key( int dx, int dy, int dz )
{
    return dx + dy + dz;
}

/** Face brightness for the fixed camera direction. */
constexpr float FACE_TOP = 1.0f;
constexpr float FACE_SOUTH = 0.80f;
constexpr float FACE_EAST = 0.62f;

/**
 * Per-frame lighting environment (doc/3D_ROADMAP.md Phase 4): dynamic
 * sun-direction face brightness and time-of-day color grading.
 */
struct light_env {
    float face_south = FACE_SOUTH;
    float face_east = FACE_EAST;
    float grade_r = 1.0f;
    float grade_g = 1.0f;
    float grade_b = 1.0f;
};

/**
 * Point the side-face brightness at the sun: faces turned toward the sun
 * brighten, faces away darken.  shadow_x/y is the engine's ground shadow
 * vector (calendar sunlight_angle(): the shadow cast per unit of blocker
 * height, +x east, +y south) — the sun lies opposite it; only its
 * direction is used.  When !sun_up (night) both faces get flat, slightly
 * cool moonlit values and the vector is ignored.
 */
void sun_face_shading( bool sun_up, float shadow_x, float shadow_y, light_env &env );

/**
 * Snap the toward-sun direction (opposite the shadow vector) to a unit
 * 8-way grid step for shadow marching.  A component is set when it lies
 * within 22.5 degrees of that axis.  A zero shadow yields (0, 0).
 */
void sun_step( float shadow_x, float shadow_y, int &step_x, int &step_y );

/**
 * Soft directional shadow factor from the nearest sun-ward occluder:
 * distance 1 -> 0.55, 2 -> 0.72, 3 -> 0.86; anything else -> 1.0.
 */
float sun_shadow_factor( int first_blocker_distance );

/** Color-temperature grading for the time of day; day is identity. */
void time_of_day_grading( bool night, bool dawn_or_dusk, light_env &env );

/**
 * Weather cast, multiplied into the existing grading (composes with
 * time_of_day_grading).  sun_attenuation is incident/raw sunlight in
 * 0..1; the deficit drives a cool overcast darkening, and rain, snow and
 * fog add their own casts.
 */
void weather_grading( float sun_attenuation, bool raining, bool snowing, bool foggy,
                      light_env &env );

/**
 * Night-vision goggles: replace the grading with phosphor green.  Face
 * brightness is left untouched.
 */
void night_vision_grading( light_env &env );

/**
 * Tint a lit tile color by the accumulated colored light at the tile,
 * matching the sprite renderer's overlay semantics: lerp toward the
 * normalized saturated component of (lr, lg, lb) weighted by how much of
 * the tile's total light (scalar) is saturated.  Alpha unchanged.
 */
rgba apply_light_color( const rgba &c, float lr, float lg, float lb, float scalar );

/** Apply the environment's grading multipliers to a color (alpha unchanged). */
rgba grade( const rgba &c, const light_env &env );

/**
 * Ambient-occlusion factor for one top-face corner, from whether the two
 * edge-adjacent neighbor cells and the diagonal neighbor rise above this
 * face: classic voxel contact shadows.
 */
float corner_occlusion( bool side_a, bool side_b, bool diagonal );

/** Per-face shading inputs for emit_block_shaded. */
struct block_shading {
    /** Top-face corner AO factors in north, east, south, west order. */
    std::array<float, 4> top_ao = { 1.0f, 1.0f, 1.0f, 1.0f };
    float south = FACE_SOUTH;
    float east = FACE_EAST;
};

/**
 * Map an ambient light value (map::ambient_light_at, roughly 0..120) to a
 * shading factor: linear up to comfortable indoor light, saturating at
 * daylight, with a floor so dim-but-visible tiles stay readable.
 */
float light_factor( float ambient );

/** Multiply a color's rgb channels by f (alpha unchanged), clamped. */
rgba shade( const rgba &c, float f );

/**
 * Tint for geometry drawn from map memory rather than live sight: dim and
 * desaturated with a blue-gray cast, visually distinct from both lit and
 * dark tiles.  Alpha unchanged.
 */
rgba memory_tint( const rgba &c );

/**
 * Range of view-relative cells whose geometry can appear in a
 * width x height pixel viewport centered on the camera origin, expressed
 * on the diagonal lattice u = dx - dy, v = dx + dy.  z_below is how many
 * z-levels below the camera may be drawn; their geometry projects further
 * down-screen, which widens the v range.
 */
void visible_cell_bounds( const camera &cam, int width, int height, int z_below,
                          int &u_min, int &u_max, int &v_min, int &v_max );

/**
 * Emit a flat-shaded block for cell (dx, dy, dz): top face at top_h, side
 * faces from base_h up to top_h (heights in blocks above the level floor).
 * Emits the camera-visible faces only — top, south (+y) and east (+x) —
 * as two triangles each, with FACE_* multipliers baked into the vertex
 * colors.  A zero-height block (top_h == base_h) emits just the top face.
 */
void emit_block( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                 float base_h, float top_h, const rgba &color );

/**
 * emit_block with per-corner top-face ambient occlusion (interpolated
 * across the face) and dynamic side-face brightness.
 */
void emit_block_shaded( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                        float base_h, float top_h, const rgba &color,
                        const block_shading &shading );

/** Only the two camera-visible side faces of a block. */
void emit_block_sides( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                       float base_h, float top_h, const rgba &color,
                       float south, float east );

/**
 * A block's top face carrying a sprite laid flat across the cell (sprite
 * top-left at the cell's north corner), tinted by `tint`, with per-corner
 * ambient occlusion.  Emit into a batch drawn with the sprite's atlas
 * texture.
 */
void emit_block_top_textured( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                              float top_h, const rgba &tint, const std::array<float, 4> &ao,
                              const sprite_uv &uv );

/**
 * A camera-facing upright sprite standing on cell (dx, dy, dz) at height
 * foot_h — creatures with their real tileset sprites.  aspect is the
 * sprite's height/width ratio; the quad is 0.75 tile widths wide.
 */
void emit_sprite_billboard( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                            float foot_h, float aspect, const rgba &tint, const sprite_uv &uv );

/**
 * Emit a camera-facing vertical diamond standing on cell (dx, dy, dz) at
 * height foot_h, for creatures and the avatar.
 */
void emit_billboard( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                     float foot_h, const rgba &color );

/**
 * Emit a small camera-facing diamond marker on cell (dx, dy, dz) at height
 * foot_h, for items and traps: half the billboard's width, a third of its
 * height.
 */
void emit_marker( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                  float foot_h, const rgba &color );

/**
 * Emit a flat, ground-aligned diamond centered on cell (dx, dy, dz) at
 * height h, spanning size_cells cells across — a translucent light halo
 * around emissive tiles (needs alpha blending; pass color with alpha).
 */
void emit_glow( std::vector<vtx> &out, const camera &cam, int dx, int dy, int dz,
                float h, float size_cells, const rgba &color );

// --- GPU scene pass support (doc/3D_ROADMAP.md Phase 4) ---------------
// Pure math shared by the SDL_GPU depth-buffered scene pass and its
// tests: recovering world positions from projected vertices, the sun's
// orthographic light-space transform for shadow mapping, and packing
// screen vertices into the GPU vertex layout.

/**
 * Invert project() given the vertex's fractional view depth d = x + y + z:
 * the unique world position (in cells/blocks relative to the view center)
 * that projects to screen point (sx, sy) at that depth.  Exact for every
 * vertex emitted from a real world position.
 */
void world_from_projected( const camera &cam, float sx, float sy, float d,
                           float &wx, float &wy, float &wz );

/**
 * Orthographic light-space transform for sun shadow mapping.  Rows map a
 * world position (cells; z in blocks) to shadow-map u, v in [0, 1] and a
 * light-view depth in [0, 1] (smaller is nearer the sun), fitted over a
 * world-space bounding box by sun_light_space().
 */
struct light_space {
    // u = dot(world, (ux, uy, uz)) + uo, likewise v and depth.
    float ux = 0.0f, uy = 0.0f, uz = 0.0f, uo = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f, vo = 0.0f;
    float dx = 0.0f, dy = 0.0f, dz = 0.0f, do_ = 0.0f;

    void apply( float wx, float wy, float wz, float &u, float &v, float &d ) const {
        u = ux * wx + uy * wy + uz * wz + uo;
        v = vx * wx + vy * wy + vz * wz + vo;
        d = dx * wx + dy * wy + dz * wz + do_;
    }
};

/**
 * Build the sun's light space from the engine's ground-shadow vector
 * (sunlight_angle(): the shadow cast per unit of blocker height) and the
 * world-space box (in view-relative cells / blocks) the shadow map must
 * cover.  Returns false when the shadow vector is degenerate or the box
 * is empty; the identity-zero transform then maps everything to depth 0,
 * which reads as fully lit against a far-cleared shadow map.
 */
bool sun_light_space( float shadow_x, float shadow_y,
                      float x0, float y0, float z0, float x1, float y1, float z1,
                      light_space &out );

/**
 * Emit all six faces of the axis-aligned world box spanning
 * (wx0, wy0, wz0)..(wx1, wy1, wz1) (cells; z in blocks) directly in light
 * space, as shadow-caster triangles of (u, v, depth) triples appended to
 * out (three floats per vertex).  Depth-only geometry: no color, no
 * winding significance.  Used for creature/avatar shadow casters.
 */
void emit_box_light_space( std::vector<float> &out, const light_space &ls,
                           float wx0, float wy0, float wz0,
                           float wx1, float wy1, float wz1 );

/**
 * emit_box_light_space for the unit block spanning cells
 * (dx, dy, dz, base_h)..(dx+1, dy+1, dz, top_h).
 */
void emit_block_light_space( std::vector<float> &out, const light_space &ls,
                             int dx, int dy, int dz, float base_h, float top_h );

/**
 * One vertex of the GPU scene pass: clip-space position (x, y in NDC with
 * +y up, z depth in [0, 1] where smaller is nearer), a shadow-receive
 * flag in w, color, atlas uv, and the interpolated light-space coordinate
 * for shadow sampling.
 */
struct gpu_vtx {
    float x = 0.0f, y = 0.0f, z = 0.0f, recv = 0.0f;
    uint8_t r = 0, g = 0, b = 0, a = 255;
    float u = 0.0f, v = 0.0f;
    float lu = 0.0f, lv = 0.0f, ld = 0.0f;
    // Fractional view depth (world x + y + z), passed through for the
    // screen-space AO pass to compare in block units.  Larger is nearer.
    float vd = 0.0f;
};

/**
 * Convert screen-space vertices (from the emit_* family) to GPU scene
 * vertices: viewport-local NDC, depth normalized over [d_min, d_max]
 * (flipped so larger view depth is nearer, i.e. smaller z), light-space
 * coordinates recovered via world_from_projected, and the receive flag.
 */
void pack_gpu_vertices( const std::vector<vtx> &in, const camera &cam,
                        int view_x, int view_y, int view_w, int view_h,
                        float d_min, float d_max, float recv,
                        const light_space &ls, std::vector<gpu_vtx> &out );

} // namespace render_3d

#endif // CATA_SRC_RENDER_3D_H
