#pragma once
#ifndef CATA_SRC_WORLD_RENDERER_H
#define CATA_SRC_WORLD_RENDERER_H

#if defined(TILES)

#include <map>
#include <string>

#include "cata_tiles.h"
#include "coordinates.h"
#include "point.h"

/**
 * Description of what the world viewport should show this frame — the
 * camera parameters shared by every backend.  Phase 2.1 of
 * doc/3D_ROADMAP.md grows this into a full scene description with
 * per-tile draw entries; until then, backends query the map and its
 * caches directly for tile data.
 */
struct render_scene {
    /** Top-left corner of the viewport in screen pixels. */
    point dest;
    /** Map position at the center of the view. */
    tripoint_bub_ms center;
    /** Viewport width in pixels. */
    int width = 0;
    /** Viewport height in pixels. */
    int height = 0;
};

/**
 * Interface for backends that draw the world viewport — everything inside
 * the terrain window: map, creatures, items and effects.  The surrounding
 * UI (curses-rasterized panels, ImGui overlays) composites on top of
 * whatever the active backend produced, so backends can be swapped without
 * touching any menu or panel code.
 *
 * This is the Phase 2 seam from doc/3D_ROADMAP.md: the SDL2 sprite blitter
 * is the default backend, and future renderers (including true-3D ones)
 * implement the same interface.
 */
class world_renderer
{
    public:
        virtual ~world_renderer() = default;

        /** Identifier matching a value of the WORLD_RENDERER option. */
        virtual std::string id() const = 0;

        /**
         * Draw the world viewport described by `scene`.
         * @param overlay_strings Text the backend wants drawn on top of the
         *        viewport (e.g. item labels); rendered by the UI layer after
         *        this call returns.
         * @param color_blocks Colored highlight overlays (e.g. auto-travel
         *        route previews); rendered by the UI layer after this call
         *        returns.
         */
        virtual void draw_world( const render_scene &scene,
                                 std::multimap<point, formatted_text> &overlay_strings,
                                 color_block_overlay_container &color_blocks ) = 0;

        /**
         * Map a terrain-window-relative pixel to the map cell drawn there,
         * on the view-center z-plane — the inverse of this backend's
         * projection, used for mouse picking.  `center` is the cell at the
         * window center; the default implementation is the sprite
         * renderer's ortho/iso conversion.
         */
        virtual point_bub_ms screen_to_map( const point &screen_pos, const point &tile_size,
                                            const point &win_size,
                                            const point_bub_ms &center ) const;

        /**
         * Offer the backend the zoom keypress before the normal tileset
         * rescale runs.  Return true to consume it — the block_3d backend
         * enters its first-person view when zooming in past the maximum
         * tile zoom, and leaves it again on zoom out.  The default ignores
         * the event so standard zoom behavior applies.
         */
        virtual bool handle_zoom_in() {
            return false;
        }
        virtual bool handle_zoom_out() {
            return false;
        }

        /**
         * Offer the backend a view-shift (map panning) keypress.  Return
         * true to consume it — in the block_3d first-person view the shift
         * keys turn the camera (left/right rotate 45 degrees, back turns
         * around) instead of panning the map.
         */
        virtual bool handle_view_shift( const point &/* dir */ ) {
            return false;
        }

        /**
         * Remap a movement keypress delta into the backend's control frame.
         * The default is identity (world-compass movement); the block_3d
         * first-person view rotates it by the camera heading, so "forward"
         * walks the way the player is looking and left/right strafe.
         */
        virtual point remap_move_delta( const point &d ) const {
            return d;
        }
};

/** The backend selected by the WORLD_RENDERER display option. */
world_renderer &get_active_world_renderer();

#endif // TILES

#endif // CATA_SRC_WORLD_RENDERER_H
