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
- ✅ The player's own stair/ladder traversal (`game::vertical_move` in `src/game.cpp`) charges the same +50 penalty (fliers and gliders exempt), so the avatar, NPCs and pathfinding all agree on what stairs cost.
- ✅ The A* pathfinder's stair transition g-score (`src/pathfinding.cpp`) now matches the movement-cost model: a stair step costs 1.5 normal steps instead of 1.0.
- ✅ Monsters pay the same +50 climb penalty in `monster::calc_movecost` (`src/monmove.cpp`), fliers and ramps exempt — every actor now agrees on what vertical movement costs.

### 1.3 Single-z audits

- ✅ `map::decay_fields_and_scent` (`src/map.cpp`) now decays fields on **all** loaded z-levels instead of only the player's, matching the `map::process_fields` pattern — rain and time now affect fire/smoke on floors above and below you.
- ✅ Removed the stale 3D-vision shortcut in `map::spawn_monsters_submap_group` (`src/map.cpp`): monster groups no longer skip the player-line-of-sight check just because they spawn on another z-level, so monsters can't pop into existence in plain view over a ledge.
- ✅ Added the `zlevel()` math function (`src/math_parser_diag.cpp`, documented in `doc/JSON/NPCs.md`) so EOC/dialogue JSON can query the z-level of a location or actor — resolving the TODO in `src/condition.cpp` `f_map_in_city`. Covered by tests in `tests/math_parser_test.cpp`.
- ✅ Removed the obsolete same-z-level restriction in `game::chat` (`src/npctalk.cpp`): with 3D vision, any visible creature in earshot can be talked to, including ones on other z-levels.
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

- ✅ (camera slice) The `render_scene` value type exists (`src/world_renderer.h`) and carries the per-frame camera parameters (viewport destination, map center, dimensions); `world_renderer::draw_world( const render_scene &, … )` is the interface shape all backends implement.
- Open (the big part): grow `render_scene` to carry per-visible-cell draw entries — z-level, terrain/furniture/trap/field/creature entries (sprite or asset IDs, rotation, lighting level, memory status) — a structured form of what `draw_points_cache` plus the 11 `draw_*` layer calls compute per frame. Building the scene stays in `cata_tiles` (it owns visibility, memory, and lighting queries); consuming it moves fully behind the interface. This is a large refactor of `src/cata_tiles.cpp` and should be its own reviewed effort.

### 2.2 Define the renderer interface

- ✅ Implemented: the `world_renderer` interface (`src/world_renderer.h`) abstracts the world-viewport draw call as `draw_world( const render_scene &, … )`. The existing sprite blitter is the default backend (`sdl2_sprite_world_renderer` in `src/sdltiles.cpp`), and a `flat_color` debug backend — drawing the map as flat colored blocks from map data and the visibility cache with no tileset involvement — proves the seam supports genuinely different renderers, selectable at runtime via the new `WORLD_RENDERER` display option (`src/options.cpp`). This meets the phase exit criteria: identical default rendering through the interface, plus a runtime-selectable second backend.
- Open: add initialize/shutdown/resize/capability hooks when the first backend that owns GPU resources (Phase 3) needs them.

### 2.3 Decouple UI composition from world rendering

- ✅ Realized at the seam: the world renderer draws into the terrain-window viewport region of the `display_buffer`, and the overlay text, color-block highlights, curses-raster panels and ImGui composite on top in `cata_cursesport::curses_drawwindow` (`src/sdltiles.cpp`) regardless of which backend produced the viewport contents — verified by the `flat_color` backend, under which every menu, panel and popup renders unchanged.
- This keeps every menu, panel, and popup working identically over a 3D viewport later; a Phase 3 GPU backend rendering to its own target will need the composition step to blit that target into the `display_buffer` at the same point.

**Exit criteria:** tiles build renders pixel-identically (or near-identically) through the new interface; a trivial second backend (e.g. a debug wireframe or flat-color renderer) can be selected at runtime, proving the seam works.

## Phase 3 — First true-3D backend

The first visible payoff: the same grid world, drawn as real projected 3D geometry.

**Status: first milestone implemented** — the `block_3d` backend (select `WORLD_RENDERER = 3D blocks` in display options). Fixed axonometric camera; walls as full-height blocks, floors as slabs, windows/fences/stairs as half-blocks, furniture stacked on top, vehicles as plain gray blocks, creatures as camera-facing billboards; the full visible z-stack renders as real multi-story geometry, lit per tile by the game's lightmap with per-face shading. Implementation: SDL-free projection/mesh math in `src/render_3d.h/.cpp` (unit-tested in `tests/render_3d_test.cpp`), the `RenderTriangles` batch wrapper in `src/sdl_wrappers.cpp` (SDL2/SDL3-proof over `SDL_RenderGeometry`), and `block_3d_world_renderer` in `src/sdltiles.cpp`, which draws one triangle batch per frame in painter's order over the integer depth key `dx + dy + dz` (exact because one z-level is half a tile width tall). A pixel-level smoke test (`tests/block_3d_smoke_test.cpp`) draws through the real software renderer headless.

**Playability update (implemented):**
- ✅ **Map memory, both directions**: out-of-sight tiles render the remembered terrain/furniture/trap in a dim blue-gray memory tint (`render_3d::memory_tint`), classified from the remembered ids' own flags; and the backend *writes* memory for seen tiles with the sprite renderer's exact dirty-bit protocol (`memorize_terrain`/`memorize_clear_decoration`/`memorize_decoration`, then clearing the bits), because map memory is renderer-written in this engine — without this a block_3d player would silently stop accumulating remembered terrain. `prepare_map_memory_region` is called per frame over the culled range. Caveat: memories are stored with subtile 0 / rotation 0, so a later switch to the sprite renderer shows unrotated remembered tiles until re-seen.
- ✅ **Fields** as colored overlay slabs (displayed field entry's color, `display_field`-gated); **items** (uppermost visible item, `sees_some_items`-gated) and **revealed traps** as small diamond markers (`render_3d::emit_marker`); **vehicles** now use the real displayed part color instead of flat gray.
- ✅ **Scrolling combat text** projected with the 3D camera into the UI layer's overlay-text pass (damage numbers float over the 3D scene).

**Textured update (implemented):**
- ✅ **Tileset-textured block tops**: terrain and furniture top faces carry the loaded tileset's actual sprites (season-resolved, first fg sprite), laid flat across the cell and modulated by the full lighting pipeline — light level, time-of-day grading, per-corner ambient occlusion, and the blue-gray memory tint for remembered tiles. Block sides stay flat-shaded color, a deliberate style. Plumbing: `texture::rect()` + `cata_tiles::get_sprite_ref()` expose atlas sheet + source rect; `render_3d::emit_block_top_textured` / `emit_sprite_billboard` emit UV geometry; `RenderTriangles` gained a textured overload.
- ✅ **Monster sprite billboards**: monsters render as upright camera-facing sprites with their real tileset art (aspect-correct), lit and graded; NPCs and the avatar keep colored diamonds (layered character sprites punted).
- ✅ Draw batching stays cheap: within a depth bucket nothing overlaps, so each bucket reorders into one colored batch plus one batch per atlas sheet.

**Interaction update (implemented):**
- ✅ **Mouse picking**: the `world_renderer` seam gained a `screen_to_map` hook — `input_context::get_coordinates` routes terrain-window picking through the active backend, so hover/examine/aim/mouse-travel select the correct cell under the 3D projection (`render_3d::unproject`, inverted on the ground-slab plane at the view-center z, round-trip unit-tested). The sprite backend's default preserves the previous ortho/iso behavior exactly.
- ✅ **Look-around cursor and highlights** render in 3D (cyan billboard / yellow markers).

**Animation update (implemented):**
- ✅ **Combat and UI animations play in 3D.** `cata_tiles::take_overlay_frame` snapshots the deferred animation state (`overlay_frame_snapshot`) for the backend each frame, applying the sprite draw block's exact void policy: line/weather/SCT/zones/cursor/highlight self-void per frame, hit flashes expire by age (`void_hit`), while explosions, custom explosions, bullets and async anims stay owned by their game-side drivers/timeouts and are only copied — the previous `take_overlay_queues` drained driver-owned state and is gone.
- ✅ Rendering (`block_3d_world_renderer::emit_overlay_frame`): **explosions** as an orange center flash plus growing glow rings mirroring the sprite frame's ring geometry; **custom explosions** as per-tile glows in the animation's own color; **bullets** as a bright in-flight marker; **hit flashes** as a red glow + marker on the struck creature's cell; **trajectory lines** as gray markers with an emphasized endpoint (honoring the target-line sees-gate); **zone previews** as translucent blue glow cells over the offset-applied rect; **async anims** (portal storms) as tileset sprite billboards with a pale marker fallback. All entries carry real z and flow through the standard depth buckets, so they occlude correctly. Snapshot semantics are unit-tested in `tests/block_3d_smoke_test.cpp` (self-voiding vs driver-owned kinds).
- Weather drops stay punted: their positions are screen-space cells in non-isometric mode, meaningless under the 3D projection; they void unrendered.

**Milestone punts** (still open in this phase): textured side faces; multitile/rotation/connection sprite variants (base sprite only); NPC/avatar layered character sprites; item and vehicle-part sprites; graffiti; debug overlays and item name-tag async text; color-block highlight overlays (auto-travel preview); colored light and night vision are done (Phase 4) but night-vision *sprite variants* are not; true stair/ramp slopes; weather drop animations (screen-space data in non-iso mode); z-levels above the camera; chunked mesh caching (geometry is re-emitted per frame — fine at current vertex volumes, revisit with caching keyed to submaps if profiling demands).

### 3.1 GPU API choice

The milestone deliberately uses SDL2's triangle API (`SDL_RenderGeometry`) — no new dependency, works with the existing renderer and UI compositing. For the *next* step up (real depth buffer, shaders, textured meshes):

Recommendation: **SDL3 GPU API** (first choice) or **bgfx** (fallback), not raw Vulkan.

- The codebase already has SDL3 awareness (parallel SDL2/SDL3 paths in sound and ImGui backends), and SDL3's GPU API abstracts Vulkan/D3D12/Metal — matching the project's wide platform matrix (Windows, Linux, macOS, Android) with one codebase.
- bgfx is the proven alternative if SDL3 GPU is found lacking; it adds a third-party dependency but has a long track record.
- Raw Vulkan is only justified later if hardware ray tracing (Phase 4) demands it, and even then only inside one backend.

### 3.2 Camera model

Start constrained, then loosen:

1. ✅ **Fixed-angle axonometric camera** matching today's isometric view — same information visible, no gameplay questions raised. Implemented in `src/render_3d.h` (classic 2:1 dimetric, zoom flows through the tile width).
2. **Limited orbit/tilt** (e.g. four 90° rotations, adjustable pitch) once occlusion/cutaway handling works.
3. Free perspective camera as a stretch goal — it raises real gameplay-legibility questions (what does "seen" mean when the camera sees more than the avatar) and must never affect the simulation's own FOV, which stays `fov_3d_z_range`-based.

### 3.3 World geometry

- **Extruded tile meshes / voxels**: generate chunk meshes from map data — full-height blocks for walls, thin slabs for floors, partial shapes derived from existing terrain/furniture flags (windows, half-walls, stairs, ramps). The world is already a voxel grid; this is a direct translation.
- **Chunked mesh caching keyed to submaps** (the map is stored as submaps already), invalidated by the same events that invalidate `draw_points_cache` / level caches today (terrain change, bash, construction).
- **Billboarded sprites for creatures, items, and fields** initially — the entire tileset ecosystem keeps working in 3D from day one, rendered as camera-facing quads at the right cell and height.
- **Cutaway/roof handling**: reuse the existing multi-z visibility logic (`dont_draw_lower_floor`, inter-level visibility bitsets) to decide which levels to draw, replacing the translucent fog overlay with true geometry culling or transparency.

### 3.4 Lighting bootstrap

- ✅ Implemented in the milestone: per-cell visibility (`visibility_cache` → `map::get_visibility`) gates what is drawn, and `map::ambient_light_at` drives a per-tile shading factor (`render_3d::light_factor`) baked into vertex colors — the game's own light simulation drives the visuals, so 3D output matches 2D output semantically. Colored light and night-vision tinting remain open.

### 3.5 Asset pipeline (minimum viable)

- A `tile_config`-style JSON extension (e.g. `model_config.json` in a tileset/asset pack) mapping tile IDs to: block shape, textures per face, and optional mesh reference. Loaded alongside tilesets in `src/tileset_loader.cpp` / `src/mod_tileset.cpp`.
- Everything unmapped falls back to a textured block using the 2D sprite as its top/side texture, so a bare tileset still produces a coherent 3D scene.

**Exit criteria:** the game is playable end-to-end in the 3D backend at the fixed camera angle, with all UI working, at ≥60 FPS on mid-range hardware, selectable via an option, and with the 2D backends untouched.

## Phase 4 — Advanced lighting and high-end graphics

Strictly layered on top of the Phase 3 raster renderer; every step optional and toggleable.

**Status: raster-feasible core implemented** on the `block_3d` backend (the steps below that need a GPU depth buffer or shaders wait for the Phase 3 GPU-API upgrade):

- ✅ **Contact shadows (ambient occlusion)**: top-face corners that walls wrap around darken with classic voxel AO, interpolated across the face via per-vertex colors (`render_3d::corner_occlusion`, `emit_block_shaded`; neighbor sampling in `src/sdltiles.cpp`).
- ✅ **Dynamic sun-direction shading**: the two visible side faces track the real sun azimuth through the day (`render_3d::sun_face_shading` driven by `sun_azimuth_altitude`) — east faces glow in the morning, south faces at midday, flat cool moonlight at night. Underground stays neutral.
- ✅ **Time-of-day color grading**: cool blue nights, warm golden-hour dawn/dusk (`render_3d::time_of_day_grading`/`grade`), applied to terrain and creatures; memory and emissive tiles are exempt by design.
- ✅ **Alpha blending + translucency**: the triangle batch now blends (`RenderTriangles` sets `SDL_BLENDMODE_BLEND`); fields render as translucent overlays.
- ✅ **Emissive light sources**: light-emitting fields (fire) render ungraded and unshaded at full color, keyed off the field's real `light_emitted` value, with a **three-layer soft bloom** of concentric translucent halos.
- ✅ **Directional sun shadows (per-tile)**: outdoor ground tiles march up to three cells toward the sun (8-way snapped from `sunlight_angle`'s shadow vector); a wall on the way casts soft shade, nearer = darker (`render_3d::sun_step`/`sun_shadow_factor`, multiplied into the AO corners). The engine's lightmap has no directional sun shadowing, so this is new visual information. This work also **fixed an azimuth-convention bug**: the engine measures azimuth from south positive-west, so the earlier face shading lit east faces in the evening; `sun_face_shading` now takes the engine's shadow vector directly.
- ✅ **Weather color grading**: overcast darkens with a cool cast scaled by the real sunlight attenuation (`incident_sunlight`/`sun_light_at`), rain cools, snow brightens blue, fog desaturates (`render_3d::weather_grading`, composing multiplicatively with time-of-day grading).
- ✅ **Night-vision tint**: phosphor-green grading when NV goggles are active (`get_vision_modes()[NV_GOGGLES]`, the same bit the sprite renderer keys on).
- ✅ **Colored light**: per-tile accumulated colored light (`level_cache.light_color_cache`) tints lit tiles and sprites using the sprite renderer's exact overlay formula (`render_3d::apply_light_color`), gated on `has_colored_lights`.

**GPU shader lane opened (implemented):**
- ✅ **The block_3d renderer now runs on the SDL3 GPU pipeline** (`USE_SDL3=ON`, the `gpu` render driver over Vulkan/D3D12/Metal): the whole 3D stack — triangle batches, textured tops, sprite billboards, animations — compiles and passes its full test suite under SDL3, and the first programmable-shader stage is live.
- ✅ **FXAA post-process pass**: the finished 3D viewport is copied to a scratch target and drawn back through a custom fragment shader (`data/shaders/scene_fxaa.frag`, classic luma FXAA), smoothing the jagged edges of the flat-shaded block world. Plumbing: a `SCENE_POST` variant added to `cata_shader::variant_pass`, inheriting the existing all-or-nothing probe, renderer-recovery embargo, and `SDL_GPURenderState` bind discipline; the pass in `block_3d_world_renderer::apply_scene_post` cooperates with `scoped_render_target`/quarantine semantics and unbinds before any UI draws. Toggle: `WORLD_POSTFX` display option (SDL3 only, default on). Falls back to the plain raster output on the software/GL renderers, missing shader artifacts, or SDL2 builds — behavior there is unchanged.
- ✅ Verified end-to-end on a headless software Vulkan device (lavapipe): the compiled SPIR-V artifact executes through `SDL_CreateGPURenderState` on the real `gpu` render driver and measurably anti-aliases a staircase edge (0 blended pixels without the shader, >150 with it), and the SDL3 test suite passes with the pass compiled in.

**Depth-buffered GPU scene pass with sun shadow maps (implemented):**
- ✅ **The 3D scene now renders through raw `SDL_GPU` graphics pipelines** on the SDL3 gpu driver (`WORLD_GPU_SCENE` option, default on): a real depth buffer resolves visibility per pixel instead of painter's sorting, and a **true shadow-map pass** draws per-pixel sun shadows — walls cast correctly-shaped shadows on the ground, replacing the 3-cell march approximation (which auto-disables to avoid double-darkening).
- ✅ Architecture: everything stays linear in world space, so the CPU delivers pre-transformed vertices and the shaders are passthrough with **zero uniforms** — clip-space position with fractional view depth (`render_3d::vtx::d`, filled by every emitter), plus interpolable light-space coordinates recovered via the exact projection inverse (`world_from_projected`) and the sun's fitted orthographic transform (`sun_light_space`), all unit-tested pure math. Shadow casters (opaque blocks) are emitted directly in light space (`emit_block_light_space`). `scene_gpu_pass` (`src/render_3d_gpu.*`) owns the pipelines, shadow map (1024², R32F + depth), per-frame vertex uploads, and the scene texture — a renderer texture unwrapped to its `SDL_GPUTexture` (`SDL_PROP_TEXTURE_GPU_TEXTURE_POINTER`), so the 2D pipeline composites the result like any other texture and the FXAA post pass stacks on top. Shaders: `data/shaders/scene_3d.*`, `scene_3d_shadow.*` (4-tap PCF, biased compare).
- ✅ Degrades cleanly: any unavailability (software/GL renderer, missing artifacts, GPU failure) falls back to the CPU triangle batch mid-frame; failures disable the pass until the renderer's GPU device changes, mirroring `cata_shader`'s session discipline. One found-the-hard-way constraint is documented in code: the gpu render driver only submits its command buffer at present time, so resources this pass samples must not depend on same-frame renderer uploads (the flat-color white texture is a pure `SDL_GPU` upload for exactly that reason).
- ✅ Verified end-to-end on headless Vulkan (lavapipe): a standalone probe compiles the real `render_3d` math and loads the shipped SPIR-V artifacts, renders a wall on a ground plane, and asserts the sun-side geometry stays lit while the ground cell along the engine's shadow vector darkens by exactly the shadow strength (180 → 99); depth testing was separately proven to override draw order. All three build flavors pass their suites.

Staged next steps, in order of payoff-per-effort:

1. **Cast shadows (remaining)** — penumbra quality (cascaded/higher-res maps, slope-scaled bias), point-light shadows, and creature billboard shadows; the sun shadow map above covers the dominant case.
2. **Physically-based materials** — extend the asset JSON with roughness/metalness/emissive maps; ship sensible defaults derived from material types (`data/json/materials.json`) so unmodded content benefits. Pairs with tileset-textured faces.
3. **Screen-space effects** — more passes on the now-open `SCENE_POST` lane: SSAO-style edge darkening, full-screen bloom (explosions, portal storms), sharpening/CRT looks. Effects needing scene depth wait for the raw-GPU-pipeline step.
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
