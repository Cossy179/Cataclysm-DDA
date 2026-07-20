// Visual-verification probe: renders a representative block_3d scene
// offscreen and dumps it to block3d_frame.bmp for human inspection. Hidden
// tag — run explicitly by name ("block_3d_frame_dump"); SDL3 builds only
// (uses SDL3 surface/target APIs).
#if defined(TILES)

#include "sdl_wrappers.h"

#if SDL_MAJOR_VERSION >= 3

#include <map>
#include <vector>

#include "avatar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "field.h"
#include "field_type.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "sdl_renderer_recovery.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "type_id.h"
#include "uistate.h"
#include "world_renderer.h"

static const ter_str_id ter_t_grass( "t_grass" );
static const ter_str_id ter_t_dirt( "t_dirt" );
static const ter_str_id ter_t_water_dp( "t_water_dp" );
static const ter_str_id ter_t_brick_wall( "t_brick_wall" );
static const ter_str_id ter_t_floor( "t_floor" );
static const ter_str_id ter_t_tree( "t_tree" );
static const ter_str_id ter_t_pavement( "t_pavement" );
static const ter_str_id ter_t_door_c( "t_door_c" );
static const ter_str_id ter_t_flat_roof( "t_flat_roof" );
static const furn_str_id furn_f_counter( "f_counter" );
static const mtype_id mon_zombie_dump( "mon_zombie" );

TEST_CASE( "block_3d_frame_dump", "[.frame-dump]" )
{
    software_render_fixture fx;
    if( !fx.available() ) {
        WARN( "dummy SDL video backend unavailable; skipping" );
        return;
    }

    clear_avatar();
    clear_map();
    map &here = get_map();
    set_time_to_day();

    avatar &you = get_avatar();
    const tripoint_bub_ms c = you.pos_bub();

    // A representative slice of world: grass field, dirt path, pavement,
    // deep water, a brick building with a wood floor, and trees.
    for( int dx = -14; dx <= 14; dx++ ) {
        for( int dy = -14; dy <= 14; dy++ ) {
            here.ter_set( c + point( dx, dy ), ter_t_grass );
        }
    }
    for( int dy = -14; dy <= 14; dy++ ) {
        here.ter_set( c + point( -2, dy ), ter_t_dirt );
        here.ter_set( c + point( -10, dy ), ter_t_pavement );
        here.ter_set( c + point( -11, dy ), ter_t_pavement );
    }
    for( int dx = 8; dx <= 14; dx++ ) {
        for( int dy = -14; dy <= -8; dy++ ) {
            here.ter_set( c + point( dx, dy ), ter_t_water_dp );
        }
    }
    // Building: 7x6 brick shell, wood floor inside, door gap. Placed a few
    // cells southeast so the first-person dump views it from outside.
    for( int dx = 5; dx <= 11; dx++ ) {
        for( int dy = 5; dy <= 10; dy++ ) {
            const bool edge = dx == 5 || dx == 11 || dy == 5 || dy == 10;
            if( edge && !( dx == 8 && dy == 5 ) ) {
                here.ter_set( c + point( dx, dy ), ter_t_brick_wall );
            } else {
                here.ter_set( c + point( dx, dy ), ter_t_floor );
            }
            // Roof above, so the interior has a ceiling in first person.
            here.ter_set( c + tripoint( dx, dy, 1 ), ter_t_flat_roof );
        }
    }
    // Full second story: every wall cell rises another floor (and the roof
    // moves up), so the building reads as a complete two-story house.
    for( int dx = 5; dx <= 11; dx++ ) {
        for( int dy = 5; dy <= 10; dy++ ) {
            const bool edge = dx == 5 || dx == 11 || dy == 5 || dy == 10;
            if( edge ) {
                here.ter_set( c + tripoint( dx, dy, 1 ), ter_t_brick_wall );
            }
        }
    }
    here.ter_set( c + point( -6, -6 ), ter_t_tree );
    here.ter_set( c + point( -8, 4 ), ter_t_tree );
    here.ter_set( c + point( 5, -5 ), ter_t_tree );
    // Tall grass and a fire, for the animated-texture and flame paths.
    for( int dx = -2; dx <= 0; dx++ ) {
        here.ter_set( c + point( dx, 4 ), ter_str_id( "t_grass_tall" ) );
    }
    here.add_field( c + point( 2, 3 ), field_type_id( "fd_fire" ), 2 );
    // A closed door in the building's north face and counters outside, so
    // the first-person dump shows furniture and door detail.
    here.ter_set( c + point( 8, 5 ), ter_t_door_c );
    here.furn_set( c + point( 3, 6 ), furn_f_counter );
    here.furn_set( c + point( 3, 7 ), furn_f_counter );

    // Entities: a zombie, an NPC, and ground items.
    monster &zed = spawn_test_monster( mon_zombie_dump.str(), c + point( -4, -3 ) );
    ( void ) zed;
    spawn_npc( c.xy() + point( 3, -2 ), "test_talker" );
    here.add_item( c + point( 1, 1 ), item( itype_id( "rock" ) ) );

    here.build_map_cache( 0 );

    override_option opt( "WORLD_RENDERER", "block_3d" );
    world_renderer &wr = get_active_world_renderer();
    REQUIRE( wr.id() == "block_3d" );

    // The fixture's window/render target is only 64px; draw into an
    // offscreen target big enough to actually see the scene.
    constexpr int vw = 900;
    constexpr int vh = 700;
    SDL_Texture *const target = SDL_CreateTexture( get_sdl_renderer().get(),
                                SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, vw, vh );
    REQUIRE( target != nullptr );
    SDL_Texture *const prev_target = SDL_GetRenderTarget( get_sdl_renderer().get() );
    REQUIRE( SDL_SetRenderTarget( get_sdl_renderer().get(), target ) );

    const render_scene scene{ point::zero, c, vw, vh };
    std::multimap<point, formatted_text> overlay_strings;
    color_block_overlay_container color_blocks;
    wr.draw_world( scene, overlay_strings, color_blocks );

    std::vector<Uint32> pixels( static_cast<size_t>( vw ) * vh, 0 );
    const SDL_Rect rect{ 0, 0, vw, vh };
    const auto save_frame = [&]( const char *const name ) {
        REQUIRE( RenderReadPixels( get_sdl_renderer(), &rect, SDL_PIXELFORMAT_ARGB8888,
                                   pixels.data(), vw * 4 ) );
        SDL_Surface *dump = SDL_CreateSurfaceFrom( vw, vh, SDL_PIXELFORMAT_ARGB8888,
                            pixels.data(), vw * 4 );
        REQUIRE( dump != nullptr );
        REQUIRE( SDL_SaveBMP( dump, name ) );
        SDL_DestroySurface( dump );
    };
    save_frame( "block3d_frame.bmp" );

    // First-person: step the avatar southeast so the tracked heading faces
    // the brick building, then enter the mode via the zoom hook and draw.
    you.setpos( here, c + tripoint{ 1, 1, 0 } );
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    // One block-view draw absorbs the move into the tracked heading (in
    // first person the camera only turns via the look keys), then enter
    // first person facing the building.
    const render_scene fp_scene{ point::zero, you.pos_bub(), vw, vh };
    wr.draw_world( fp_scene, overlay_strings, color_blocks );
    uistate.tileset_zoom = 128;
    REQUIRE( wr.handle_zoom_in() );
    // The camera eases toward its target over a few hundred ms of drawing;
    // pump frames until it has converged so the dump is deterministic.
    for( int i = 0; i < 30; i++ ) {
        wr.draw_world( fp_scene, overlay_strings, color_blocks );
        SDL_Delay( 20 );
    }
    save_frame( "block3d_fp.bmp" );

    // Indoors: stand inside the building looking at the far wall — the
    // dump should show a ceiling, not sky.
    you.setpos( here, c + tripoint{ 7, 8, 0 } );
    here.build_map_cache( 0 );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( 0 );
    const render_scene in_scene{ point::zero, you.pos_bub(), vw, vh };
    for( int i = 0; i < 30; i++ ) {
        wr.draw_world( in_scene, overlay_strings, color_blocks );
        SDL_Delay( 20 );
    }
    save_frame( "block3d_fp_indoor.bmp" );
    REQUIRE( wr.handle_zoom_out() );

    SDL_SetRenderTarget( get_sdl_renderer().get(), prev_target );
    SDL_DestroyTexture( target );
}

#endif // SDL_MAJOR_VERSION >= 3
#endif // TILES
