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

/** A screen-space triangle vertex; triples of these form triangles. */
struct vtx {
    float x = 0.0f;
    float y = 0.0f;
    rgba c;
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
 * Point the side-face brightness at the sun: faces turned toward the sun's
 * azimuth brighten, faces away darken.  Below the horizon (night) both
 * faces get flat, slightly cool moonlit values.  Azimuth is degrees
 * clockwise from north.
 */
void sun_face_shading( float azimuth_deg, float altitude_deg, light_env &env );

/** Color-temperature grading for the time of day; day is identity. */
void time_of_day_grading( bool night, bool dawn_or_dusk, light_env &env );

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

} // namespace render_3d

#endif // CATA_SRC_RENDER_3D_H
