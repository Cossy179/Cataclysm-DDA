#if defined(TILES)

#include <map>
#include <vector>

#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "map.h"
#include "map_helpers.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "sdl_renderer_recovery.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "world_renderer.h"

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
}

#endif // TILES
