#include "render_3d.h"

#include <algorithm>
#include <cmath>

namespace render_3d
{

fpoint project( const camera &cam, const float dx, const float dy, const float dz_blocks )
{
    const float half_w = static_cast<float>( cam.half_w() );
    const float quarter_w = static_cast<float>( cam.quarter_w() );
    const float block_h = static_cast<float>( cam.block_h() );
    return fpoint{
        static_cast<float>( cam.origin_x ) + ( dx - dy ) *half_w,
        static_cast<float>( cam.origin_y ) + ( dx + dy ) *quarter_w - dz_blocks *block_h
    };
}

float light_factor( const float ambient )
{
    return std::clamp( 0.30f + 0.70f * std::min( ambient, 60.0f ) / 60.0f, 0.30f, 1.0f );
}

rgba shade( const rgba &c, const float f )
{
    const auto mul = []( const uint8_t channel, const float factor ) {
        return static_cast<uint8_t>( std::clamp( static_cast<float>( channel ) * factor,
                                     0.0f, 255.0f ) );
    };
    return rgba{ mul( c.r, f ), mul( c.g, f ), mul( c.b, f ), c.a };
}

rgba memory_tint( const rgba &c )
{
    const auto mix = []( const uint8_t channel, const float bias ) {
        return static_cast<uint8_t>( std::clamp( static_cast<float>( channel ) * 0.35f + bias,
                                     0.0f, 255.0f ) );
    };
    return rgba{ mix( c.r, 25.0f ), mix( c.g, 30.0f ), mix( c.b, 45.0f ), c.a };
}

void sun_face_shading( const bool sun_up, const float shadow_x, const float shadow_y,
                       light_env &env )
{
    const float len = std::sqrt( shadow_x * shadow_x + shadow_y * shadow_y );
    if( !sun_up || len <= 0.0f ) {
        // Sun below the horizon: flat, slightly cool moonlight.
        env.face_south = 0.72f;
        env.face_east = 0.66f;
        return;
    }
    // The sun lies opposite the shadow; only the direction matters (the
    // engine's shadow vector scales with cot(altitude)).
    const float to_sun_x = -shadow_x / len;
    const float to_sun_y = -shadow_y / len;
    const auto face = []( const float facing_dot ) {
        return 0.60f + 0.28f * std::max( 0.0f, facing_dot );
    };
    // South faces have normal (0, 1); east faces have normal (1, 0).
    env.face_south = face( to_sun_y );
    env.face_east = face( to_sun_x );
}

void sun_step( const float shadow_x, const float shadow_y, int &step_x, int &step_y )
{
    step_x = 0;
    step_y = 0;
    const float to_sun_x = -shadow_x;
    const float to_sun_y = -shadow_y;
    // tan(22.5 degrees): a component participates in the 8-way step when
    // the direction is within 22.5 degrees of its axis.
    constexpr float threshold = 0.4142f;
    if( std::abs( to_sun_x ) > threshold * std::abs( to_sun_y ) ) {
        step_x = to_sun_x > 0.0f ? 1 : -1;
    }
    if( std::abs( to_sun_y ) > threshold * std::abs( to_sun_x ) ) {
        step_y = to_sun_y > 0.0f ? 1 : -1;
    }
    if( to_sun_x == 0.0f && to_sun_y == 0.0f ) {
        step_x = 0;
        step_y = 0;
    }
}

float sun_shadow_factor( const int first_blocker_distance )
{
    switch( first_blocker_distance ) {
        case 1:
            return 0.55f;
        case 2:
            return 0.72f;
        case 3:
            return 0.86f;
        default:
            return 1.0f;
    }
}

void time_of_day_grading( const bool night, const bool dawn_or_dusk, light_env &env )
{
    if( night ) {
        // Cool blue nights.
        env.grade_r = 0.62f;
        env.grade_g = 0.72f;
        env.grade_b = 1.05f;
    } else if( dawn_or_dusk ) {
        // Warm golden-hour cast.
        env.grade_r = 1.08f;
        env.grade_g = 0.94f;
        env.grade_b = 0.80f;
    } else {
        env.grade_r = 1.0f;
        env.grade_g = 1.0f;
        env.grade_b = 1.0f;
    }
}

void weather_grading( const float sun_attenuation, const bool raining, const bool snowing,
                      const bool foggy, light_env &env )
{
    // Overcast: darken by the sunlight deficit with a cool cast.
    const float d = std::clamp( 1.0f - sun_attenuation, 0.0f, 1.0f );
    env.grade_r *= 1.0f - 0.25f * d;
    env.grade_g *= 1.0f - 0.15f * d;
    env.grade_b *= 1.0f - 0.05f * d;
    if( raining ) {
        env.grade_r *= 0.90f;
        env.grade_g *= 0.93f;
    }
    if( snowing ) {
        env.grade_r *= 1.04f;
        env.grade_g *= 1.06f;
        env.grade_b *= 1.12f;
    }
    if( foggy ) {
        env.grade_r *= 0.92f;
        env.grade_g *= 0.92f;
        env.grade_b *= 0.96f;
    }
}

void night_vision_grading( light_env &env )
{
    env.grade_r = 0.35f;
    env.grade_g = 1.15f;
    env.grade_b = 0.45f;
}

rgba grade( const rgba &c, const light_env &env )
{
    const auto mul = []( const uint8_t channel, const float factor ) {
        return static_cast<uint8_t>( std::clamp( static_cast<float>( channel ) * factor,
                                     0.0f, 255.0f ) );
    };
    return rgba{ mul( c.r, env.grade_r ), mul( c.g, env.grade_g ), mul( c.b, env.grade_b ), c.a };
}

rgba apply_light_color( const rgba &c, const float lr, const float lg, const float lb,
                        const float scalar )
{
    // Mirrors the sprite renderer's colored-light overlay (the
    // light_color_cache consumer in cata_tiles::draw): tint by the
    // saturated component of the accumulated light, weighted by its share
    // of the tile's total light energy, capped at 80/255.
    const float min_ch = std::min( { lr, lg, lb } );
    const float sat_r = lr - min_ch;
    const float sat_g = lg - min_ch;
    const float sat_b = lb - min_ch;
    const float sat_mag = std::max( { sat_r, sat_g, sat_b } );
    if( sat_mag < 0.01f || scalar <= 0.1f ) {
        return c;
    }
    const float ratio = std::min( 1.0f, sat_mag / scalar );
    const float w = ratio * 80.0f / 255.0f;
    const auto mix = [w]( const uint8_t base, const float target ) {
        return static_cast<uint8_t>( std::clamp(
                                         static_cast<float>( base ) * ( 1.0f - w ) + target * w, 0.0f, 255.0f ) );
    };
    return rgba{ mix( c.r, sat_r / sat_mag * 255.0f ),
                 mix( c.g, sat_g / sat_mag * 255.0f ),
                 mix( c.b, sat_b / sat_mag * 255.0f ), c.a };
}

float corner_occlusion( const bool side_a, const bool side_b, const bool diagonal )
{
    if( side_a && side_b ) {
        // Fully wrapped corner: the diagonal can't add more darkness.
        return 0.55f;
    }
    return 1.0f - 0.15f * ( static_cast<float>( side_a ) + static_cast<float>( side_b ) )
           - 0.10f * static_cast<float>( diagonal );
}

void visible_cell_bounds( const camera &cam, const int width, const int height,
                          const int z_below, int &u_min, int &u_max, int &v_min, int &v_max )
{
    const int half_w = std::max( cam.half_w(), 1 );
    const int quarter_w = std::max( cam.quarter_w(), 1 );
    // Geometry from z_below levels down projects up to z_below * block_h
    // pixels further down-screen; pad both v ends so partially-visible
    // blocks at every depth are included.
    const int z_slack = z_below * cam.block_h() / quarter_w;
    u_min = -( width / 2 ) / half_w - 2;
    u_max = ( width / 2 ) / half_w + 2;
    v_min = -( height / 2 ) / quarter_w - z_slack - 3;
    v_max = ( height / 2 ) / quarter_w + z_slack + 3;
}

namespace
{

void emit_quad( std::vector<vtx> &out, const fpoint &a, const fpoint &b,
                const fpoint &c, const fpoint &d, const rgba &color )
{
    out.push_back( vtx{ a.x, a.y, color } );
    out.push_back( vtx{ b.x, b.y, color } );
    out.push_back( vtx{ c.x, c.y, color } );
    out.push_back( vtx{ a.x, a.y, color } );
    out.push_back( vtx{ c.x, c.y, color } );
    out.push_back( vtx{ d.x, d.y, color } );
}

void emit_quad_pv( std::vector<vtx> &out, const fpoint &a, const fpoint &b,
                   const fpoint &c, const fpoint &d, const rgba &ca, const rgba &cb,
                   const rgba &cc, const rgba &cd )
{
    out.push_back( vtx{ a.x, a.y, ca } );
    out.push_back( vtx{ b.x, b.y, cb } );
    out.push_back( vtx{ c.x, c.y, cc } );
    out.push_back( vtx{ a.x, a.y, ca } );
    out.push_back( vtx{ c.x, c.y, cc } );
    out.push_back( vtx{ d.x, d.y, cd } );
}

} // namespace

void emit_block( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                 const int dz, const float base_h, const float top_h, const rgba &color )
{
    emit_block_shaded( out, cam, dx, dy, dz, base_h, top_h, color, block_shading{} );
}

void emit_block_shaded( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                        const int dz, const float base_h, const float top_h, const rgba &color,
                        const block_shading &shading )
{
    const float fdx = static_cast<float>( dx );
    const float fdy = static_cast<float>( dy );
    const float fdz = static_cast<float>( dz );
    // Top face diamond: north, east, south, west corners, with per-corner
    // ambient occlusion interpolated across the face.
    const fpoint tn = project( cam, fdx, fdy, fdz + top_h );
    const fpoint te = project( cam, fdx + 1.0f, fdy, fdz + top_h );
    const fpoint ts = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + top_h );
    const fpoint tw = project( cam, fdx, fdy + 1.0f, fdz + top_h );
    emit_quad_pv( out, tn, te, ts, tw,
                  shade( color, FACE_TOP * shading.top_ao[0] ),
                  shade( color, FACE_TOP * shading.top_ao[1] ),
                  shade( color, FACE_TOP * shading.top_ao[2] ),
                  shade( color, FACE_TOP * shading.top_ao[3] ) );

    emit_block_sides( out, cam, dx, dy, dz, base_h, top_h, color, shading.south, shading.east );
}

void emit_block_sides( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                       const int dz, const float base_h, const float top_h, const rgba &color,
                       const float south, const float east )
{
    if( top_h <= base_h ) {
        return;
    }
    const float fdx = static_cast<float>( dx );
    const float fdy = static_cast<float>( dy );
    const float fdz = static_cast<float>( dz );
    const fpoint te = project( cam, fdx + 1.0f, fdy, fdz + top_h );
    const fpoint ts = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + top_h );
    const fpoint tw = project( cam, fdx, fdy + 1.0f, fdz + top_h );
    const fpoint bw = project( cam, fdx, fdy + 1.0f, fdz + base_h );
    const fpoint bs = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + base_h );
    const fpoint be = project( cam, fdx + 1.0f, fdy, fdz + base_h );
    // South (+y) face: west-top, south-top, south-bottom, west-bottom.
    emit_quad( out, tw, ts, bs, bw, shade( color, south ) );
    // East (+x) face: south-top, east-top, east-bottom, south-bottom.
    emit_quad( out, ts, te, be, bs, shade( color, east ) );
}

void emit_block_top_textured( std::vector<vtx> &out, const camera &cam, const int dx,
                              const int dy, const int dz, const float top_h, const rgba &tint,
                              const std::array<float, 4> &ao, const sprite_uv &uv )
{
    const float fdx = static_cast<float>( dx );
    const float fdy = static_cast<float>( dy );
    const float fdz = static_cast<float>( dz );
    const fpoint tn = project( cam, fdx, fdy, fdz + top_h );
    const fpoint te = project( cam, fdx + 1.0f, fdy, fdz + top_h );
    const fpoint ts = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + top_h );
    const fpoint tw = project( cam, fdx, fdy + 1.0f, fdz + top_h );
    const rgba cn = shade( tint, FACE_TOP * ao[0] );
    const rgba ce = shade( tint, FACE_TOP * ao[1] );
    const rgba cs = shade( tint, FACE_TOP * ao[2] );
    const rgba cw = shade( tint, FACE_TOP * ao[3] );
    // Sprite top-left maps to the cell's north corner, top-right to east,
    // bottom-right to south, bottom-left to west.
    out.push_back( vtx{ tn.x, tn.y, cn, uv.u0, uv.v0 } );
    out.push_back( vtx{ te.x, te.y, ce, uv.u1, uv.v0 } );
    out.push_back( vtx{ ts.x, ts.y, cs, uv.u1, uv.v1 } );
    out.push_back( vtx{ tn.x, tn.y, cn, uv.u0, uv.v0 } );
    out.push_back( vtx{ ts.x, ts.y, cs, uv.u1, uv.v1 } );
    out.push_back( vtx{ tw.x, tw.y, cw, uv.u0, uv.v1 } );
}

void emit_sprite_billboard( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                            const int dz, const float foot_h, const float aspect,
                            const rgba &tint, const sprite_uv &uv )
{
    const fpoint foot = project( cam, static_cast<float>( dx ) + 0.5f,
                                 static_cast<float>( dy ) + 0.5f,
                                 static_cast<float>( dz ) + foot_h );
    const float width = 0.75f * static_cast<float>( cam.tile_width );
    const float height = width * std::max( aspect, 0.1f );
    const float half_width = width / 2.0f;
    const fpoint bl{ foot.x - half_width, foot.y };
    const fpoint br{ foot.x + half_width, foot.y };
    const fpoint tr{ foot.x + half_width, foot.y - height };
    const fpoint tl{ foot.x - half_width, foot.y - height };
    out.push_back( vtx{ tl.x, tl.y, tint, uv.u0, uv.v0 } );
    out.push_back( vtx{ tr.x, tr.y, tint, uv.u1, uv.v0 } );
    out.push_back( vtx{ br.x, br.y, tint, uv.u1, uv.v1 } );
    out.push_back( vtx{ tl.x, tl.y, tint, uv.u0, uv.v0 } );
    out.push_back( vtx{ br.x, br.y, tint, uv.u1, uv.v1 } );
    out.push_back( vtx{ bl.x, bl.y, tint, uv.u0, uv.v1 } );
}

namespace
{

void emit_diamond( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                   const int dz, const float foot_h, const float height, const float half_width,
                   const rgba &color )
{
    const fpoint foot = project( cam, static_cast<float>( dx ) + 0.5f,
                                 static_cast<float>( dy ) + 0.5f,
                                 static_cast<float>( dz ) + foot_h );
    const fpoint top{ foot.x, foot.y - height };
    const fpoint left{ foot.x - half_width, foot.y - height / 2.0f };
    const fpoint right{ foot.x + half_width, foot.y - height / 2.0f };
    out.push_back( vtx{ foot.x, foot.y, color } );
    out.push_back( vtx{ left.x, left.y, color } );
    out.push_back( vtx{ top.x, top.y, color } );
    out.push_back( vtx{ foot.x, foot.y, color } );
    out.push_back( vtx{ top.x, top.y, color } );
    out.push_back( vtx{ right.x, right.y, color } );
}

} // namespace

void emit_billboard( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                     const int dz, const float foot_h, const rgba &color )
{
    emit_diamond( out, cam, dx, dy, dz, foot_h,
                  1.5f * static_cast<float>( cam.block_h() ),
                  static_cast<float>( cam.half_w() ) / 2.0f, color );
}

void emit_marker( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                  const int dz, const float foot_h, const rgba &color )
{
    emit_diamond( out, cam, dx, dy, dz, foot_h,
                  0.5f * static_cast<float>( cam.block_h() ),
                  static_cast<float>( cam.half_w() ) / 4.0f, color );
}

void emit_glow( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                const int dz, const float h, const float size_cells, const rgba &color )
{
    const float cx = static_cast<float>( dx ) + 0.5f;
    const float cy = static_cast<float>( dy ) + 0.5f;
    const float half_size = size_cells / 2.0f;
    const float fz = static_cast<float>( dz ) + h;
    const fpoint n = project( cam, cx - half_size, cy - half_size, fz );
    const fpoint e = project( cam, cx + half_size, cy - half_size, fz );
    const fpoint s = project( cam, cx + half_size, cy + half_size, fz );
    const fpoint w = project( cam, cx - half_size, cy + half_size, fz );
    emit_quad( out, n, e, s, w, color );
}

} // namespace render_3d
