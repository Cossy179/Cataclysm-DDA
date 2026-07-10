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

void sun_face_shading( const float azimuth_deg, const float altitude_deg, light_env &env )
{
    if( altitude_deg <= 0.0f ) {
        // Sun below the horizon: flat, slightly cool moonlight.
        env.face_south = 0.72f;
        env.face_east = 0.66f;
        return;
    }
    // Direction toward the sun in map coordinates (+x east, +y south),
    // azimuth measured clockwise from north.
    const float az = azimuth_deg * static_cast<float>( M_PI ) / 180.0f;
    const float to_sun_x = std::sin( az );
    const float to_sun_y = -std::cos( az );
    const auto face = []( const float facing_dot ) {
        return 0.60f + 0.28f * std::max( 0.0f, facing_dot );
    };
    // South faces have normal (0, 1); east faces have normal (1, 0).
    env.face_south = face( to_sun_y );
    env.face_east = face( to_sun_x );
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

rgba grade( const rgba &c, const light_env &env )
{
    const auto mul = []( const uint8_t channel, const float factor ) {
        return static_cast<uint8_t>( std::clamp( static_cast<float>( channel ) * factor,
                                     0.0f, 255.0f ) );
    };
    return rgba{ mul( c.r, env.grade_r ), mul( c.g, env.grade_g ), mul( c.b, env.grade_b ), c.a };
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

    if( top_h > base_h ) {
        const fpoint bw = project( cam, fdx, fdy + 1.0f, fdz + base_h );
        const fpoint bs = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + base_h );
        const fpoint be = project( cam, fdx + 1.0f, fdy, fdz + base_h );
        // South (+y) face: west-top, south-top, south-bottom, west-bottom.
        emit_quad( out, tw, ts, bs, bw, shade( color, shading.south ) );
        // East (+x) face: south-top, east-top, east-bottom, south-bottom.
        emit_quad( out, ts, te, be, bs, shade( color, shading.east ) );
    }
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
