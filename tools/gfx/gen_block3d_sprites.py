#!/usr/bin/env python3
"""Generate the built-in entity sprite atlas for the block_3d 3D renderer.

The source tree ships only ASCIITileset (10px glyphs), so the 3D view has no
real character/monster/item art and everything falls back to diamond markers.
This produces a compact, self-contained pixel-art atlas the block_3d backend
loads at runtime (gfx/Block3D/entities.png) and billboards in place of the
diamonds, keyed by a fixed cell layout mirrored in src/sdltiles.cpp.

Style: 64px cells, 4px transparent margin (so linear atlas taps never bleed a
neighbour), bold near-black outline, flat fills with a single darker shade on
the lower-right, feet near the cell bottom (the billboard's foot anchor).

Run: python3 tools/gfx/gen_block3d_sprites.py
"""

from PIL import Image, ImageDraw

CELL = 64
COLS = 8
ROWS = 6
OUTLINE = (24, 22, 28, 255)

# Cell layout — index = row * COLS + col. Must match entity_art in sdltiles.cpp.
LAYOUT = [
    # row 0: humanoids
    "player", "npc", "zombie", "human", "skeleton", "robot", "child", "hulk",
    # row 1: creatures
    "mammal", "insect", "spider", "bird", "reptile", "fish", "slime", "fungus",
    # row 2: items A
    "gun", "melee", "ammo", "food", "drink", "clothing", "tool", "book",
    # row 3: items B + misc
    "container", "chem", "electronic", "material", "generic", "corpse",
    "plant", "creature",
    # row 4: more creatures
    "bear", "rat", "nether", "feral", "cow", "frog", "bat", "worm",
    # row 5: female characters, zombie variants, small animals
    "player_f", "npc_f", "zombie_spitter", "zombie_shocker", "zombie_soldier",
    "squirrel", "moose", "crow",
]


def shade(c, f):
    return tuple(int(x * f) for x in c[:3]) + (c[3] if len(c) > 4 else 255,)


class Cell:
    """A 64x64 draw surface with helpers that auto-add outline + shading."""

    def __init__(self):
        self.img = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def ellipse(self, box, color, outline=True):
        self.d.ellipse(box, fill=color, outline=OUTLINE if outline else None)
        # lower-right shading crescent
        x0, y0, x1, y1 = box
        cx = (x0 + x1) / 2
        self.d.pieslice(box, 20, 160, fill=shade(color, 0.72))
        self.d.ellipse(box, outline=OUTLINE if outline else None)

    def solid_ellipse(self, box, color, outline=True):
        self.d.ellipse(box, fill=color, outline=OUTLINE if outline else None)

    def rrect(self, box, color, r=6, outline=True):
        self.d.rounded_rectangle(box, radius=r, fill=color,
                                 outline=OUTLINE if outline else None)
        x0, y0, x1, y1 = box
        # right-third shade
        self.d.rounded_rectangle((x0 + (x1 - x0) * 0.55, y0, x1, y1), radius=r,
                                 fill=shade(color, 0.78))
        self.d.rounded_rectangle(box, radius=r, outline=OUTLINE if outline else None)

    def poly(self, pts, color):
        self.d.polygon(pts, fill=color, outline=OUTLINE)

    def line(self, pts, color, w=3):
        self.d.line(pts, fill=color, width=w)


# ---- palettes -------------------------------------------------------------
SKIN = (233, 187, 148, 255)
SKIN2 = (150, 110, 92, 255)
HAIR = (74, 52, 40, 255)
SHIRT = (70, 120, 175, 255)
PANTS = (60, 66, 82, 255)
NPC_SHIRT = (150, 90, 70, 255)
ZOMBIE_SKIN = (128, 156, 108, 255)
ZOMBIE_CLOTH = (96, 92, 84, 255)
BONE = (232, 228, 210, 255)
METAL = (150, 158, 170, 255)
METAL_D = (96, 104, 120, 255)


def humanoid(c, skin, shirt, pants, hair=HAIR, hunch=0, glow_eyes=False,
             long_hair=False, eye=None, helmet=None):
    """A simple front-facing biped: head, torso, arms, legs; feet ~y=60."""
    cx = 32
    top = 10 + hunch
    # long hair falls behind the shoulders
    if long_hair:
        c.rrect((22, top + 6, 27, top + 26), hair, r=2)
        c.rrect((37, top + 6, 42, top + 26), hair, r=2)
    # legs
    c.rrect((25, 44, 31, 60), pants, r=3)
    c.rrect((33, 44, 39, 60), pants, r=3)
    # torso
    c.rrect((23, 26 + hunch, 41, 46 + hunch), shirt, r=5)
    # arms
    c.rrect((18, 27 + hunch, 24, 45 + hunch), shirt, r=3)
    c.rrect((40, 27 + hunch, 46, 45 + hunch), shirt, r=3)
    # hands
    c.solid_ellipse((17, 42 + hunch, 24, 49 + hunch), skin)
    c.solid_ellipse((40, 42 + hunch, 47, 49 + hunch), skin)
    # head
    c.solid_ellipse((24, top, 40, top + 17), skin)
    # hair cap
    c.d.pieslice((24, top, 40, top + 17), 180, 360, fill=hair)
    c.d.ellipse((24, top, 40, top + 17), outline=OUTLINE)
    # face shade
    c.d.pieslice((24, top, 40, top + 17), 20, 130, fill=shade(skin, 0.82))
    if helmet:
        c.d.pieslice((23, top - 2, 41, top + 16), 180, 360, fill=helmet)
        c.d.arc((23, top - 2, 41, top + 16), 180, 360, fill=OUTLINE)
    # eyes
    if eye is None:
        eye = (40, 210, 90, 255) if glow_eyes else (40, 34, 40, 255)
    c.d.ellipse((28, top + 8, 30, top + 10), fill=eye)
    c.d.ellipse((34, top + 8, 36, top + 10), fill=eye)


def draw_player(c):
    humanoid(c, SKIN, SHIRT, PANTS)


def draw_npc(c):
    humanoid(c, SKIN, NPC_SHIRT, (74, 70, 60, 255), hair=(40, 34, 30, 255))


def draw_child(c):
    cx = 32
    c.rrect((28, 46, 32, 58), PANTS, r=3)
    c.rrect((33, 46, 37, 58), PANTS, r=3)
    c.rrect((26, 32, 39, 48), (110, 160, 90, 255), r=5)
    c.solid_ellipse((26, 18, 39, 31), SKIN)
    c.d.pieslice((26, 18, 39, 31), 180, 360, fill=HAIR)
    c.d.ellipse((26, 18, 39, 31), outline=OUTLINE)
    c.d.ellipse((29, 25, 31, 27), fill=(40, 34, 40, 255))
    c.d.ellipse((34, 25, 36, 27), fill=(40, 34, 40, 255))


def draw_zombie(c):
    humanoid(c, ZOMBIE_SKIN, ZOMBIE_CLOTH, (70, 66, 60, 255),
             hair=(60, 70, 50, 255), hunch=3, glow_eyes=False)
    # blood / torn detail
    c.d.line((30, 32, 34, 40), fill=(120, 40, 40, 255), width=2)
    c.d.ellipse((27, 20, 29, 22), fill=(150, 40, 40, 255))


def draw_human(c):
    # armed human enemy: darker clothes, holding a bar
    humanoid(c, SKIN, (90, 96, 104, 255), (54, 58, 66, 255),
             hair=(30, 28, 26, 255))
    c.line((46, 24, 52, 46), (150, 150, 158, 255), w=3)


def draw_skeleton(c):
    cx = 32
    # ribcage
    c.rrect((26, 26, 38, 44), BONE, r=4)
    for y in range(29, 43, 4):
        c.d.line((27, y, 37, y), fill=shade(BONE, 0.6), width=1)
    # limbs (bones)
    c.line((24, 28, 20, 44), BONE, w=3)
    c.line((40, 28, 44, 44), BONE, w=3)
    c.line((28, 44, 27, 60), BONE, w=3)
    c.line((36, 44, 37, 60), BONE, w=3)
    # skull
    c.solid_ellipse((25, 12, 39, 27), BONE)
    c.d.ellipse((28, 18, 31, 22), fill=(30, 30, 34, 255))
    c.d.ellipse((33, 18, 36, 22), fill=(30, 30, 34, 255))
    c.d.line((30, 24, 34, 24), fill=(30, 30, 34, 255), width=1)


def draw_robot(c):
    # boxy chassis on treads, single optical sensor
    c.rrect((22, 46, 44, 58), METAL_D, r=3)
    c.d.ellipse((24, 49, 30, 55), fill=(40, 44, 52, 255))
    c.d.ellipse((36, 49, 42, 55), fill=(40, 44, 52, 255))
    c.rrect((24, 28, 42, 47), METAL, r=4)
    c.rrect((28, 16, 38, 29), METAL, r=3)
    c.d.ellipse((30, 19, 36, 25), fill=(220, 70, 60, 255))
    c.d.ellipse((31, 20, 34, 23), fill=(255, 190, 180, 255))
    c.line((33, 10, 33, 16), (150, 158, 170, 255), w=2)
    c.d.ellipse((31, 8, 35, 12), fill=(230, 120, 60, 255))


def draw_hulk(c):
    # oversized brute
    humanoid(c, (150, 120, 110, 255), (120, 70, 70, 255), (60, 50, 48, 255),
             hair=(40, 34, 30, 255))
    # bulk up arms
    c.rrect((12, 26, 22, 50), (150, 120, 110, 255), r=5)
    c.rrect((42, 26, 52, 50), (150, 120, 110, 255), r=5)
    c.rrect((20, 24, 44, 48), (120, 70, 70, 255), r=6)
    c.solid_ellipse((25, 12, 41, 28), (150, 120, 110, 255))
    c.d.ellipse((29, 19, 31, 22), fill=(200, 60, 50, 255))
    c.d.ellipse((35, 19, 37, 22), fill=(200, 60, 50, 255))


def draw_mammal(c):
    # side-view quadruped (dog/wolf)
    body = (140, 116, 92, 255)
    c.rrect((16, 34, 46, 48), body, r=7)
    # legs
    for x in (19, 27, 35, 42):
        c.rrect((x, 46, x + 4, 58), shade(body, 0.8), r=2)
    # head
    c.solid_ellipse((40, 28, 54, 42), body)
    c.poly([(52, 30), (58, 34), (50, 38)], body)  # snout
    c.poly([(42, 26), (45, 20), (47, 30)], body)  # ear
    c.d.ellipse((47, 32, 50, 35), fill=(30, 26, 24, 255))
    # tail
    c.line((16, 36, 8, 30), body, w=4)


def draw_insect(c):
    body = (110, 92, 70, 255)
    c.solid_ellipse((26, 34, 42, 54), body)      # abdomen
    c.solid_ellipse((26, 24, 40, 38), body)      # thorax
    c.solid_ellipse((28, 14, 38, 26), body)      # head
    # legs
    for y in (30, 38, 46):
        c.line((26, y, 12, y - 4), OUTLINE, w=2)
        c.line((40, y, 54, y - 4), OUTLINE, w=2)
    # antennae
    c.line((30, 15, 24, 6), OUTLINE, w=2)
    c.line((36, 15, 42, 6), OUTLINE, w=2)
    c.d.ellipse((29, 18, 32, 21), fill=(210, 60, 50, 255))
    c.d.ellipse((34, 18, 37, 21), fill=(210, 60, 50, 255))


def draw_spider(c):
    body = (60, 56, 66, 255)
    c.solid_ellipse((24, 32, 44, 52), body)
    c.solid_ellipse((28, 24, 40, 36), body)
    for i, y in enumerate((30, 36, 42, 48)):
        c.line((28, 38, 8, y), OUTLINE, w=2)
        c.line((40, 38, 56, y), OUTLINE, w=2)
    c.d.ellipse((30, 27, 33, 30), fill=(220, 60, 60, 255))
    c.d.ellipse((35, 27, 38, 30), fill=(220, 60, 60, 255))


def draw_bird(c):
    body = (120, 130, 150, 255)
    c.solid_ellipse((24, 30, 44, 50), body)
    c.solid_ellipse((36, 20, 48, 32), body)      # head
    c.poly([(46, 24), (56, 26), (46, 29)], (210, 170, 70, 255))  # beak
    c.poly([(24, 34), (10, 30), (24, 44)], shade(body, 0.8))     # wing
    c.line((30, 50, 28, 60), (210, 170, 70, 255), w=2)
    c.line((36, 50, 38, 60), (210, 170, 70, 255), w=2)
    c.d.ellipse((41, 24, 44, 27), fill=(30, 26, 24, 255))


def draw_reptile(c):
    body = (96, 140, 92, 255)
    # serpentine S body
    c.line((14, 50, 26, 42), body, w=8)
    c.line((26, 42, 40, 48), body, w=8)
    c.line((40, 48, 50, 36), body, w=8)
    c.solid_ellipse((44, 26, 58, 40), body)      # head
    c.d.ellipse((52, 30, 55, 33), fill=(230, 200, 60, 255))
    c.line((57, 34, 62, 33), (200, 60, 60, 255), w=1)  # tongue


def draw_fish(c):
    body = (90, 150, 175, 255)
    c.solid_ellipse((18, 30, 46, 46), body)
    c.poly([(44, 30), (56, 24), (56, 52), (44, 46)], shade(body, 0.85))  # tail
    c.poly([(28, 30), (34, 20), (40, 30)], shade(body, 0.8))  # dorsal fin
    c.d.ellipse((22, 34, 27, 39), fill=(240, 240, 245, 255))
    c.d.ellipse((23, 35, 26, 38), fill=(30, 26, 24, 255))


def draw_slime(c):
    body = (110, 200, 140, 220)
    c.d.pieslice((16, 26, 48, 58), 180, 360, fill=body, outline=OUTLINE)
    c.d.rectangle((16, 42, 48, 52), fill=body)
    c.d.ellipse((16, 46, 48, 58), fill=body, outline=OUTLINE)
    c.d.ellipse((22, 44, 42, 56), outline=None)
    c.d.ellipse((25, 34, 31, 41), fill=(30, 40, 34, 255))
    c.d.ellipse((34, 34, 40, 41), fill=(30, 40, 34, 255))
    c.d.ellipse((26, 30, 30, 34), fill=(210, 245, 220, 255))  # highlight


def draw_fungus(c):
    stalk = (222, 210, 190, 255)
    cap = (180, 90, 96, 255)
    c.rrect((29, 36, 37, 58), stalk, r=3)
    c.d.pieslice((16, 20, 48, 46), 180, 360, fill=cap, outline=OUTLINE)
    for x in (24, 32, 40):
        c.d.ellipse((x - 2, 26, x + 2, 30), fill=(230, 220, 210, 255))


# ---- items ----------------------------------------------------------------
def draw_gun(c):
    metal = (70, 74, 82, 255)
    grip = (60, 46, 40, 255)
    c.rrect((14, 30, 50, 38), metal, r=2)     # body/barrel
    c.rrect((44, 26, 52, 34), metal, r=2)      # rear
    c.poly([(22, 38), (30, 38), (26, 50)], grip)  # grip
    c.rrect((30, 38, 34, 46), metal, r=1)      # trigger guard
    c.d.rectangle((16, 32, 46, 34), fill=shade(metal, 1.4))


def draw_melee(c):
    blade = (200, 206, 216, 255)
    hilt = (120, 80, 46, 255)
    c.poly([(40, 12), (46, 16), (26, 50), (22, 46)], blade)  # blade
    c.rrect((20, 46, 30, 52), (150, 120, 60, 255), r=2)       # guard
    c.rrect((16, 50, 26, 60), hilt, r=2)                       # handle
    c.line((41, 14, 27, 46), shade(blade, 1.15), w=1)


def draw_ammo(c):
    brass = (200, 160, 70, 255)
    tip = (150, 120, 110, 255)
    for i, x in enumerate((20, 32, 44)):
        c.rrect((x - 5, 30, x + 5, 52), brass, r=2)
        c.poly([(x - 5, 30), (x + 5, 30), (x, 22)], tip)


def draw_food(c):
    # apple
    body = (200, 70, 60, 255)
    c.solid_ellipse((20, 26, 44, 54), body)
    c.d.pieslice((20, 26, 44, 54), 20, 150, fill=shade(body, 0.75))
    c.d.ellipse((20, 26, 44, 54), outline=OUTLINE)
    c.line((32, 26, 34, 16), (90, 60, 40, 255), w=2)          # stem
    c.poly([(34, 20), (44, 16), (38, 24)], (90, 150, 70, 255))  # leaf
    c.d.ellipse((26, 32, 31, 38), fill=(240, 200, 190, 120))  # highlight


def draw_drink(c):
    glass = (120, 180, 210, 220)
    c.rrect((24, 20, 40, 56), (150, 158, 170, 255), r=3)      # bottle
    c.rrect((28, 12, 36, 22), (150, 158, 170, 255), r=2)      # neck
    c.rrect((26, 30, 38, 52), glass, r=2)                     # liquid
    c.d.rectangle((29, 24, 31, 50), fill=(210, 235, 245, 120))


def draw_clothing(c):
    cloth = (90, 130, 180, 255)
    c.poly([(20, 26), (28, 20), (36, 20), (44, 26), (48, 34),
            (42, 38), (42, 54), (22, 54), (22, 38), (16, 34)], cloth)
    c.d.line((32, 21, 32, 30), fill=shade(cloth, 0.7), width=1)  # collar
    c.d.rectangle((38, 24, 44, 52), fill=shade(cloth, 0.8))       # shade


def draw_tool(c):
    metal = (150, 158, 170, 255)
    # wrench
    c.rrect((28, 24, 36, 56), metal, r=3)
    c.d.pieslice((22, 12, 42, 32), 210, 120, fill=metal, outline=OUTLINE)
    c.d.ellipse((28, 17, 36, 25), fill=(0, 0, 0, 0))
    c.d.arc((22, 12, 42, 32), 210, 120, fill=OUTLINE, width=1)
    c.d.rectangle((30, 26, 34, 54), fill=shade(metal, 0.8))


def draw_book(c):
    cover = (150, 70, 60, 255)
    c.rrect((20, 18, 46, 52), cover, r=2)
    c.rrect((22, 20, 44, 50), (232, 226, 210, 255), r=1)      # pages
    c.d.line((33, 20, 33, 50), fill=shade(cover, 0.6), width=2)  # spine
    for y in range(25, 47, 4):
        c.d.line((25, y, 31, y), fill=(160, 155, 145, 255), width=1)
        c.d.line((35, y, 41, y), fill=(160, 155, 145, 255), width=1)


def draw_container(c):
    box = (150, 110, 70, 255)
    c.rrect((18, 26, 46, 54), box, r=3)
    c.d.line((18, 34, 46, 34), fill=shade(box, 0.7), width=2)   # lid seam
    c.d.rectangle((30, 26, 34, 54), fill=shade(box, 0.8))       # strap
    c.d.rectangle((36, 28, 46, 52), fill=shade(box, 0.85))


def draw_chem(c):
    glass = (150, 210, 130, 220)
    c.poly([(28, 16), (36, 16), (36, 30), (46, 52), (18, 52), (28, 30)],
           (180, 190, 200, 200))                                # flask
    c.poly([(30, 34), (34, 34), (42, 50), (22, 50)], glass)     # liquid
    c.rrect((28, 12, 36, 18), (120, 100, 90, 255), r=1)         # stopper


def draw_electronic(c):
    board = (60, 110, 80, 255)
    c.rrect((18, 24, 46, 50), board, r=2)
    for x in (24, 32, 40):
        for y in (30, 40):
            c.d.rectangle((x - 3, y - 3, x + 3, y + 3), fill=(150, 158, 170, 255))
    c.d.line((21, 46, 43, 46), fill=(200, 170, 70, 255), width=2)


def draw_material(c):
    # roll / bar of raw material
    mat = (170, 150, 120, 255)
    c.rrect((16, 30, 48, 48), mat, r=4)
    c.d.ellipse((14, 30, 22, 48), fill=shade(mat, 0.85), outline=OUTLINE)
    c.d.ellipse((44, 30, 52, 48), fill=shade(mat, 1.1), outline=OUTLINE)


def draw_generic(c):
    box = (140, 146, 156, 255)
    c.rrect((20, 24, 44, 52), box, r=4)
    c.d.text((28, 30), "?", fill=(40, 40, 46, 255))
    c.d.rectangle((33, 24, 37, 52), fill=shade(box, 0.85))


def draw_corpse(c):
    flesh = (150, 120, 116, 255)
    c.d.pieslice((14, 40, 50, 60), 180, 360, fill=flesh, outline=OUTLINE)
    c.rrect((14, 46, 50, 56), flesh, r=6)
    c.solid_ellipse((14, 42, 26, 54), flesh)                   # head lying
    c.d.line((30, 46, 44, 46), fill=(110, 40, 40, 255), width=2)
    c.d.line((22, 47, 24, 49), fill=(30, 26, 24, 255), width=1)  # x eye


def draw_plant(c):
    stem = (90, 140, 70, 255)
    c.line((32, 58, 32, 34), stem, w=3)
    c.poly([(32, 40), (18, 30), (30, 34)], stem)
    c.poly([(32, 44), (46, 34), (34, 38)], stem)
    c.solid_ellipse((26, 18, 40, 34), (210, 90, 120, 255))     # flower
    c.d.ellipse((30, 24, 36, 30), fill=(240, 210, 90, 255))


def draw_creature(c):
    # generic unknown critter blob with eyes + legs
    body = (130, 110, 150, 255)
    c.solid_ellipse((20, 30, 44, 52), body)
    for x in (24, 32, 40):
        c.line((x, 50, x, 60), OUTLINE, w=2)
    c.d.ellipse((26, 34, 32, 41), fill=(240, 240, 245, 255))
    c.d.ellipse((33, 34, 39, 41), fill=(240, 240, 245, 255))
    c.d.ellipse((28, 36, 31, 39), fill=(30, 26, 24, 255))
    c.d.ellipse((35, 36, 38, 39), fill=(30, 26, 24, 255))


def draw_bear(c):
    body = (104, 78, 56, 255)
    c.rrect((12, 26, 48, 48), body, r=9)
    for x in (15, 24, 34, 42):
        c.rrect((x, 46, x + 6, 58), shade(body, 0.8), r=2)
    c.solid_ellipse((40, 16, 58, 34), body)          # head
    c.poly([(42, 16), (45, 10), (48, 18)], body)      # ear
    c.poly([(52, 15), (55, 10), (58, 18)], body)
    c.solid_ellipse((52, 24, 60, 31), shade(body, 1.2))  # snout
    c.d.ellipse((47, 21, 50, 24), fill=(30, 26, 24, 255))


def draw_rat(c):
    body = (128, 122, 118, 255)
    c.solid_ellipse((18, 40, 44, 56), body)
    c.solid_ellipse((38, 38, 52, 50), body)          # head
    c.poly([(50, 42), (58, 44), (50, 47)], (200, 160, 160, 255))  # snout
    c.poly([(40, 36), (43, 30), (46, 38)], (170, 140, 140, 255))  # ear
    c.line((18, 48, 6, 42), (190, 150, 150, 255), w=2)  # tail
    c.d.ellipse((44, 42, 46, 44), fill=(30, 26, 24, 255))


def draw_nether(c):
    body = (96, 60, 120, 255)
    c.solid_ellipse((18, 22, 46, 52), body)
    # writhing tendrils
    for x, dx in ((20, -8), (28, -4), (36, 4), (44, 8)):
        c.line((x, 50, x + dx, 60), shade(body, 0.75), w=3)
    # too many eyes
    for ex, ey in ((25, 30), (33, 26), (39, 32), (29, 38), (37, 40)):
        c.d.ellipse((ex - 2, ey - 2, ex + 2, ey + 2), fill=(240, 210, 90, 255))
        c.d.ellipse((ex - 1, ey - 1, ex + 1, ey + 1), fill=(60, 20, 20, 255))


def draw_feral(c):
    humanoid(c, SKIN, (96, 84, 70, 255), (70, 62, 54, 255),
             hair=(52, 40, 32, 255), hunch=2, eye=(210, 60, 50, 255),
             long_hair=True)
    # torn clothing marks
    c.d.line((26, 34, 30, 38), fill=(60, 50, 42, 255), width=2)
    c.d.line((36, 40, 39, 43), fill=(60, 50, 42, 255), width=2)


def draw_cow(c):
    body = (222, 214, 202, 255)
    c.rrect((12, 26, 48, 48), body, r=8)
    # patches
    c.solid_ellipse((18, 30, 30, 42), (70, 58, 50, 255))
    c.solid_ellipse((34, 34, 44, 44), (70, 58, 50, 255))
    for x in (15, 24, 34, 42):
        c.rrect((x, 46, x + 5, 58), shade(body, 0.85), r=2)
    c.solid_ellipse((42, 18, 58, 34), body)          # head
    c.poly([(43, 18), (40, 12), (47, 16)], (170, 150, 140, 255))  # ear
    c.solid_ellipse((50, 26, 60, 34), (216, 180, 178, 255))       # muzzle
    c.d.ellipse((47, 22, 50, 25), fill=(30, 26, 24, 255))


def draw_frog(c):
    body = (96, 156, 84, 255)
    c.solid_ellipse((16, 34, 48, 56), body)
    c.solid_ellipse((20, 26, 32, 40), body)          # eye bumps
    c.solid_ellipse((34, 26, 46, 40), body)
    c.d.ellipse((24, 29, 29, 34), fill=(240, 240, 220, 255))
    c.d.ellipse((37, 29, 42, 34), fill=(240, 240, 220, 255))
    c.d.ellipse((26, 30, 28, 33), fill=(30, 26, 24, 255))
    c.d.ellipse((38, 30, 41, 33), fill=(30, 26, 24, 255))
    c.poly([(14, 52), (6, 58), (18, 56)], shade(body, 0.85))   # legs
    c.poly([(50, 52), (58, 58), (46, 56)], shade(body, 0.85))


def draw_bat(c):
    wing = (58, 50, 62, 255)
    body = (76, 64, 76, 255)
    c.poly([(30, 30), (6, 22), (12, 36), (20, 34), (28, 40)], wing)
    c.poly([(34, 30), (58, 22), (52, 36), (44, 34), (36, 40)], wing)
    c.solid_ellipse((26, 26, 38, 44), body)
    c.poly([(28, 26), (26, 18), (32, 24)], body)
    c.poly([(36, 26), (38, 18), (32, 24)], body)
    c.d.ellipse((29, 31, 31, 33), fill=(220, 200, 90, 255))
    c.d.ellipse((33, 31, 35, 33), fill=(220, 200, 90, 255))


def draw_worm(c):
    body = (196, 132, 128, 255)
    # segmented coil
    for i, (x, y, r) in enumerate(((16, 48, 8), (26, 42, 9), (38, 44, 9),
                                   (48, 38, 8), (44, 26, 7))):
        c.solid_ellipse((x - r, y - r, x + r, y + r),
                        shade(body, 1.0 - 0.05 * i))
    c.d.ellipse((40, 20, 50, 30), fill=shade(body, 1.1), outline=OUTLINE)
    c.d.ellipse((43, 23, 47, 27), fill=(120, 60, 60, 255))  # maw


def draw_player_f(c):
    humanoid(c, SKIN, (140, 80, 140, 255), PANTS, hair=(90, 60, 34, 255),
             long_hair=True)


def draw_npc_f(c):
    humanoid(c, SKIN, (80, 130, 110, 255), (74, 70, 60, 255),
             hair=(40, 34, 30, 255), long_hair=True)


def draw_zombie_spitter(c):
    humanoid(c, (160, 164, 96, 255), (110, 112, 76, 255), (86, 88, 62, 255),
             hair=(80, 84, 50, 255), hunch=3)
    # acid drool
    c.d.line((32, 24, 32, 30), fill=(180, 220, 60, 255), width=2)
    c.d.ellipse((30, 30, 34, 34), fill=(180, 220, 60, 255))


def draw_zombie_shocker(c):
    humanoid(c, (150, 170, 200, 255), (100, 116, 150, 255), (80, 92, 120, 255),
             hair=(110, 130, 160, 255), hunch=2, eye=(120, 200, 255, 255))
    # sparks
    for x, y in ((20, 22), (44, 30), (26, 46)):
        c.d.line((x - 3, y, x + 3, y), fill=(170, 230, 255, 255), width=1)
        c.d.line((x, y - 3, x, y + 3), fill=(170, 230, 255, 255), width=1)


def draw_zombie_soldier(c):
    humanoid(c, ZOMBIE_SKIN, (92, 104, 72, 255), (78, 88, 62, 255),
             hair=(60, 70, 50, 255), hunch=2, helmet=(70, 80, 58, 255))
    # webbing strap
    c.d.line((24, 30, 40, 42), fill=(56, 64, 44, 255), width=3)


def draw_squirrel(c):
    body = (150, 104, 66, 255)
    c.solid_ellipse((24, 42, 42, 56), body)
    c.solid_ellipse((36, 34, 48, 46), body)          # head
    # big curled tail
    c.d.arc((10, 26, 34, 54), 90, 300, fill=shade(body, 0.85), width=6)
    c.poly([(38, 32), (40, 27), (43, 33)], body)      # ear
    c.d.ellipse((41, 38, 43, 40), fill=(30, 26, 24, 255))


def draw_moose(c):
    body = (98, 76, 58, 255)
    c.rrect((12, 24, 44, 44), body, r=8)
    for x in (14, 22, 32, 39):
        c.rrect((x, 42, x + 5, 60), shade(body, 0.8), r=2)
    c.solid_ellipse((38, 12, 54, 30), body)          # head
    c.solid_ellipse((48, 20, 58, 30), shade(body, 0.9))  # muzzle
    # antlers
    c.d.arc((28, 2, 46, 18), 180, 330, fill=(196, 178, 140, 255), width=3)
    c.d.arc((42, 2, 60, 18), 210, 360, fill=(196, 178, 140, 255), width=3)
    c.d.ellipse((43, 17, 46, 20), fill=(30, 26, 24, 255))


def draw_crow(c):
    body = (48, 48, 56, 255)
    c.solid_ellipse((22, 32, 44, 50), body)
    c.solid_ellipse((36, 22, 48, 34), body)          # head
    c.poly([(46, 26), (56, 28), (46, 31)], (110, 110, 118, 255))  # beak
    c.poly([(22, 36), (8, 32), (22, 46)], shade(body, 0.8))       # wing
    c.line((30, 50, 28, 60), (110, 110, 118, 255), w=2)
    c.line((36, 50, 38, 60), (110, 110, 118, 255), w=2)
    c.d.ellipse((40, 26, 43, 29), fill=(220, 220, 226, 255))


DRAW = {
    "player": draw_player, "npc": draw_npc, "zombie": draw_zombie,
    "human": draw_human, "skeleton": draw_skeleton, "robot": draw_robot,
    "child": draw_child, "hulk": draw_hulk,
    "mammal": draw_mammal, "insect": draw_insect, "spider": draw_spider,
    "bird": draw_bird, "reptile": draw_reptile, "fish": draw_fish,
    "slime": draw_slime, "fungus": draw_fungus,
    "gun": draw_gun, "melee": draw_melee, "ammo": draw_ammo, "food": draw_food,
    "drink": draw_drink, "clothing": draw_clothing, "tool": draw_tool,
    "book": draw_book,
    "container": draw_container, "chem": draw_chem, "electronic": draw_electronic,
    "material": draw_material, "generic": draw_generic, "corpse": draw_corpse,
    "plant": draw_plant, "creature": draw_creature,
    "bear": draw_bear, "rat": draw_rat, "nether": draw_nether,
    "feral": draw_feral, "cow": draw_cow, "frog": draw_frog, "bat": draw_bat,
    "worm": draw_worm,
    "player_f": draw_player_f, "npc_f": draw_npc_f,
    "zombie_spitter": draw_zombie_spitter, "zombie_shocker": draw_zombie_shocker,
    "zombie_soldier": draw_zombie_soldier, "squirrel": draw_squirrel,
    "moose": draw_moose, "crow": draw_crow,
}


# ---- terrain textures ------------------------------------------------------
# Tileable 32px textures for block top and side faces. No outlines — they
# tile across the world. Order mirrors terrain_cell in src/sdltiles.cpp.
import random

TER_CELL = 32
TER_COLS = 8
TER_ROWS = 4
TERRAIN_LAYOUT = [
    "grass", "tall_grass", "dirt", "sand", "gravel", "pavement", "sidewalk",
    "concrete",
    "floor_wood", "wall_brick", "wall_concrete", "roof", "water", "deep_water",
    "rock", "mud",
    "tree", "shrub", "underbrush", "door", "window", "dirt_side", "wood_side",
    "metal",
    "water2", "deep_water2", "rubble", "fungal", "ice", "snow", "tile_floor",
    "rock_floor",
]


def speckle(d, rng, base, specks, n=170):
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=base)
    for _ in range(n):
        d.point((rng.randrange(TER_CELL), rng.randrange(TER_CELL)),
                fill=rng.choice(specks))


def t_grass(d, rng):
    speckle(d, rng, (74, 111, 57, 255),
            [(88, 128, 66, 255), (62, 96, 48, 255), (99, 140, 74, 255),
             (55, 86, 44, 255)])
    for _ in range(14):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x, max(0, y - 2)), fill=(104, 148, 80, 255))


def t_tall_grass(d, rng):
    speckle(d, rng, (86, 118, 56, 255),
            [(104, 140, 66, 255), (70, 100, 48, 255), (120, 152, 78, 255)])
    for _ in range(22):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(6, TER_CELL)
        d.line((x, y, x + rng.choice((-1, 0, 1)), y - rng.randrange(3, 6)),
               fill=(126, 160, 84, 255))


def t_dirt(d, rng):
    speckle(d, rng, (121, 96, 68, 255),
            [(138, 111, 80, 255), (104, 82, 58, 255), (146, 120, 90, 255),
             (92, 72, 52, 255)])


def t_sand(d, rng):
    speckle(d, rng, (204, 178, 128, 255),
            [(216, 192, 142, 255), (188, 162, 114, 255), (224, 202, 156, 255)])


def t_gravel(d, rng):
    speckle(d, rng, (136, 132, 126, 255),
            [(158, 154, 148, 255), (112, 108, 104, 255), (170, 168, 162, 255),
             (96, 94, 90, 255)], n=220)


def t_pavement(d, rng):
    speckle(d, rng, (72, 72, 76, 255),
            [(80, 80, 84, 255), (64, 64, 68, 255), (88, 88, 92, 255)], n=120)
    # cracks
    for _ in range(2):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        for _ in range(8):
            nx = x + rng.choice((-1, 0, 1, 1))
            ny = y + rng.choice((-1, 0, 1))
            d.line((x, y, nx, ny), fill=(52, 52, 56, 255))
            x, y = nx % TER_CELL, ny % TER_CELL


def t_sidewalk(d, rng):
    speckle(d, rng, (156, 154, 148, 255),
            [(168, 166, 160, 255), (144, 142, 136, 255)], n=110)
    d.line((0, 15, 31, 15), fill=(120, 118, 112, 255))
    d.line((15, 0, 15, 31), fill=(120, 118, 112, 255))


def t_concrete(d, rng):
    speckle(d, rng, (128, 128, 130, 255),
            [(140, 140, 142, 255), (116, 116, 118, 255)], n=130)


def t_floor_wood(d, rng):
    base = (150, 111, 74, 255)
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=base)
    for px in range(0, TER_CELL, 8):
        d.line((px, 0, px, TER_CELL - 1), fill=(112, 80, 52, 255))
        for _ in range(10):
            x = px + 1 + rng.randrange(7)
            y = rng.randrange(TER_CELL)
            d.line((x, y, x, y + rng.randrange(2, 5)), fill=(136, 99, 64, 255))


def t_wall_brick(d, rng):
    mortar = (168, 158, 148, 255)
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=mortar)
    bh = 6
    bw = 12
    for row, y in enumerate(range(0, TER_CELL, bh)):
        off = (bw // 2) if row % 2 else 0
        for x in range(-bw, TER_CELL + bw, bw):
            c = rng.choice([(148, 74, 58, 255), (158, 82, 62, 255),
                            (140, 68, 54, 255)])
            d.rectangle((x + off + 1, y + 1, x + off + bw - 1, y + bh - 1),
                        fill=c)


def t_wall_concrete(d, rng):
    speckle(d, rng, (140, 138, 136, 255),
            [(150, 148, 146, 255), (128, 126, 124, 255)], n=110)
    d.line((0, 10, 31, 10), fill=(116, 114, 112, 255))
    d.line((0, 21, 31, 21), fill=(116, 114, 112, 255))


def t_roof(d, rng):
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=(94, 88, 96, 255))
    for row, y in enumerate(range(0, TER_CELL, 6)):
        c = (106, 100, 108, 255) if row % 2 else (84, 78, 86, 255)
        d.rectangle((0, y, TER_CELL - 1, y + 4), fill=c)
        d.line((0, y + 5, TER_CELL - 1, y + 5), fill=(64, 60, 66, 255))


def t_water(d, rng):
    speckle(d, rng, (52, 106, 158, 255),
            [(60, 118, 172, 255), (46, 96, 146, 255)], n=80)
    for _ in range(7):
        x = rng.randrange(TER_CELL - 8)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x + rng.randrange(4, 9), y), fill=(96, 152, 200, 255))


def t_deep_water(d, rng):
    speckle(d, rng, (34, 74, 122, 255),
            [(40, 84, 134, 255), (28, 64, 110, 255)], n=70)
    for _ in range(5):
        x = rng.randrange(TER_CELL - 8)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x + rng.randrange(4, 8), y), fill=(64, 110, 160, 255))


def t_rock(d, rng):
    speckle(d, rng, (118, 116, 118, 255),
            [(132, 130, 132, 255), (104, 102, 104, 255)], n=120)
    for _ in range(6):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        r = rng.randrange(3, 7)
        d.ellipse((x - r, y - r // 2, x + r, y + r // 2),
                  outline=(96, 94, 96, 255))


def t_mud(d, rng):
    speckle(d, rng, (94, 82, 60, 255),
            [(106, 94, 70, 255), (82, 72, 52, 255), (74, 78, 56, 255)], n=180)


def t_tree(d, rng):
    speckle(d, rng, (44, 78, 40, 255),
            [(54, 92, 48, 255), (36, 66, 34, 255)], n=140)
    for _ in range(12):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        r = rng.randrange(2, 5)
        d.ellipse((x - r, y - r, x + r, y + r), fill=(62, 104, 54, 255))
    for _ in range(8):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        d.point((x, y), fill=(90, 138, 74, 255))


def t_shrub(d, rng):
    speckle(d, rng, (66, 100, 52, 255),
            [(80, 118, 62, 255), (54, 84, 44, 255), (96, 134, 72, 255)], n=200)


def t_underbrush(d, rng):
    speckle(d, rng, (84, 96, 52, 255),
            [(100, 114, 62, 255), (70, 80, 44, 255), (110, 96, 60, 255)],
            n=200)
    for _ in range(10):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x + rng.choice((-2, 2)), y - rng.randrange(2, 4)),
               fill=(118, 128, 70, 255))


def t_door(d, rng):
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=(134, 96, 60, 255))
    d.rectangle((2, 2, TER_CELL - 3, TER_CELL - 3),
                outline=(104, 72, 44, 255))
    d.rectangle((6, 5, TER_CELL - 7, 14), outline=(104, 72, 44, 255))
    d.rectangle((6, 18, TER_CELL - 7, 27), outline=(104, 72, 44, 255))
    d.ellipse((24, 15, 27, 18), fill=(210, 190, 120, 255))


def t_window(d, rng):
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=(140, 138, 136, 255))
    d.rectangle((3, 3, TER_CELL - 4, TER_CELL - 4), fill=(158, 196, 220, 255))
    d.rectangle((3, 3, TER_CELL - 4, TER_CELL - 4),
                outline=(110, 108, 106, 255))
    d.line((TER_CELL // 2, 3, TER_CELL // 2, TER_CELL - 4),
           fill=(110, 108, 106, 255))
    d.line((3, TER_CELL // 2, TER_CELL - 4, TER_CELL // 2),
           fill=(110, 108, 106, 255))
    d.line((6, 6, 12, 12), fill=(210, 232, 244, 255))


def t_dirt_side(d, rng):
    speckle(d, rng, (108, 84, 58, 255),
            [(122, 96, 68, 255), (94, 72, 50, 255), (130, 106, 78, 255)],
            n=190)
    # buried stones
    for _ in range(5):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(8, TER_CELL)
        d.ellipse((x, y, x + 3, y + 2), fill=(140, 134, 126, 255))


def t_wood_side(d, rng):
    base = (128, 92, 58, 255)
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=base)
    for y in range(0, TER_CELL, 8):
        d.line((0, y, TER_CELL - 1, y), fill=(96, 66, 42, 255))
        for _ in range(8):
            x = rng.randrange(TER_CELL)
            yy = y + 1 + rng.randrange(7)
            d.line((x, yy, x + rng.randrange(2, 6), yy),
                   fill=(112, 80, 50, 255))


def t_metal(d, rng):
    speckle(d, rng, (122, 128, 136, 255),
            [(134, 140, 148, 255), (110, 116, 124, 255)], n=90)
    for x in (4, 27):
        for y in (4, 27):
            d.ellipse((x - 1, y - 1, x + 1, y + 1), fill=(90, 96, 104, 255))
    d.line((0, 15, 31, 15), fill=(104, 110, 118, 255))


def t_rubble(d, rng):
    speckle(d, rng, (112, 106, 100, 255),
            [(128, 122, 116, 255), (96, 90, 86, 255), (140, 100, 80, 255)],
            n=160)
    for _ in range(9):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        w = rng.randrange(3, 7)
        d.rectangle((x, y, x + w, y + w // 2), fill=rng.choice(
            [(130, 124, 118, 255), (150, 82, 64, 255), (100, 96, 92, 255)]),
            outline=(70, 66, 62, 255))


def t_fungal(d, rng):
    speckle(d, rng, (176, 160, 176, 255),
            [(196, 178, 196, 255), (156, 140, 158, 255)], n=150)
    for _ in range(8):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        r = rng.randrange(2, 4)
        d.ellipse((x - r, y - r, x + r, y + r), fill=(210, 190, 210, 255),
                  outline=(140, 110, 140, 255))


def t_ice(d, rng):
    speckle(d, rng, (188, 216, 232, 255),
            [(200, 226, 240, 255), (176, 206, 224, 255)], n=90)
    for _ in range(5):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x + rng.randrange(4, 10), y + rng.randrange(2, 6)),
               fill=(226, 240, 248, 255))


def t_snow(d, rng):
    speckle(d, rng, (232, 236, 242, 255),
            [(242, 245, 250, 255), (220, 226, 234, 255)], n=110)


def t_tile_floor(d, rng):
    d.rectangle((0, 0, TER_CELL - 1, TER_CELL - 1), fill=(206, 204, 198, 255))
    for x in range(0, TER_CELL, 8):
        d.line((x, 0, x, TER_CELL - 1), fill=(170, 168, 162, 255))
    for y in range(0, TER_CELL, 8):
        d.line((0, y, TER_CELL - 1, y), fill=(170, 168, 162, 255))


def t_rock_floor(d, rng):
    speckle(d, rng, (134, 130, 128, 255),
            [(146, 142, 140, 255), (120, 116, 114, 255)], n=140)
    for _ in range(4):
        x = rng.randrange(TER_CELL)
        y = rng.randrange(TER_CELL)
        d.line((x, y, x + rng.randrange(3, 8), y + rng.randrange(-2, 3)),
               fill=(104, 100, 98, 255))


TERRAIN_DRAW = {
    "grass": t_grass, "tall_grass": t_tall_grass, "dirt": t_dirt,
    "sand": t_sand, "gravel": t_gravel, "pavement": t_pavement,
    "sidewalk": t_sidewalk, "concrete": t_concrete,
    "floor_wood": t_floor_wood, "wall_brick": t_wall_brick,
    "wall_concrete": t_wall_concrete, "roof": t_roof, "water": t_water,
    "deep_water": t_deep_water, "rock": t_rock, "mud": t_mud,
    "tree": t_tree, "shrub": t_shrub, "underbrush": t_underbrush,
    "door": t_door, "window": t_window, "dirt_side": t_dirt_side,
    "wood_side": t_wood_side, "metal": t_metal,
    # Second animation frames reuse the water painters with a different
    # rng seed (keyed by name), giving shifted wave placement.
    "water2": t_water, "deep_water2": t_deep_water,
    "rubble": t_rubble, "fungal": t_fungal, "ice": t_ice, "snow": t_snow,
    "tile_floor": t_tile_floor, "rock_floor": t_rock_floor,
}


def build_entities():
    atlas = Image.new("RGBA", (CELL * COLS, CELL * ROWS), (0, 0, 0, 0))
    for i, name in enumerate(LAYOUT):
        cell = Cell()
        DRAW[name](cell)
        atlas.paste(cell.img, ((i % COLS) * CELL, (i // COLS) * CELL))
    return atlas


def build_terrain():
    atlas = Image.new("RGBA", (TER_CELL * TER_COLS, TER_CELL * TER_ROWS),
                      (0, 0, 0, 0))
    for i, name in enumerate(TERRAIN_LAYOUT):
        tile = Image.new("RGBA", (TER_CELL, TER_CELL), (0, 0, 0, 0))
        d = ImageDraw.Draw(tile)
        TERRAIN_DRAW[name](d, random.Random(hash(name) & 0xFFFF))
        atlas.paste(tile, ((i % TER_COLS) * TER_CELL,
                           (i // TER_COLS) * TER_CELL))
    return atlas


def c_array(name, data):
    lines = [f"const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 20):
        lines.append("    " + ",".join(str(b) for b in data[i:i + 20]) + ",")
    lines.append("};")
    lines.append(f"const unsigned int {name}_len = {len(data)};")
    return "\n".join(lines)


def main():
    import io
    import os
    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
    out_dir = os.path.join(root, "gfx", "Block3D")
    os.makedirs(out_dir, exist_ok=True)

    entities = build_entities()
    terrain = build_terrain()
    entities.save(os.path.join(out_dir, "entities.png"))
    terrain.save(os.path.join(out_dir, "terrain.png"))

    # Embed both PNGs as C arrays so the renderer can never miss them at
    # runtime, whatever the install layout.
    bufs = {}
    for name, img in (("block3d_entities_png", entities),
                      ("block3d_terrain_png", terrain)):
        b = io.BytesIO()
        img.save(b, format="PNG", optimize=True)
        bufs[name] = b.getvalue()

    src = os.path.join(root, "src", "block3d_atlas.cpp")
    with open(src, "w") as f:
        f.write("// Generated by tools/gfx/gen_block3d_sprites.py — do not "
                "edit by hand.\n")
        f.write("// Embedded built-in sprite/texture atlases for the "
                "block_3d renderer.\n")
        f.write('#include "block3d_atlas.h"\n\n')
        for name, data in bufs.items():
            f.write(c_array(name, data))
            f.write("\n\n")
    print("wrote", src, {k: len(v) for k, v in bufs.items()})
    print("wrote", os.path.join(out_dir, "entities.png"), entities.size)
    print("wrote", os.path.join(out_dir, "terrain.png"), terrain.size)


if __name__ == "__main__":
    main()
