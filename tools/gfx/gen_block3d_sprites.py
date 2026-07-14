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
ROWS = 4
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


def humanoid(c, skin, shirt, pants, hair=HAIR, hunch=0, glow_eyes=False):
    """A simple front-facing biped: head, torso, arms, legs; feet ~y=60."""
    cx = 32
    top = 10 + hunch
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
    # eyes
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
}


def main():
    import os
    atlas = Image.new("RGBA", (CELL * COLS, CELL * ROWS), (0, 0, 0, 0))
    for i, name in enumerate(LAYOUT):
        cell = Cell()
        DRAW[name](cell)
        col = i % COLS
        row = i // COLS
        atlas.paste(cell.img, (col * CELL, row * CELL))
    out_dir = os.path.join(os.path.dirname(__file__), "..", "..", "gfx", "Block3D")
    out_dir = os.path.abspath(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "entities.png")
    atlas.save(out)
    print("wrote", out, atlas.size, "cells:", len(LAYOUT))


if __name__ == "__main__":
    main()
