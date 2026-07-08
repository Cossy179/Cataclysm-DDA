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
         * Draw the world viewport.
         * @param dest Top-left corner of the viewport in screen pixels.
         * @param center Map position at the center of the view.
         * @param width Viewport width in pixels.
         * @param height Viewport height in pixels.
         * @param overlay_strings Text the backend wants drawn on top of the
         *        viewport (e.g. item labels); rendered by the UI layer after
         *        this call returns.
         * @param color_blocks Colored highlight overlays (e.g. auto-travel
         *        route previews); rendered by the UI layer after this call
         *        returns.
         */
        virtual void draw_world( const point &dest, const tripoint_bub_ms &center,
                                 int width, int height,
                                 std::multimap<point, formatted_text> &overlay_strings,
                                 color_block_overlay_container &color_blocks ) = 0;
};

/** The backend selected by the WORLD_RENDERER display option. */
world_renderer &get_active_world_renderer();

#endif // TILES

#endif // CATA_SRC_WORLD_RENDERER_H
