#if defined(TILES)

#include <map>
#include <vector>

#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "enums.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "sdl_renderer_recovery.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "type_id.h"
#include "world_renderer.h"

static const ter_str_id ter_t_wall( "t_wall" );

// End-to-end smoke test for the block_3d world renderer: draw the avatar's
// surroundings through the real SDL software renderer and assert pixels
// actually landed on the render target.
TEST_CASE( "block_3d_renderer_draws_scene", "[tiles][render_3d]" )
{
    software_render_fixture fx;
    if( !fx.available() ) {
        WARN( "dummy SDL video backend unavailable; skipping" );
        return;
    }

    clear_avatar();
    clear_map();
    map &here = get_map();
    // Daylight, so tiles around the avatar are seen rather than DARK — the
    // memory write path only fires for tiles the player actually sees.
    set_time_to_day();
    here.build_map_cache( 0 );

    override_option opt( "WORLD_RENDERER", "block_3d" );
    world_renderer &wr = get_active_world_renderer();
    REQUIRE( wr.id() == "block_3d" );

    constexpr int view_size = 64;
    const render_scene scene{ point::zero, get_avatar().pos_bub(), view_size, view_size };
    std::multimap<point, formatted_text> overlay_strings;
    color_block_overlay_container color_blocks;
    wr.draw_world( scene, overlay_strings, color_blocks );

    std::vector<Uint32> pixels( static_cast<size_t>( view_size ) * view_size, 0 );
    const SDL_Rect rect{ 0, 0, view_size, view_size };
    REQUIRE( RenderReadPixels( get_sdl_renderer(), &rect, SDL_PIXELFORMAT_ARGB8888,
                               pixels.data(), view_size * 4 ) );

    // The avatar marker is always drawn in white at the view center, so the
    // frame can never be entirely black.
    int lit_pixels = 0;
    for( const Uint32 px : pixels ) {
        if( ( px & 0x00FFFFFF ) != 0 ) {
            lit_pixels++;
        }
    }
    CHECK( lit_pixels > 0 );

    // The draw pass writes map memory for tiles the avatar can see.
    avatar &you = get_avatar();
    CHECK( you.has_memory_at( here.get_abs( you.pos_bub() + tripoint::east ) ) );

    // Mouse picking through the backend: the pixel at the window center
    // maps to the view-center cell, and stepping a full tile width right
    // moves one cell along the screen-right diagonal (+x, -y).
    const point_bub_ms center_xy( you.pos_bub().xy() );
    CHECK( wr.screen_to_map( point( view_size / 2, view_size / 2 ),
                             point( 32, 32 ), point( view_size, view_size ),
                             center_xy ) == center_xy );
    CHECK( wr.screen_to_map( point( view_size / 2 + 32, view_size / 2 ),
                             point( 32, 32 ), point( view_size, view_size ),
                             center_xy ) == center_xy + point( 1, -1 ) );

    // And remembered terrain renders for unseen tiles: seal a cell behind
    // walls, plant a remembered wall inside, and draw again through the
    // memory path.
    const tripoint_bub_ms hidden = you.pos_bub() + tripoint{ 3, 3, 0 };
    for( const tripoint_bub_ms &wp : here.points_in_radius( hidden, 1 ) ) {
        if( wp != hidden ) {
            here.ter_set( wp, ter_t_wall );
        }
    }
    here.invalidate_map_cache( 0 );
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    REQUIRE( here.get_visibility(
                 here.access_cache( 0 ).visibility_cache[hidden.x()][hidden.y()],
                 here.get_visibility_variables_cache() ) == visibility_type::HIDDEN );
    you.memorize_terrain( here.get_abs( hidden ), "t_wall", 0, 0 );
    wr.draw_world( scene, overlay_strings, color_blocks );
}

#endif // TILES
