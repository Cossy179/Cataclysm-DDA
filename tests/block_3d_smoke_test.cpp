#if defined(TILES)

#include <map>
#include <memory>
#include <vector>

#include "avatar.h"
#include "block3d_atlas.h"
#include "cata_catch.h"
#include "cata_tiles.h"
#include "coordinates.h"
#include "enums.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "sdl_geometry.h"
#include "sdl_renderer_recovery.h"
#include "sdl_wrappers.h"
#include "sdltiles.h"
#include "type_id.h"
#include "uistate.h"
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

// The built-in entity + terrain atlases the block_3d backend uses in place
// of diamonds/flat colors are embedded in the binary; they must decode with
// the cell-grid dimensions the renderer's UV math assumes — otherwise the 3D
// view silently falls back to placeholder primitives again.
TEST_CASE( "block_3d_entity_atlas_present", "[tiles][render_3d]" )
{
    SDL_Surface_Ptr entities;
    REQUIRE_NOTHROW( entities = load_image_mem( block3d_entities_png,
                                block3d_entities_png_len ) );
    REQUIRE( entities );
    // 8x4 grid of 64px cells (tools/gfx/gen_block3d_sprites.py).
    CHECK( entities->w == 512 );
    CHECK( entities->h == 256 );

    SDL_Surface_Ptr terrain;
    REQUIRE_NOTHROW( terrain = load_image_mem( block3d_terrain_png,
                               block3d_terrain_png_len ) );
    REQUIRE( terrain );
    // 8x4 grid of 32px cells.
    CHECK( terrain->w == 256 );
    CHECK( terrain->h == 128 );
}

// The character- and item-sprite paths: the avatar (with worn gear) and a
// nearby NPC route through emit_character (base sprite plus one billboard per
// resolved worn-item / mutation overlay), and a ground item routes through
// the item-sprite lookup. The bare test tileset has no character or item
// sprites, so this exercises the overlay-id traversal
// (Character::get_overlay_ids), the item ITEM-category lookup, and the
// diamond/marker fallbacks without a tileset — the frame must still render
// and stay navigable.
TEST_CASE( "block_3d_renderer_draws_characters", "[tiles][render_3d]" )
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
    // Worn gear feeds Character::get_overlay_ids(); with no tileset loaded
    // the overlays resolve to nothing but the traversal still runs.
    you.wear_item( item( itype_id( "jeans" ) ), false );

    // A visible NPC one cell away reaches emit_creature -> emit_character.
    spawn_npc( you.pos_bub().xy() + point::east, "test_talker" );

    // A ground item under the avatar drives the item-sprite path
    // (sprite_for ITEM lookup, marker fallback with no tileset loaded).
    here.add_item( you.pos_bub(), item( itype_id( "rock" ) ) );
    here.build_map_cache( 0 );

    override_option opt( "WORLD_RENDERER", "block_3d" );
    world_renderer &wr = get_active_world_renderer();
    REQUIRE( wr.id() == "block_3d" );

    constexpr int view_size = 64;
    const render_scene scene{ point::zero, you.pos_bub(), view_size, view_size };
    std::multimap<point, formatted_text> overlay_strings;
    color_block_overlay_container color_blocks;
    wr.draw_world( scene, overlay_strings, color_blocks );

    std::vector<Uint32> pixels( static_cast<size_t>( view_size ) * view_size, 0 );
    const SDL_Rect rect{ 0, 0, view_size, view_size };
    REQUIRE( RenderReadPixels( get_sdl_renderer(), &rect, SDL_PIXELFORMAT_ARGB8888,
                               pixels.data(), view_size * 4 ) );
    int lit_pixels = 0;
    for( const Uint32 px : pixels ) {
        if( ( px & 0x00FFFFFF ) != 0 ) {
            lit_pixels++;
        }
    }
    // Avatar diamond (fallback) plus the NPC marker must land on the target.
    CHECK( lit_pixels > 0 );
}

// First-person mode: zooming in past maximum tile zoom enters it through
// the world_renderer zoom hooks, the raycast view draws (sky at minimum),
// and zooming out leaves it again.
TEST_CASE( "block_3d_first_person_smoke", "[tiles][render_3d]" )
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
    // A wall ahead of the default (north-facing) heading so the view has
    // both sky and wall geometry.
    here.ter_set( you.pos_bub() + tripoint{ 0, -3, 0 }, ter_t_wall );
    here.build_map_cache( 0 );

    override_option opt( "WORLD_RENDERER", "block_3d" );
    world_renderer &wr = get_active_world_renderer();
    REQUIRE( wr.id() == "block_3d" );

    const int saved_zoom = uistate.tileset_zoom;
    uistate.tileset_zoom = 128;
    // Below max zoom the hook declines; at max it enters first person.
    uistate.tileset_zoom = 64;
    CHECK_FALSE( wr.handle_zoom_in() );
    uistate.tileset_zoom = 128;
    REQUIRE( wr.handle_zoom_in() );

    constexpr int view_size = 64;
    const render_scene scene{ point::zero, you.pos_bub(), view_size, view_size };
    std::multimap<point, formatted_text> overlay_strings;
    color_block_overlay_container color_blocks;
    wr.draw_world( scene, overlay_strings, color_blocks );

    std::vector<Uint32> pixels( static_cast<size_t>( view_size ) * view_size, 0 );
    const SDL_Rect rect{ 0, 0, view_size, view_size };
    REQUIRE( RenderReadPixels( get_sdl_renderer(), &rect, SDL_PIXELFORMAT_ARGB8888,
                               pixels.data(), view_size * 4 ) );
    int lit_pixels = 0;
    for( const Uint32 px : pixels ) {
        if( ( px & 0x00FFFFFF ) != 0 ) {
            lit_pixels++;
        }
    }
    // The daytime sky alone guarantees a mostly-lit frame.
    CHECK( lit_pixels > view_size * view_size / 4 );

    // Picking in first person ray-casts into the world: the result is a
    // sane nearby cell (within the 24-cell pick range of the avatar).
    const point_bub_ms picked = wr.screen_to_map( point( 10, 10 ), point( 32, 32 ),
                                point( view_size, view_size ),
                                you.pos_bub().xy() );
    CHECK( std::abs( picked.x() - you.pos_bub().x() ) <= 25 );
    CHECK( std::abs( picked.y() - you.pos_bub().y() ) <= 25 );

    // Zoom out leaves first person and consumes the step; the next one
    // falls through to the normal zoom path.
    REQUIRE( wr.handle_zoom_out() );
    CHECK_FALSE( wr.handle_zoom_out() );
    uistate.tileset_zoom = saved_zoom;
}

// The overlay/animation snapshot the block_3d backend consumes each frame:
// self-voiding kinds (cursor, zones) drain on take, driver-owned kinds
// (explosion, bullet) persist until their driver voids them.
TEST_CASE( "block_3d_overlay_frame_snapshot", "[tiles][render_3d]" )
{
    software_render_fixture fx;
    if( !fx.available() ) {
        WARN( "dummy SDL video backend unavailable; skipping" );
        return;
    }

    clear_avatar();
    clear_map();
    avatar &you = get_avatar();
    const tripoint_bub_ms origin = you.pos_bub();

    tileset_cache cache;
    GeometryRenderer_Ptr geom = std::make_unique<DefaultGeometryRenderer>();
    cata_tiles tiles( get_sdl_renderer(), geom, cache );

    tiles.init_draw_cursor( origin );
    tiles.init_draw_zones( origin, origin + tripoint{ 1, 1, 0 },
                           tripoint_rel_ms{ 1, 0, 0 } );
    tiles.init_draw_bullet( origin, "animation_bullet_normal" );
    tiles.init_explosion( origin, 2 );

    overlay_frame_snapshot snap;
    tiles.take_overlay_frame( snap );
    REQUIRE( snap.cursors.size() == 1 );
    CHECK( snap.cursors.front() == origin );
    CHECK( snap.zones );
    // The zone rect comes back with the offset applied at the player's z.
    CHECK( snap.zone_start == origin + point( 1, 0 ) );
    CHECK( snap.zone_end == origin + point( 2, 1 ) );
    CHECK( snap.bullet );
    CHECK( snap.bullet_pos == origin );
    CHECK( snap.explosion );
    CHECK( snap.explosion_pos == origin );
    CHECK( snap.explosion_radius == 2 );

    // Second take: cursor and zones self-voided on the first take, while
    // the driver-owned explosion and bullet are still pending.
    tiles.take_overlay_frame( snap );
    CHECK( snap.cursors.empty() );
    CHECK_FALSE( snap.zones );
    CHECK( snap.bullet );
    CHECK( snap.explosion );

    // Driver voids end the driver-owned animations.
    tiles.void_bullet();
    tiles.void_explosion();
    tiles.take_overlay_frame( snap );
    CHECK_FALSE( snap.bullet );
    CHECK_FALSE( snap.explosion );
}

#endif // TILES
