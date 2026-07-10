#include "render_3d.h"

#include <algorithm>

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

} // namespace

void emit_block( std::vector<vtx> &out, const camera &cam, const int dx, const int dy,
                 const int dz, const float base_h, const float top_h, const rgba &color )
{
    const float fdx = static_cast<float>( dx );
    const float fdy = static_cast<float>( dy );
    const float fdz = static_cast<float>( dz );
    // Top face diamond: north, east, south, west corners.
    const fpoint tn = project( cam, fdx, fdy, fdz + top_h );
    const fpoint te = project( cam, fdx + 1.0f, fdy, fdz + top_h );
    const fpoint ts = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + top_h );
    const fpoint tw = project( cam, fdx, fdy + 1.0f, fdz + top_h );
    emit_quad( out, tn, te, ts, tw, shade( color, FACE_TOP ) );

    if( top_h > base_h ) {
        const fpoint bw = project( cam, fdx, fdy + 1.0f, fdz + base_h );
        const fpoint bs = project( cam, fdx + 1.0f, fdy + 1.0f, fdz + base_h );
        const fpoint be = project( cam, fdx + 1.0f, fdy, fdz + base_h );
        // South (+y) face: west-top, south-top, south-bottom, west-bottom.
        emit_quad( out, tw, ts, bs, bw, shade( color, FACE_SOUTH ) );
        // East (+x) face: south-top, east-top, east-bottom, south-bottom.
        emit_quad( out, ts, te, be, bs, shade( color, FACE_EAST ) );
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

} // namespace render_3d
