# 3D Roadmap

This document is a long-term engineering roadmap for making Cataclysm: Dark Days Ahead fully three dimensional — completing the 3D simulation the game already has, introducing a true 3D renderer, and eventually supporting advanced lighting up to and including ray tracing. It is a design/planning document only; it does not describe work that has been merged.

- [Vision and guiding principles](#vision-and-guiding-principles)
- [Where the project already is](#where-the-project-already-is)
- [Phase 1 — Finish 3D gameplay](#phase-1--finish-3d-gameplay)
- [Phase 2 — Renderer abstraction layer](#phase-2--renderer-abstraction-layer)
- [Phase 3 — First true-3D backend](#phase-3--first-true-3d-backend)
- [Phase 4 — Advanced lighting and high-end graphics](#phase-4--advanced-lighting-and-high-end-graphics)
- [Phase 5 — Full 3D presentation polish](#phase-5--full-3d-presentation-polish)
- [Cross-cutting concerns](#cross-cutting-concerns)
- [Risks and open questions](#risks-and-open-questions)

## Vision and guiding principles

Cataclysm: DDA is, at heart, a turn-based, grid-based survival *simulation*. Nothing in this roadmap changes that. "Going 3D" means two separable things:

1. **Simulation completeness** — the game logic already models a 3D world (21 vertical levels, 3D field of view, 3D sound). The remaining gaps are localized (chiefly monster AI). Phase 1 closes them, and benefits every player including curses users.
2. **Presentation** — replacing the flat sprite view with a real 3D-rendered world, in stages, from a fixed-camera "2.5D" view up to a modern lit renderer with optional ray tracing.

Guiding principles for all phases:

- **The simulation never depends on the renderer.** All 3D graphics work happens behind a renderer interface. Headless builds, the curses build, and the existing SDL tiles build keep working throughout, exactly as tiles-vs-curses coexist today.
- **Every phase ships standalone value.** No multi-year branch. Each phase is mergeable and useful on its own; a phase can be paused indefinitely without stranding the codebase.
- **3D rendering is opt-in.** The project runs on very low-end hardware and that must remain true. The 2D sprite renderer remains the default for the foreseeable future.
- **Honest scoping.** "AAA graphics" and hardware ray tracing are a long-horizon aspiration for a volunteer-driven C++ project. The later phases are deliberately sketched at lower resolution than the earlier ones; they should be re-planned when the earlier phases are real. The phasing exists precisely so the project gets value even if the last phases never happen.
- **The modding ecosystem carries over.** Tilesets, mods, and JSON-driven content are the lifeblood of the project. 3D asset support must be an extension of the existing JSON pipeline, not a replacement.

## Where the project already is

Contributors picking up this roadmap should know what **not** to rebuild. The engine is much further along than "a 2D game" suggests.

### The game logic is already substantially 3D — and always on

- Coordinates are three dimensional throughout: `tripoint` (`src/point.h`) with typed wrappers in `src/coordinates.h` (`tripoint_bub_ms`, `tripoint_abs_omt`, …).
- The world spans 21 z-levels: `OVERMAP_DEPTH = 10`, `OVERMAP_HEIGHT = 10`, `OVERMAP_LAYERS = 21` (`src/map_scale_constants.h`).
- The reality bubble loads **all** z-layers: the default `map` constructor enables z-levels unconditionally (`src/map.h`, `map::map` in `src/map.cpp`), with per-z-level caches (`level_cache`, pathfinding caches) in `src/map.h`.
- 3D field of view is mandatory, not optional: `constexpr int fov_3d_z_range = 10;` (`src/game_constants.h`). The old `FOV_3D_Z_RANGE` option was removed (see the removed-options migration list in `src/options.cpp`). Ledge visibility propagates downward through open air (`map::seen_cache_process_ledges` in `src/lightmap.cpp`), and sunlight propagates down through z-levels (`map::build_sunlight_cache`).
- Sound is 3D: distance, clustering, and centroids all account for z (`src/sounds.cpp`).
- Vertical movement is rich: stairs, ladders, climbing aids, wall-clinging, flying, falling, ledges, and vehicle ramps (`game::vertical_move` and the `climb_down*` family in `src/game.cpp`; `map::valid_move` in `src/map.cpp`; `TFLAG_RAMP_UP`/`TFLAG_RAMP_DOWN` handling in `src/vehicle_move.cpp`).

### The renderer is a 2D sprite blitter with a mature pseudo-3D layer

- Rendering uses the SDL2 2D renderer exclusively — sprite blits via `SDL_RenderCopyEx` (wrapped in `src/sdl_wrappers.cpp`) and filled primitives via the `GeometryRenderer` abstraction (`src/sdl_geometry.cpp`). There is no OpenGL/Vulkan/Metal code of the game's own; everything composites into an offscreen `display_buffer` texture in `src/sdltiles.cpp`.
- The world draw pipeline is `cata_tiles::draw()` (`src/cata_tiles.cpp`): an 11-layer sequence (`draw_terrain`, `draw_furniture`, `draw_trap`, …) funneling through `draw_from_id_string_internal`.
- **Isometric projection already exists** as a first-class tileset feature (the `"iso"` flag, loaded in `src/tileset_loader.cpp`), with dedicated coordinate math (`player_to_screen` in `src/cata_tiles.cpp`) and an isometric pixel-minimap mode.
- **Multi-z-level rendering already exists**: `cata_tiles::draw()` renders a stack of z-levels bottom-up (up to `fov_3d_z_range` below the player), applying a per-level vertical pixel offset (`zlevel_height` from the tileset, accumulated in `height_3d`) and a translucent depth fog per level (`draw_zlevel_overlay`). Visible tile positions per level are cached in `map::draw_points_cache`.
- UI text and panels are curses windows rasterized to the SDL surface (`cata_cursesport::curses_drawwindow` in `src/sdltiles.cpp`), plus Dear ImGui rendered through `imgui_impl_sdlrenderer2/3` (`src/cata_imgui.cpp`). Redraws are orchestrated by the `ui_adaptor` invalidation system (`src/ui_manager.cpp`), with `game::draw()` in `src/game.cpp` as the world/panel entry point.

In short: the engine already computes a full 3D visible scene every frame — it just flattens it into stacked 2D sprites at the last moment. That is an excellent starting position for a true 3D renderer.

## Phase 1 — Finish 3D gameplay

Pure game-logic work, no graphics. Benefits every build including curses. Each item is independently mergeable.

**Status:** items marked ✅ are implemented on this branch; unmarked items are open.

### 1.1 Real multi-z monster pathfinding

The single biggest remaining gap. Monster movement still contains a legacy "stair teleport": monsters at aligned stairs are teleported between z-levels rather than pathing (`src/monmove.cpp`, marked with `// TODO: Remove z-level stair bullshit teleport after aligning all stairs`).

- Extend monster A* to treat z-transitions (stairs, ladders, ramps, open air for fliers, climbable terrain for climbers) as ordinary graph edges, using the existing per-z `pathfinding_caches` in `src/map.h`.
- Delete the stair-teleport path once multi-z A* handles the same cases.
- Audit stair alignment in mapgen data (the TODO's precondition) and add a test that every `GOES_UP` terrain has a reachable `GOES_DOWN` counterpart above it in generated buildings.

### 1.2 Z-aware movement costs

- ✅ Implement the stair/climb movement penalty flagged by `// TODO: Penalize for using stairs` (`map::combined_movecost` in `src/map.cpp`): non-flying movement between z-levels now costs an extra 50 move points (half a turn) unless taken via a gradual ramp. Covered by the `stairs_cost_more_than_flat_movement` test in `tests/move_cost_test.cpp`.
- Make vertical movement cost visible to pathfinding so AI stops treating stairs as free (the A* in `src/pathfinding.cpp` still uses its own small stair g-score bonus).

### 1.3 Single-z audits

- ✅ `map::decay_fields_and_scent` (`src/map.cpp`) now decays fields on **all** loaded z-levels instead of only the player's, matching the `map::process_fields` pattern — rain and time now affect fire/smoke on floors above and below you.
- ✅ Removed the stale 3D-vision shortcut in `map::spawn_monsters_submap_group` (`src/map.cpp`): monster groups no longer skip the player-line-of-sight check just because they spawn on another z-level, so monsters can't pop into existence in plain view over a ledge.
- ✅ Added the `z_level()` math function (`src/math_parser_diag.cpp`, documented in `doc/JSON/NPCs.md`) so EOC/dialogue JSON can query the z-level of a location or actor — resolving the TODO in `src/condition.cpp` `f_map_in_city`. Covered by tests in `tests/math_parser_test.cpp`.
- The remaining single-z helper flagged `// TODO: Support z-levels` — `map::build_obstacle_cache` in `src/map.cpp`, the 2D obstacle grid used by shrapnel propagation in `src/explosion.cpp` — is still open; making it 3D belongs with vertical explosion propagation work.

### 1.4 NPC and faction parity

- NPCs should use the same multi-z pathfinding as monsters (follow the player up stairs, flee downward, take elevated sniping positions).
- Faction camp and NPC activity logic should be audited for implicit "same z-level" assumptions.

### 1.5 Vertical world content

- More multi-story mapgen making real use of verticality: balconies, atriums, collapsed floors, rooftop encounters, basements connected to sewers.
- These are JSON-side changes that become far more valuable once 1.1 lands, because enemies can then actually use the geometry.

**Exit criteria:** a monster with no special flags can chase the player from a basement to a rooftop through normal stairs with no teleports; CI has tests covering multi-z pathfinding; the stair-teleport code is deleted.

## Phase 2 — Renderer abstraction layer

Refactoring only — zero visual change. This phase creates the seam that lets a 3D backend exist at all, and is the highest-leverage engineering investment in this roadmap.

### 2.1 Extract a scene description

Today `cata_tiles::draw()` both *decides what is visible* and *draws it*. Split those:

- Define a `render_scene` (working name) value type: for each visible map cell, its z-level, terrain/furniture/trap/field/creature entries (sprite or asset IDs, rotation, lighting level, memory status), plus camera parameters (center, zoom, projection). This is essentially a structured form of what `draw_points_cache` plus the 11 `draw_*` layer calls already compute per frame.
- Building the scene stays in `cata_tiles` (it owns visibility, memory, and lighting queries); consuming it moves behind an interface.

### 2.2 Define the renderer interface

- A `world_renderer` interface with (at minimum): initialize/shutdown, `draw(const render_scene &)`, resize, and capability queries.
- The existing SDL2 sprite blitter becomes the first implementation (`sdl2_sprite_renderer`), wrapping the current `SDL_RenderCopyEx` / `GeometryRenderer` path with no behavior change.
- Renderer selection joins the existing `RENDERER` option machinery in `src/options.cpp`.

### 2.3 Decouple UI composition from world rendering

- The `ui_adaptor` system (`src/ui_manager.cpp`) and `game::draw()` already separate panel drawing from terrain drawing. Formalize it: the world renderer draws into a viewport (today: the `display_buffer` region), and curses-raster panels plus ImGui composite on top, regardless of which world renderer produced the viewport contents.
- This keeps every menu, panel, and popup working identically over a 3D viewport later.

**Exit criteria:** tiles build renders pixel-identically (or near-identically) through the new interface; a trivial second backend (e.g. a debug wireframe or flat-color renderer) can be selected at runtime, proving the seam works.

## Phase 3 — First true-3D backend

The first visible payoff: the same grid world, drawn as real 3D geometry with a GPU API.

### 3.1 GPU API choice

Recommendation: **SDL3 GPU API** (first choice) or **bgfx** (fallback), not raw Vulkan.

- The codebase already has SDL3 awareness (parallel SDL2/SDL3 paths in sound and ImGui backends), and SDL3's GPU API abstracts Vulkan/D3D12/Metal — matching the project's wide platform matrix (Windows, Linux, macOS, Android) with one codebase.
- bgfx is the proven alternative if SDL3 GPU is found lacking; it adds a third-party dependency but has a long track record.
- Raw Vulkan is only justified later if hardware ray tracing (Phase 4) demands it, and even then only inside one backend.

### 3.2 Camera model

Start constrained, then loosen:

1. **Fixed-angle axonometric camera** matching today's isometric view — same information visible, no gameplay questions raised. Reuses the existing iso projection conventions and `zlevel_height` semantics.
2. **Limited orbit/tilt** (e.g. four 90° rotations, adjustable pitch) once occlusion/cutaway handling works.
3. Free perspective camera as a stretch goal — it raises real gameplay-legibility questions (what does "seen" mean when the camera sees more than the avatar) and must never affect the simulation's own FOV, which stays `fov_3d_z_range`-based.

### 3.3 World geometry

- **Extruded tile meshes / voxels**: generate chunk meshes from map data — full-height blocks for walls, thin slabs for floors, partial shapes derived from existing terrain/furniture flags (windows, half-walls, stairs, ramps). The world is already a voxel grid; this is a direct translation.
- **Chunked mesh caching keyed to submaps** (the map is stored as submaps already), invalidated by the same events that invalidate `draw_points_cache` / level caches today (terrain change, bash, construction).
- **Billboarded sprites for creatures, items, and fields** initially — the entire tileset ecosystem keeps working in 3D from day one, rendered as camera-facing quads at the right cell and height.
- **Cutaway/roof handling**: reuse the existing multi-z visibility logic (`dont_draw_lower_floor`, inter-level visibility bitsets) to decide which levels to draw, replacing the translucent fog overlay with true geometry culling or transparency.

### 3.4 Lighting bootstrap

- Feed the existing per-cell lighting/visibility results (the `level_cache` lightmap, `lit_level` per cell) into vertex colors or a per-chunk light texture. No new lighting model yet — the game's own light simulation drives the visuals, so 3D output matches 2D output semantically.

### 3.5 Asset pipeline (minimum viable)

- A `tile_config`-style JSON extension (e.g. `model_config.json` in a tileset/asset pack) mapping tile IDs to: block shape, textures per face, and optional mesh reference. Loaded alongside tilesets in `src/tileset_loader.cpp` / `src/mod_tileset.cpp`.
- Everything unmapped falls back to a textured block using the 2D sprite as its top/side texture, so a bare tileset still produces a coherent 3D scene.

**Exit criteria:** the game is playable end-to-end in the 3D backend at the fixed camera angle, with all UI working, at ≥60 FPS on mid-range hardware, selectable via an option, and with the 2D backends untouched.

## Phase 4 — Advanced lighting and high-end graphics

Strictly layered on top of the Phase 3 raster renderer; every step optional and toggleable.

Staged in order of payoff-per-effort:

1. **Dynamic shadows** — shadow maps for the sun/moon and strong point lights (fires, vehicle headlights). Light sources already exist as simulation data.
2. **Physically-based materials** — extend the asset JSON with roughness/metalness/emissive maps; ship sensible defaults derived from material types (`data/json/materials.json`) so unmodded content benefits.
3. **Screen-space effects** — SSAO, bloom (emissive fires, explosions, portal storms), color grading tied to weather and time of day.
4. **Global illumination** — start with baked/irradiance approximations per chunk; the fully static terrain between bashes makes caching viable.
5. **Ray tracing (optional high-end path)** — two candidate routes, to be decided when we get there:
   - **Software voxel ray marching** (à la Teardown): a natural fit since the world is literally a voxel grid; runs on any GPU with compute shaders; likely the pragmatic choice.
   - **Hardware RT** (Vulkan RT / DXR via the chosen GPU API): higher fidelity, much higher implementation and maintenance cost; only worth it if a dedicated contributor owns it.

**Exit criteria per step:** each effect ships behind its own graphics option, defaults off or auto-detected, and the renderer degrades gracefully to the previous step's output on unsupported hardware.

## Phase 5 — Full 3D presentation polish

Deliberately sketched at low resolution — re-plan when Phase 3–4 are real:

- 3D character/monster models or high-quality multi-angle billboards, with the overlay/equipment system (worn items, mutations) mapped onto them.
- An animation layer for movement/attacks — presentation-only interpolation between turns; the simulation stays discrete and turn-based.
- Volumetric weather and particle effects (rain, snow, smoke fields, portal storms) driven by existing field/weather data.
- Camera polish: smooth follow, look-around, photo mode.

## Cross-cutting concerns

- **Performance budget**: the game is CPU-simulation-heavy; the renderer must not steal the sim's thread time. Scene building stays on the game thread (it reads game state); GPU submission should be cheap and ideally offloadable. Target: 3D backend adds no measurable slowdown to turn processing.
- **Platform floor**: curses and the SDL2 sprite renderer remain fully supported. 3D requires a GPU-API-capable device but must run acceptably on integrated graphics; ray tracing is enthusiast-only.
- **Testing and CI**: all rendering stays behind the Phase 2 interface, so headless tests and CI are unaffected. Add a scene-description snapshot test (assert what *would* be drawn) to lock in the 2D/3D semantic parity, and keep `doc/c++/TESTING.md` conventions.
- **Contribution strategy**: Phases 1–2 are upstream-friendly incremental PRs. Phase 3+ should live behind a build flag (like tiles vs curses today) until stable. Each phase needs a tracking issue broken into reviewable PRs; graphics work especially benefits from an owning maintainer per backend.
- **Documentation**: extend `doc/TILESET.md` with the 3D asset JSON when Phase 3.5 lands; keep this roadmap updated as phases complete or get re-scoped.

## Risks and open questions

- **Engine-rewrite temptation.** The biggest historical failure mode for "make it 3D" efforts. Mitigation: the phase structure — no phase requires the next, and no phase forks the simulation.
- **Dual-renderer maintenance burden.** Every world-drawing feature (new field type, new overlay) potentially needs 2D and 3D handling. Mitigation: the scene description is the single source of truth; backends consume it generically, and the block-fallback rule (3.5) means unmapped content still renders.
- **Tileset ecosystem compatibility.** Tileset authors must not be forced to make 3D assets. Mitigation: billboards + block fallback make every existing tileset usable in 3D unchanged.
- **Gameplay legibility with a free camera.** Seeing around corners the avatar can't see changes the feel of the game. Open question for Phase 3.2 step 3; the conservative answer (camera shows only what the avatar's FOV knows, using the existing map-memory rendering) is the default.
- **SDL3 migration timing.** Phase 3.1's first-choice API assumes the project's SDL3 transition continues; if it stalls, bgfx on SDL2 is the fallback.
- **Volunteer bandwidth.** Phases 4–5 only happen with dedicated owners. The roadmap is valuable even if the project permanently stops after Phase 2 or 3 — Phase 1 alone is a major gameplay improvement.
