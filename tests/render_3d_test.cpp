#include <algorithm>
#include <cmath>
#include <vector>

#include "cata_catch.h"
#include "render_3d.h"

// Tests for the block_3d renderer's projection math (src/render_3d.h).
// Pure math, so these run in every test binary, tiles or not.

static render_3d::camera test_camera()
{
    render_3d::camera cam;
    cam.tile_width = 32;
    cam.origin_x = 0;
    cam.origin_y = 0;
    return cam;
}

TEST_CASE( "render_3d_projection_steps", "[render_3d]" )
{
    const render_3d::camera cam = test_camera();
    REQUIRE( cam.half_w() == 16 );
    REQUIRE( cam.quarter_w() == 8 );
    // Load-bearing invariant: one z-level equals half a tile width, which
    // makes the view direction (1, 1, 1) and depth_key exact.
    REQUIRE( cam.block_h() == cam.tile_width / 2 );

    const render_3d::fpoint origin = render_3d::project( cam, 0.0f, 0.0f, 0.0f );
    CHECK( origin.x == 0.0f );
    CHECK( origin.y == 0.0f );

    // +x goes screen right-down, +y goes screen left-down, +z straight up.
    const render_3d::fpoint px = render_3d::project( cam, 1.0f, 0.0f, 0.0f );
    CHECK( px.x == 16.0f );
    CHECK( px.y == 8.0f );
    const render_3d::fpoint py = render_3d::project( cam, 0.0f, 1.0f, 0.0f );
    CHECK( py.x == -16.0f );
    CHECK( py.y == 8.0f );
    const render_3d::fpoint pz = render_3d::project( cam, 0.0f, 0.0f, 1.0f );
    CHECK( pz.x == 0.0f );
    CHECK( pz.y == -16.0f );
}

TEST_CASE( "render_3d_unproject_roundtrip", "[render_3d]" )
{
    render_3d::camera cam = test_camera();
    cam.origin_x = 137;   // arbitrary viewport center
    cam.origin_y = -42;
    const float plane_h = 0.125f;

    for( int dy = -7; dy <= 7; dy += 2 ) {
        for( int dx = -7; dx <= 7; dx += 3 ) {
            // Project the middle of the cell's top surface and invert.
            const render_3d::fpoint sp = render_3d::project(
                                             cam, static_cast<float>( dx ) + 0.5f,
                                             static_cast<float>( dy ) + 0.5f, plane_h );
            float fdx = 0.0f;
            float fdy = 0.0f;
            render_3d::unproject( cam, sp.x, sp.y, plane_h, fdx, fdy );
            INFO( "dx=" << dx << " dy=" << dy );
            CHECK( fdx == Approx( static_cast<float>( dx ) + 0.5f ).margin( 0.001 ) );
            CHECK( fdy == Approx( static_cast<float>( dy ) + 0.5f ).margin( 0.001 ) );
            CHECK( static_cast<int>( std::floor( fdx ) ) == dx );
            CHECK( static_cast<int>( std::floor( fdy ) ) == dy );
        }
    }

    // The picking plane height matters: inverting at the wrong plane
    // shifts the result diagonally.
    const render_3d::fpoint sp = render_3d::project( cam, 0.5f, 0.5f, plane_h );
    float fdx = 0.0f;
    float fdy = 0.0f;
    render_3d::unproject( cam, sp.x, sp.y, 0.0f, fdx, fdy );
    CHECK( fdx < 0.5f );
    CHECK( fdy < 0.5f );
}

TEST_CASE( "render_3d_depth_key_ordering", "[render_3d]" )
{
    // An occluder is strictly nearer (larger key) than what it hides.
    CHECK( render_3d::depth_key( 1, 0, 0 ) > render_3d::depth_key( 0, 0, 0 ) );
    CHECK( render_3d::depth_key( 0, 1, 0 ) > render_3d::depth_key( 0, 0, 0 ) );
    CHECK( render_3d::depth_key( 0, 0, 1 ) > render_3d::depth_key( 0, 0, 0 ) );

    // The z-major counterexample: a wall 3 cells south-east on the same
    // level is nearer than a floor one level up at the center, so painting
    // z-levels in order would layer them wrongly.
    CHECK( render_3d::depth_key( 3, 3, 0 ) > render_3d::depth_key( 0, 0, 1 ) );
}

TEST_CASE( "render_3d_visible_cell_bounds_cover_viewport", "[render_3d]" )
{
    const render_3d::camera cam = test_camera();
    const int width = 320;
    const int height = 240;
    const int z_below = 3;

    int u_min = 0;
    int u_max = 0;
    int v_min = 0;
    int v_max = 0;
    render_3d::visible_cell_bounds( cam, width, height, z_below, u_min, u_max, v_min, v_max );
    REQUIRE( u_min < u_max );
    REQUIRE( v_min < v_max );

    // Brute force: every full block whose projected bounding box touches
    // the viewport must have (u, v) inside the reported bounds.
    for( int dz = -z_below; dz <= 0; dz++ ) {
        for( int dy = -60; dy <= 60; dy++ ) {
            for( int dx = -60; dx <= 60; dx++ ) {
                float min_x = 0.0f;
                float max_x = 0.0f;
                float min_y = 0.0f;
                float max_y = 0.0f;
                bool first = true;
                for( int cx = 0; cx <= 1; cx++ ) {
                    for( int cy = 0; cy <= 1; cy++ ) {
                        for( int ch = 0; ch <= 1; ch++ ) {
                            const render_3d::fpoint p = render_3d::project(
                                                            cam, static_cast<float>( dx + cx ), static_cast<float>( dy + cy ),
                                                            static_cast<float>( dz + ch ) );
                            min_x = first ? p.x : std::min( min_x, p.x );
                            max_x = first ? p.x : std::max( max_x, p.x );
                            min_y = first ? p.y : std::min( min_y, p.y );
                            max_y = first ? p.y : std::max( max_y, p.y );
                            first = false;
                        }
                    }
                }
                const bool on_screen = max_x >= -width / 2.0f && min_x <= width / 2.0f &&
                                       max_y >= -height / 2.0f && min_y <= height / 2.0f;
                if( on_screen ) {
                    const int u = dx - dy;
                    const int v = dx + dy;
                    INFO( "dx=" << dx << " dy=" << dy << " dz=" << dz );
                    CHECK( u >= u_min );
                    CHECK( u <= u_max );
                    CHECK( v >= v_min );
                    CHECK( v <= v_max );
                }
            }
        }
    }
}

TEST_CASE( "render_3d_block_emission", "[render_3d]" )
{
    const render_3d::camera cam = test_camera();
    const render_3d::rgba base{ 200, 100, 50, 255 };

    std::vector<render_3d::vtx> out;
    render_3d::emit_block( out, cam, 0, 0, 0, 0.0f, 1.0f, base );
    // Three faces, two triangles each.
    REQUIRE( out.size() == 18 );

    const render_3d::rgba top = render_3d::shade( base, render_3d::FACE_TOP );
    const render_3d::rgba south = render_3d::shade( base, render_3d::FACE_SOUTH );
    const render_3d::rgba east = render_3d::shade( base, render_3d::FACE_EAST );
    const auto color_eq = []( const render_3d::rgba & a, const render_3d::rgba & b ) {
        return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
    };
    for( int i = 0; i < 6; i++ ) {
        CHECK( color_eq( out[i].c, top ) );
        CHECK( color_eq( out[6 + i].c, south ) );
        CHECK( color_eq( out[12 + i].c, east ) );
    }

    // Full block spans exactly one z-level of height on the south corner:
    // the south face's bottom edge is block_h below its top edge.
    const float top_south_y = out[1].y;   // top face, east... see order below
    // Vertices 0..5 are the top face (n, e, s, n, s, w); index 2 is south.
    const float south_corner_top_y = out[2].y;
    const render_3d::fpoint south_corner_bottom =
        render_3d::project( cam, 1.0f, 1.0f, 0.0f );
    CHECK( south_corner_bottom.y - south_corner_top_y ==
           static_cast<float>( cam.block_h() ) );
    ( void ) top_south_y;

    // A zero-height block emits only its top face.
    out.clear();
    render_3d::emit_block( out, cam, 0, 0, 0, 0.5f, 0.5f, base );
    CHECK( out.size() == 6 );

    // A billboard is two triangles.
    out.clear();
    render_3d::emit_billboard( out, cam, 0, 0, 0, 0.0f, base );
    CHECK( out.size() == 6 );

    // A marker is the same diamond shape at half width and a third height.
    out.clear();
    render_3d::emit_marker( out, cam, 0, 0, 0, 0.0f, base );
    REQUIRE( out.size() == 6 );
    // Foot is at the cell center; top is 0.5 block_h above it.
    const render_3d::fpoint center_foot = render_3d::project( cam, 0.5f, 0.5f, 0.0f );
    CHECK( out[0].x == center_foot.x );
    CHECK( out[0].y == center_foot.y );
    CHECK( out[0].y - out[2].y == 0.5f * static_cast<float>( cam.block_h() ) );
    // Side points sit half the marker width from the center line.
    CHECK( out[0].x - out[1].x == static_cast<float>( cam.half_w() ) / 4.0f );
}

TEST_CASE( "render_3d_sun_face_shading", "[render_3d]" )
{
    render_3d::light_env env;

    // Morning sun in the east: shadows point west (-x), so east faces
    // brighten and south faces stay at the base level.
    render_3d::sun_face_shading( true, -2.0f, 0.0f, env );
    CHECK( env.face_east > env.face_south );
    CHECK( env.face_east == Approx( 0.88f ) );
    CHECK( env.face_south == Approx( 0.60f ) );

    // Midday sun in the south: shadows point north (-y); south faces bright.
    render_3d::sun_face_shading( true, 0.0f, -1.0f, env );
    CHECK( env.face_south > env.face_east );
    CHECK( env.face_south == Approx( 0.88f ) );

    // The shadow vector's magnitude (cot altitude) must not matter.
    render_3d::light_env env_long;
    render_3d::sun_face_shading( true, 0.0f, -50.0f, env_long );
    CHECK( env_long.face_south == Approx( env.face_south ) );

    // Night: flat moonlit values, south slightly brighter than east.
    render_3d::sun_face_shading( false, 0.0f, 0.0f, env );
    CHECK( env.face_south == Approx( 0.72f ) );
    CHECK( env.face_east == Approx( 0.66f ) );
}

TEST_CASE( "render_3d_sun_step", "[render_3d]" )
{
    int sx = 9;
    int sy = 9;
    // Shadow west -> sun east -> step east.
    render_3d::sun_step( -1.0f, 0.0f, sx, sy );
    CHECK( sx == 1 );
    CHECK( sy == 0 );
    // Shadow north -> sun south.
    render_3d::sun_step( 0.0f, -1.0f, sx, sy );
    CHECK( sx == 0 );
    CHECK( sy == 1 );
    // Shadow north-west -> sun south-east (diagonal).
    render_3d::sun_step( -1.0f, -1.0f, sx, sy );
    CHECK( sx == 1 );
    CHECK( sy == 1 );
    // Nearly axial: the small component is ignored below the 22.5° cone.
    render_3d::sun_step( -1.0f, -0.2f, sx, sy );
    CHECK( sx == 1 );
    CHECK( sy == 0 );
    // Zero shadow -> no step.
    render_3d::sun_step( 0.0f, 0.0f, sx, sy );
    CHECK( sx == 0 );
    CHECK( sy == 0 );
}

TEST_CASE( "render_3d_sun_shadow_factor", "[render_3d]" )
{
    CHECK( render_3d::sun_shadow_factor( 1 ) == Approx( 0.55f ) );
    CHECK( render_3d::sun_shadow_factor( 2 ) == Approx( 0.72f ) );
    CHECK( render_3d::sun_shadow_factor( 3 ) == Approx( 0.86f ) );
    CHECK( render_3d::sun_shadow_factor( 0 ) == 1.0f );
    CHECK( render_3d::sun_shadow_factor( -1 ) == 1.0f );
    CHECK( render_3d::sun_shadow_factor( 4 ) == 1.0f );
    CHECK( render_3d::sun_shadow_factor( 1 ) < render_3d::sun_shadow_factor( 2 ) );
    CHECK( render_3d::sun_shadow_factor( 2 ) < render_3d::sun_shadow_factor( 3 ) );
}

TEST_CASE( "render_3d_weather_grading", "[render_3d]" )
{
    // Full sun, no precipitation: identity.
    render_3d::light_env env;
    render_3d::weather_grading( 1.0f, false, false, false, env );
    CHECK( env.grade_r == Approx( 1.0f ) );
    CHECK( env.grade_g == Approx( 1.0f ) );
    CHECK( env.grade_b == Approx( 1.0f ) );

    // Heavy overcast: red drops the most, blue the least (cool cast).
    render_3d::weather_grading( 0.3f, false, false, false, env );
    CHECK( env.grade_r < env.grade_g );
    CHECK( env.grade_g < env.grade_b );
    CHECK( env.grade_r == Approx( 1.0f - 0.25f * 0.7f ) );

    // Snow brightens blue relative to red.
    render_3d::light_env snow_env;
    render_3d::weather_grading( 1.0f, false, true, false, snow_env );
    CHECK( snow_env.grade_b > snow_env.grade_r );
    CHECK( snow_env.grade_b == Approx( 1.12f ) );

    // Composes multiplicatively with existing (dusk) grades.
    render_3d::light_env dusk_env;
    render_3d::time_of_day_grading( false, true, dusk_env );
    const float dusk_r = dusk_env.grade_r;
    render_3d::weather_grading( 1.0f, true, false, false, dusk_env );
    CHECK( dusk_env.grade_r == Approx( dusk_r * 0.90f ) );
}

TEST_CASE( "render_3d_night_vision_grading", "[render_3d]" )
{
    render_3d::light_env env;
    render_3d::night_vision_grading( env );
    CHECK( env.grade_g > 1.0f );
    CHECK( env.grade_r < env.grade_g );
    CHECK( env.grade_b < env.grade_g );
    const render_3d::rgba gray = render_3d::grade( render_3d::rgba{ 100, 100, 100, 255 }, env );
    CHECK( gray.g > gray.r );
    CHECK( gray.g > gray.b );
}

TEST_CASE( "render_3d_apply_light_color", "[render_3d]" )
{
    const render_3d::rgba base{ 100, 100, 100, 200 };

    // White (unsaturated) light: unchanged.
    const render_3d::rgba white = render_3d::apply_light_color( base, 5.0f, 5.0f, 5.0f, 10.0f );
    CHECK( white.r == 100 );
    CHECK( white.g == 100 );
    CHECK( white.b == 100 );

    // Negligible total light: unchanged.
    const render_3d::rgba dark = render_3d::apply_light_color( base, 1.0f, 0.0f, 0.0f, 0.05f );
    CHECK( dark.r == 100 );

    // Pure red light saturating the tile's light: lerp toward red by 80/255.
    const render_3d::rgba red = render_3d::apply_light_color( base, 8.0f, 0.0f, 0.0f, 8.0f );
    const float w = 80.0f / 255.0f;
    CHECK( red.r == static_cast<uint8_t>( 100 * ( 1.0f - w ) + 255.0f * w ) );
    CHECK( red.g == static_cast<uint8_t>( 100 * ( 1.0f - w ) ) );
    CHECK( red.a == 200 );
}

TEST_CASE( "render_3d_time_of_day_grading", "[render_3d]" )
{
    render_3d::light_env env;

    render_3d::time_of_day_grading( false, false, env );
    const render_3d::rgba c{ 100, 100, 100, 255 };
    const render_3d::rgba day = render_3d::grade( c, env );
    CHECK( day.r == 100 );
    CHECK( day.g == 100 );
    CHECK( day.b == 100 );

    render_3d::time_of_day_grading( true, false, env );
    const render_3d::rgba night = render_3d::grade( c, env );
    CHECK( night.b > night.r );  // cool blue nights
    CHECK( night.r < 100 );
    CHECK( night.a == 255 );

    render_3d::time_of_day_grading( false, true, env );
    const render_3d::rgba dusk = render_3d::grade( c, env );
    CHECK( dusk.r > dusk.b );    // warm golden hour
}

TEST_CASE( "render_3d_corner_occlusion", "[render_3d]" )
{
    // Open corner: no darkening.
    CHECK( render_3d::corner_occlusion( false, false, false ) == 1.0f );
    // Diagonal only: slight.
    CHECK( render_3d::corner_occlusion( false, false, true ) == Approx( 0.90f ) );
    // One side.
    CHECK( render_3d::corner_occlusion( true, false, false ) == Approx( 0.85f ) );
    // Both sides wrap the corner fully; the diagonal adds nothing.
    CHECK( render_3d::corner_occlusion( true, true, false ) == Approx( 0.55f ) );
    CHECK( render_3d::corner_occlusion( true, true, true ) == Approx( 0.55f ) );
    // Monotonic: more occluders never brighten.
    CHECK( render_3d::corner_occlusion( true, false, true ) <
           render_3d::corner_occlusion( true, false, false ) );
}

TEST_CASE( "render_3d_shaded_block_emission", "[render_3d]" )
{
    const render_3d::camera cam = test_camera();
    const render_3d::rgba base{ 200, 100, 50, 255 };

    // Corner AO shows up in the top face's per-vertex colors.
    render_3d::block_shading shading;
    shading.top_ao = { 1.0f, 1.0f, 0.55f, 1.0f };  // south corner occluded
    std::vector<render_3d::vtx> out;
    render_3d::emit_block_shaded( out, cam, 0, 0, 0, 0.0f, 1.0f, base, shading );
    REQUIRE( out.size() == 18 );
    const render_3d::rgba open_corner = render_3d::shade( base, render_3d::FACE_TOP );
    const render_3d::rgba dark_corner = render_3d::shade( base, render_3d::FACE_TOP * 0.55f );
    // Top face vertex order: n, e, s, n, s, w.
    CHECK( out[0].c.r == open_corner.r );
    CHECK( out[2].c.r == dark_corner.r );
    CHECK( out[2].c.r < out[0].c.r );

    // Dynamic side-face brightness replaces the static face constants.
    shading = render_3d::block_shading{};
    shading.south = 0.9f;
    shading.east = 0.4f;
    out.clear();
    render_3d::emit_block_shaded( out, cam, 0, 0, 0, 0.0f, 1.0f, base, shading );
    const render_3d::rgba south = render_3d::shade( base, 0.9f );
    const render_3d::rgba east = render_3d::shade( base, 0.4f );
    CHECK( out[6].c.r == south.r );
    CHECK( out[12].c.r == east.r );

    // A glow is a single translucent quad.
    out.clear();
    render_3d::emit_glow( out, cam, 0, 0, 0, 0.2f, 2.0f, render_3d::rgba{ 255, 160, 40, 80 } );
    REQUIRE( out.size() == 6 );
    CHECK( out[0].c.a == 80 );
}

TEST_CASE( "render_3d_textured_emission", "[render_3d]" )
{
    const render_3d::camera cam = test_camera();
    const render_3d::rgba tint{ 255, 255, 255, 255 };
    const render_3d::sprite_uv uv{ 0.25f, 0.5f, 0.5f, 0.75f };

    // Textured top face: sprite corners land on the cell's diamond corners.
    std::vector<render_3d::vtx> out;
    const std::array<float, 4> no_ao = { 1.0f, 1.0f, 1.0f, 1.0f };
    render_3d::emit_block_top_textured( out, cam, 0, 0, 0, 1.0f, tint, no_ao, uv );
    REQUIRE( out.size() == 6 );
    // Vertex order: n, e, s, n, s, w.
    CHECK( out[0].u == 0.25f );  // north = sprite top-left
    CHECK( out[0].v == 0.5f );
    CHECK( out[1].u == 0.5f );   // east = sprite top-right
    CHECK( out[1].v == 0.5f );
    CHECK( out[2].u == 0.5f );   // south = sprite bottom-right
    CHECK( out[2].v == 0.75f );
    CHECK( out[5].u == 0.25f );  // west = sprite bottom-left
    CHECK( out[5].v == 0.75f );
    const render_3d::fpoint north = render_3d::project( cam, 0.0f, 0.0f, 1.0f );
    CHECK( out[0].x == north.x );
    CHECK( out[0].y == north.y );

    // Sides alone are two faces, twelve vertices.
    out.clear();
    render_3d::emit_block_sides( out, cam, 0, 0, 0, 0.0f, 1.0f, tint, 0.8f, 0.6f );
    CHECK( out.size() == 12 );
    // Zero-height blocks have no sides.
    out.clear();
    render_3d::emit_block_sides( out, cam, 0, 0, 0, 0.5f, 0.5f, tint, 0.8f, 0.6f );
    CHECK( out.empty() );

    // Sprite billboard: upright quad, aspect controls its height.
    out.clear();
    render_3d::emit_sprite_billboard( out, cam, 0, 0, 0, 0.0f, 2.0f, tint, uv );
    REQUIRE( out.size() == 6 );
    const float width = 0.75f * static_cast<float>( cam.tile_width );
    // Vertex order: tl, tr, br, tl, br, bl.
    CHECK( out[1].x - out[0].x == width );
    CHECK( out[2].y - out[1].y == width * 2.0f );
    CHECK( out[0].u == 0.25f );
    CHECK( out[0].v == 0.5f );
    CHECK( out[2].u == 0.5f );
    CHECK( out[2].v == 0.75f );

    // Untextured vertices default to uv (0, 0).
    out.clear();
    render_3d::emit_marker( out, cam, 0, 0, 0, 0.0f, tint );
    REQUIRE( !out.empty() );
    CHECK( out[0].u == 0.0f );
    CHECK( out[0].v == 0.0f );
}

TEST_CASE( "render_3d_memory_tint", "[render_3d]" )
{
    // Dim, desaturated, blue-shifted; alpha preserved; clamped.
    const render_3d::rgba c{ 200, 100, 40, 200 };
    const render_3d::rgba m = render_3d::memory_tint( c );
    CHECK( m.r == 95 );   // 200 * 0.35 + 25
    CHECK( m.g == 65 );   // 100 * 0.35 + 30
    CHECK( m.b == 59 );   // 40 * 0.35 + 45
    CHECK( m.a == 200 );

    const render_3d::rgba white = render_3d::memory_tint( render_3d::rgba{ 255, 255, 255, 255 } );
    CHECK( white.r < 255 );
    CHECK( white.b > white.r );  // the blue cast survives even from white
}

TEST_CASE( "render_3d_light_dir_face_shading", "[render_3d]" )
{
    float south = 0.0f;
    float east = 0.0f;

    // No direction or zero strength: neutral face defaults.
    render_3d::light_dir_face_shading( 0.0f, 0.0f, 1.0f, south, east );
    CHECK( south == Approx( render_3d::FACE_SOUTH ) );
    CHECK( east == Approx( render_3d::FACE_EAST ) );
    render_3d::light_dir_face_shading( 1.0f, 1.0f, 0.0f, south, east );
    CHECK( south == Approx( render_3d::FACE_SOUTH ) );
    CHECK( east == Approx( render_3d::FACE_EAST ) );

    // Light due south (+y) brightens the south face; the east face gets
    // the away-from-light minimum. Full strength.
    render_3d::light_dir_face_shading( 0.0f, 1.0f, 1.0f, south, east );
    CHECK( south == Approx( 0.90f ) ); // 0.55 + 0.35
    CHECK( east == Approx( 0.55f ) );  // 0.55 + 0.35*max(0, 0)
    CHECK( south > east );

    // Light due east (+x) brightens the east face instead.
    render_3d::light_dir_face_shading( 1.0f, 0.0f, 1.0f, south, east );
    CHECK( east == Approx( 0.90f ) );
    CHECK( south == Approx( 0.55f ) );
    CHECK( east > south );

    // Partial strength blends toward the neutral default.
    float s_full = 0.0f;
    float e_full = 0.0f;
    float s_half = 0.0f;
    float e_half = 0.0f;
    render_3d::light_dir_face_shading( 0.0f, 1.0f, 1.0f, s_full, e_full );
    render_3d::light_dir_face_shading( 0.0f, 1.0f, 0.5f, s_half, e_half );
    CHECK( s_half < s_full );
    CHECK( s_half > render_3d::FACE_SOUTH );
}

TEST_CASE( "render_3d_light_factor", "[render_3d]" )
{
    // Floor for darkness, saturation at daylight, monotonic between.
    CHECK( render_3d::light_factor( 0.0f ) == 0.45f );
    CHECK( render_3d::light_factor( -5.0f ) == 0.45f );
    CHECK( render_3d::light_factor( 50.0f ) == 1.0f );
    CHECK( render_3d::light_factor( 60.0f ) == 1.0f );
    CHECK( render_3d::light_factor( 120.0f ) == 1.0f );
    CHECK( render_3d::light_factor( 30.0f ) > render_3d::light_factor( 10.0f ) );

    // Shading clamps and preserves alpha.
    const render_3d::rgba c{ 200, 100, 50, 128 };
    const render_3d::rgba half = render_3d::shade( c, 0.5f );
    CHECK( half.r == 100 );
    CHECK( half.g == 50 );
    CHECK( half.b == 25 );
    CHECK( half.a == 128 );
    const render_3d::rgba over = render_3d::shade( c, 2.0f );
    CHECK( over.r == 255 );
    CHECK( over.a == 128 );
}

TEST_CASE( "render_3d_world_from_projected_roundtrip", "[render_3d]" )
{
    // world_from_projected must invert project() exactly (up to float
    // noise) given the vertex's fractional view depth, including on tile
    // widths where block_h is not exactly twice quarter_w.
    for( const int tw : {
             32, 34, 20
         } ) {
        render_3d::camera cam;
        cam.tile_width = tw;
        cam.origin_x = 321;
        cam.origin_y = 87;
        const float world[][3] = {
            { 0.0f, 0.0f, 0.0f },
            { 3.0f, -2.0f, 1.5f },
            { -4.25f, 7.5f, -3.125f },
            { 0.5f, 0.5f, 0.125f },
        };
        for( const auto &w : world ) {
            const render_3d::fpoint p = render_3d::project( cam, w[0], w[1], w[2] );
            float wx = 0.0f;
            float wy = 0.0f;
            float wz = 0.0f;
            render_3d::world_from_projected( cam, p.x, p.y, w[0] + w[1] + w[2],
                                             wx, wy, wz );
            CHECK( wx == Approx( w[0] ).margin( 0.001 ) );
            CHECK( wy == Approx( w[1] ).margin( 0.001 ) );
            CHECK( wz == Approx( w[2] ).margin( 0.001 ) );
        }
    }
}

TEST_CASE( "render_3d_vertex_view_depth", "[render_3d]" )
{
    // Every emitter fills vtx::d with the world x + y + z of the vertex,
    // consistent with world_from_projected recovering the position.
    const render_3d::camera cam = test_camera();
    std::vector<render_3d::vtx> out;
    render_3d::emit_block_shaded( out, cam, 2, 3, -1, 0.0f, 1.0f,
                                  render_3d::rgba{ 200, 100, 50, 255 },
                                  render_3d::block_shading{} );
    REQUIRE( out.size() >= 6 );
    // First vertex: top-face north corner (2, 3, -1 + 1.0).
    CHECK( out[0].d == Approx( 2.0f + 3.0f - 1.0f + 1.0f ) );
    for( const render_3d::vtx &v : out ) {
        float wx = 0.0f;
        float wy = 0.0f;
        float wz = 0.0f;
        render_3d::world_from_projected( cam, v.x, v.y, v.d, wx, wy, wz );
        // Recovered positions sit on the emitted block's corner lattice.
        CHECK( wx >= 1.99f );
        CHECK( wx <= 3.01f );
        CHECK( wy >= 2.99f );
        CHECK( wy <= 4.01f );
        CHECK( wz >= -1.01f );
        CHECK( wz <= 0.01f );
    }
}

TEST_CASE( "render_3d_sun_light_space", "[render_3d]" )
{
    render_3d::light_space ls;
    // Late-afternoon sun in the west: shadows displace east (+x).
    REQUIRE( render_3d::sun_light_space( 1.5f, 0.0f,
                                         -10.0f, -10.0f, -2.0f, 10.0f, 10.0f, 3.0f, ls ) );

    // Every corner of the fitted box maps into [0, 1] on all three axes.
    for( int i = 0; i < 8; i++ ) {
        const float wx = ( i & 1 ) != 0 ? 10.0f : -10.0f;
        const float wy = ( i & 2 ) != 0 ? 10.0f : -10.0f;
        const float wz = ( i & 4 ) != 0 ? 3.0f : -2.0f;
        float u = 0.0f;
        float v = 0.0f;
        float d = 0.0f;
        ls.apply( wx, wy, wz, u, v, d );
        CHECK( u >= 0.0f );
        CHECK( u <= 1.0f );
        CHECK( v >= 0.0f );
        CHECK( v <= 1.0f );
        CHECK( d >= 0.0f );
        CHECK( d <= 1.0f );
    }

    // Points along one light ray share a shadow-map texel and get deeper
    // (further from the sun) as they descend toward the ground: the wall
    // top at (0, 0, 1) shades the ground point at (1.5, 0, 0).
    float u0 = 0.0f;
    float v0 = 0.0f;
    float d0 = 0.0f;
    float u1 = 0.0f;
    float v1 = 0.0f;
    float d1 = 0.0f;
    ls.apply( 0.0f, 0.0f, 1.0f, u0, v0, d0 );
    ls.apply( 1.5f, 0.0f, 0.0f, u1, v1, d1 );
    CHECK( u1 == Approx( u0 ).margin( 0.0001 ) );
    CHECK( v1 == Approx( v0 ).margin( 0.0001 ) );
    CHECK( d1 > d0 );

    // Degenerate box refuses.
    render_3d::light_space bad;
    CHECK_FALSE( render_3d::sun_light_space( 1.0f, 0.0f,
                 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, bad ) );
}

TEST_CASE( "render_3d_pack_gpu_vertices", "[render_3d]" )
{
    render_3d::camera cam;
    cam.tile_width = 32;
    cam.origin_x = 100;
    cam.origin_y = 100;
    render_3d::light_space ls;
    REQUIRE( render_3d::sun_light_space( 1.0f, 0.5f,
                                         -20.0f, -20.0f, -5.0f, 20.0f, 20.0f, 5.0f, ls ) );

    std::vector<render_3d::vtx> in;
    render_3d::emit_block( in, cam, 0, 0, 0, 0.0f, 1.0f,
                           render_3d::rgba{ 10, 20, 30, 255 } );
    render_3d::emit_block( in, cam, 3, 3, 0, 0.0f, 1.0f,
                           render_3d::rgba{ 10, 20, 30, 255 } );

    std::vector<render_3d::gpu_vtx> out;
    render_3d::pack_gpu_vertices( in, cam, 40, 60, 120, 80,
                                  -10.0f, 12.0f, 1.0f, 1.0f, ls, out );
    REQUIRE( out.size() == in.size() );

    for( size_t i = 0; i < out.size(); i++ ) {
        // NDC from viewport-local pixels, +y up.
        const float lx = ( in[i].x - 40.0f ) / 120.0f * 2.0f - 1.0f;
        const float ly = -( ( in[i].y - 60.0f ) / 80.0f * 2.0f - 1.0f );
        CHECK( out[i].x == Approx( lx ).margin( 0.0001 ) );
        CHECK( out[i].y == Approx( ly ).margin( 0.0001 ) );
        // View depth passes straight through for the SSAO pass; emit for
        // the bloom pass.
        CHECK( out[i].vd == Approx( in[i].d ) );
        CHECK( out[i].emit == 1.0f );
        CHECK( out[i].recv == 1.0f );
        // Color passes through exactly as emitted (already face-shaded).
        CHECK( out[i].r == in[i].c.r );
        CHECK( out[i].a == in[i].c.a );
        // Light coords equal transforming the recovered world position.
        float wx = 0.0f;
        float wy = 0.0f;
        float wz = 0.0f;
        render_3d::world_from_projected( cam, in[i].x, in[i].y, in[i].d, wx, wy, wz );
        float lu = 0.0f;
        float lv = 0.0f;
        float ld = 0.0f;
        ls.apply( wx, wy, wz, lu, lv, ld );
        CHECK( out[i].lu == Approx( lu ).margin( 0.0001 ) );
        CHECK( out[i].ld == Approx( ld ).margin( 0.0001 ) );
    }

    // The second block sits deeper into the scene (larger view depth), so
    // it must get a smaller depth z than the first block's same corner.
    CHECK( out[in.size() / 2].z < out[0].z );

    // Shadow casters in light space: 6 faces x 2 triangles x 3 vertices x
    // 3 floats.
    std::vector<float> shadow;
    render_3d::emit_block_light_space( shadow, ls, 0, 0, 0, 0.0f, 1.0f );
    CHECK( shadow.size() == 6u * 2u * 3u * 3u );
}

TEST_CASE( "render_3d_box_light_space", "[render_3d]" )
{
    render_3d::light_space ls;
    REQUIRE( render_3d::sun_light_space( 1.0f, 0.5f,
                                         -20.0f, -20.0f, -5.0f, 20.0f, 20.0f, 5.0f, ls ) );

    // emit_block_light_space is exactly emit_box_light_space over the unit
    // block spanning the cell's corner lattice.
    std::vector<float> from_block;
    std::vector<float> from_box;
    render_3d::emit_block_light_space( from_block, ls, 2, -3, 1, 0.0f, 0.5f );
    render_3d::emit_box_light_space( from_box, ls, 2.0f, -3.0f, 1.0f,
                                     3.0f, -2.0f, 1.5f );
    REQUIRE( from_block.size() == from_box.size() );
    REQUIRE( from_box.size() == 6u * 2u * 3u * 3u );
    for( size_t i = 0; i < from_box.size(); i++ ) {
        CHECK( from_block[i] == Approx( from_box[i] ) );
    }

    // A fractional creature-blob caster maps entirely inside the fitted
    // shadow map, and its span reflects the requested box size.
    std::vector<float> blob;
    render_3d::emit_box_light_space( blob, ls, 0.25f, 0.25f, 0.0f,
                                     0.75f, 0.75f, 1.2f );
    REQUIRE( blob.size() == 6u * 2u * 3u * 3u );
    for( size_t i = 0; i + 2 < blob.size(); i += 3 ) {
        CHECK( blob[i] >= 0.0f );
        CHECK( blob[i] <= 1.0f );
        CHECK( blob[i + 1] >= 0.0f );
        CHECK( blob[i + 1] <= 1.0f );
        CHECK( blob[i + 2] >= 0.0f );
        CHECK( blob[i + 2] <= 1.0f );
    }
}
