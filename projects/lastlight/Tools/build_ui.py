#!/usr/bin/env python3
"""Author Lastlight's field-kit UI using ordinary JudasUI primitives.

Artwork is a background/illustration, never baked-in gameplay text or input.
This can run independently without regenerating the scene, prefabs or navmesh.
"""
from pathlib import Path
import hashlib
import json

PROJECT = Path(__file__).resolve().parents[1]
CREAM = (.93, .9, .79, 1)
MUTED = (.66, .72, .64, 1)
AMBER = (.91, .68, .31, 1)
TEAL = (.52, .75, .71, 1)
INK = (.055, .074, .058, .91)
POCKET = (.1, .135, .102, .93)


def author_ui(ids, track):
    for path in sorted((PROJECT / 'Assets/textures/ui').glob('*.png')):
        track(path, 'texture')
    for name in ['DejaVuSansCondensed-Bold', 'DejaVuSansMono']:
        track(PROJECT / 'Assets/fonts' / (name + '.ttf'), 'font')
    normal, title, mono = ids['DejaVuSans'], ids['DejaVuSansCondensed-Bold'], ids['DejaVuSansMono']
    rows = []

    def element(name, parent='canvas', kind=1, *, xy=(0, 0), wh=(0, 0),
                anchor=(0, 0), pivot=(0, 0), relative=(0, 0), visible=True,
                text='', font=None, fs=18, color=CREAM, bg=(0, 0, 0, 0),
                texture='', fit=False, wrap=False, centered=False, clip=False,
                vertical=0):
        # The normal JudasUI 2 schema; no private widget or native game path.
        values = [name, parent, kind, 0, int(visible), 1, int(clip), int(wrap),
                  int(fit), *anchor, *anchor, *xy, *wh, *relative, *pivot,
                  .5 if centered else 0, .5 if centered else vertical,
                  0, 0, 0, 0, 0, 0, 0, 0, *bg, *color, 0, fs, 0, 0, 1,
                  text, texture, font or normal, '', 0, 2 if centered else 0, 0]
        # JudasUI uses std::quoted, not JSON escapes: keep real line breaks in
        # quoted text so authored field notes match runtime multiline labels.
        def quote(value):
            return '"' + value.replace('\\', '\\\\').replace('"', '\\"') + '"'
        rows.append(' '.join(quote(v) if isinstance(v, str) else str(v) for v in values))

    def label(name, parent='canvas', text='', **kwargs):
        element(name, parent, 2, text=text, **kwargs)

    def image(name, parent, asset, **kwargs):
        element(name, parent, 3, texture=ids[asset], color=(1, 1, 1, 1), **kwargs)

    def button(name, parent, text, xy, wh, fs=18):
        element(name + '_edge', parent, xy=xy, wh=wh, bg=(.47, .43, .27, .85))
        element(name, parent, 4, xy=(xy[0] + 1, xy[1] + 1),
                wh=(wh[0] - 2, wh[1] - 2), text=text, font=title, fs=fs,
                bg=POCKET, centered=True)

    element('canvas', '', 0)
    # Compact cards leave the centre of the screen to the town and aiming.
    image('health_canvas', 'canvas', 'hud_canvas', xy=(18, 14), wh=(350, 116))
    label('title', text='LASTLIGHT', xy=(38, 29), wh=(200, 25), fs=22, font=title)
    label('health', text='HEALTH 100', xy=(38, 61), wh=(180, 24), fs=18, font=mono)
    element('health_track', xy=(38, 94), wh=(226, 5), bg=(.25, .3, .23, 1))
    element('health_fill', xy=(38, 94), wh=(226, 5), bg=(.74, .79, .48, 1))
    label('wave', text='01 / 05', xy=(245, 33), wh=(100, 24), font=mono, color=AMBER, fs=18)
    label('enemies', text='0 ENEMIES', xy=(221, 65), wh=(120, 20), font=mono, color=MUTED, fs=13)

    image('score_canvas', 'canvas', 'hud_canvas', anchor=(1, 0), pivot=(1, 0),
          xy=(-18, 14), wh=(282, 94))
    label('score', anchor=(1, 0), pivot=(1, 0), xy=(-36, 33), wh=(238, 24),
          text='SCORE 0', font=mono, fs=19)
    label('points', anchor=(1, 0), pivot=(1, 0), xy=(-36, 65), wh=(238, 20),
          text='0 POINTS', font=mono, fs=14, color=AMBER)

    image('weapon_canvas', 'canvas', 'hud_canvas', anchor=(1, 1), pivot=(1, 1),
          xy=(-18, -18), wh=(350, 116))
    image('weapon_image', 'canvas', 'item_rifle', anchor=(1, 1), pivot=(1, 1),
          xy=(-249, -36), wh=(96, 68), fit=True)
    label('weapon', anchor=(1, 1), pivot=(1, 1), xy=(-36, -78), wh=(194, 26),
          text='RIFLE', font=title, fs=21)
    label('ammo', anchor=(1, 1), pivot=(1, 1), xy=(-36, -51), wh=(194, 26),
          text='24 / 24', font=mono, fs=21, color=AMBER)
    label('grenades', anchor=(1, 1), pivot=(1, 1), xy=(-36, -37), wh=(194, 19),
          text='GRENADES 0', fs=13, color=MUTED)

    label('progress', anchor=(0, 1), pivot=(0, 1), xy=(28, -58), wh=(560, 22),
          text='FIRST PERSON', fs=14, color=TEAL)
    label('controls', anchor=(0, 1), pivot=(0, 1), xy=(28, -28), wh=(550, 23),
          text='I  PACK     V  VIEW     ESC  PAUSE', font=mono, fs=13)
    element('message_backing', anchor=(.5, 1), pivot=(.5, 1), xy=(0, -151),
            wh=(800, 42), bg=INK, visible=False)
    label('message', anchor=(.5, 1), pivot=(.5, 1), xy=(0, -153), wh=(778, 38),
          text='', fs=17, centered=True, wrap=True, color=AMBER)
    element('cross_h', anchor=(.5, .5), xy=(-6, -1), wh=(12, 2), bg=CREAM)
    element('cross_v', anchor=(.5, .5), xy=(-1, -6), wh=(2, 12), bg=CREAM)
    label('hitmark', anchor=(.5, .5), pivot=(.5, .5), wh=(32, 32), text='X',
          color=AMBER, fs=26, centered=True, visible=False)
    element('hurt', anchor=(0, 0), relative=(1, 0), wh=(0, 7),
            bg=(.7, .12, .08, .8), visible=False)
    element('modal_shade', relative=(1, 1), bg=(.012, .017, .012, .68), visible=False)

    element('pause', anchor=(.5, .5), pivot=(.5, .5), wh=(780, 544), visible=False)
    image('pause_canvas', 'pause', 'field_canvas', wh=(780, 544))
    label('pause_kicker', 'pause', 'NEIGHBOURHOOD DEFENCE', xy=(44, 37), wh=(692, 20),
          font=mono, fs=13, color=AMBER)
    label('pause_title', 'pause', 'LASTLIGHT / PAUSED', xy=(42, 68), wh=(696, 44), font=title, fs=34)
    label('pause_hint', 'pause', 'Hold the town. Fortify a house. Survive five waves.',
          xy=(44, 119), wh=(692, 30), fs=17, color=MUTED)
    button('resume', 'pause', 'RETURN TO THE TOWN', (44, 177), (320, 52))
    button('restart', 'pause', 'START A FRESH ROUND', (44, 245), (320, 52))
    button('quit', 'pause', 'LEAVE LASTLIGHT', (44, 313), (320, 52))
    label('pause_controls_title', 'pause', 'FIELD NOTES', xy=(398, 177), wh=(325, 27), font=title, fs=20, color=AMBER)
    label('pause_controls', 'pause',
          'WASD move / SHIFT faster\nSPACE jump / E mantle / CTRL aim\nLEFT MOUSE fire / R reload\nRIGHT MOUSE punch / F heavy / Q shove\n1 rifle / 2 launcher / G grenade\nB carry or drop / T rotate a prop\nC collect / I pack / N supply stall\nV view / X release gravity frame',
          xy=(398, 215), wh=(328, 246), fs=15, wrap=True)
    label('pause_footer', 'pause', 'A quieter moment. The round is paused.', xy=(44, 469), wh=(692, 28), fs=15, color=MUTED)

    element('shop', anchor=(.5, .5), pivot=(.5, .5), wh=(780, 544), visible=False)
    image('shop_canvas', 'shop', 'field_canvas', wh=(780, 544))
    label('shop_kicker', 'shop', 'THE CORNER STALL', xy=(44, 37), wh=(690, 20), font=mono, fs=13, color=AMBER)
    label('shop_title', 'shop', 'NEIGHBOURHOOD SUPPLIES', xy=(42, 68), wh=(696, 44), font=title, fs=31)
    label('shop_hint', 'shop', '', xy=(44, 121), wh=(692, 52), fs=16, color=MUTED, wrap=True)
    for index, (item, name, text) in enumerate([
        ('item_rocket', 'buy_rockets', '2 ROCKETS       250 POINTS'),
        ('item_grenade', 'buy_grenades', '2 GRENADES      150 POINTS'),
        (None, 'buy_heal', 'RESTORE HEALTH  100 POINTS')]):
        y = 200 + index * 73
        if item: image(name + '_art', 'shop', item, xy=(44, y - 3), wh=(76, 66), fit=True)
        else: label('shop_health_symbol', 'shop', '+', xy=(44, y - 3), wh=(76, 66),
                    centered=True, font=title, fs=52, color=TEAL)
        button(name, 'shop', text, (142, y), (592, 56), 21)
    button('shop_close', 'shop', 'BACK TO THE TOWN', (44, 455), (692, 46))

    element('inventory', anchor=(.5, .5), pivot=(.5, .5), wh=(990, 620), visible=False)
    image('inventory_canvas', 'inventory', 'field_canvas', wh=(990, 620))
    label('inventory_kicker', 'inventory', 'TAKE WHAT YOU NEED. LEAVE NOTHING BEHIND.',
          xy=(38, 28), wh=(910, 20), font=mono, fs=12, color=AMBER)
    label('inventory_title', 'inventory', 'YOUR FIELD KIT', xy=(36, 57), wh=(906, 40), font=title, fs=32)
    label('inventory_hint', 'inventory', '', xy=(38, 110), wh=(910, 54), fs=15, color=MUTED, wrap=True)
    label('inventory_list_title', 'inventory', 'PACK / STOCK', xy=(38, 179), wh=(426, 24), font=mono, fs=13, color=AMBER)
    for index in range(6):
        y = 217 + index * 45
        element(f'inventory_row_pocket_{index}', 'inventory', xy=(38, y), wh=(432, 39), bg=POCKET)
        element(f'inventory_row_selected_{index}', 'inventory', xy=(38, y), wh=(4, 39), bg=AMBER, visible=False)
        element(f'inventory_row_{index}', 'inventory', 4, xy=(48, y + 1), wh=(416, 37),
                fs=14, bg=(.09, .12, .09, .4), visible=False, vertical=.5)
    button('inventory_previous', 'inventory', '< PREVIOUS', (38, 503), (154, 31), 13)
    label('inventory_page', 'inventory', '1 / 1', xy=(211, 503), wh=(76, 31), font=mono, fs=13, centered=True)
    button('inventory_next', 'inventory', 'NEXT >', (316, 503), (154, 31), 13)
    element('inventory_preview_pocket', 'inventory', xy=(495, 177), wh=(455, 340), bg=(.04, .063, .044, .6))
    image('inventory_item_image', 'inventory', 'item_rifle', xy=(528, 185), wh=(388, 166), fit=True)
    label('inventory_item_title', 'inventory', '', xy=(517, 361), wh=(410, 34), font=title, fs=25)
    label('inventory_item_status', 'inventory', '', xy=(519, 401), wh=(408, 22), font=mono, fs=13, color=TEAL)
    label('inventory_detail', 'inventory', '', xy=(519, 436), wh=(405, 74), fs=14, wrap=True)
    button('inventory_equip', 'inventory', 'EQUIP', (495, 531), (218, 40), 17)
    button('inventory_drop', 'inventory', 'DROP ONE', (728, 531), (222, 40), 17)
    label('inventory_help', 'inventory', 'Drop places one real pickup\nin front of you and closes the pack.',
          xy=(38, 546), wh=(432, 44), fs=13, color=MUTED, wrap=True)
    button('inventory_close', 'inventory', 'CLOSE / I', (779, 58), (171, 37), 16)

    path = PROJECT / 'Assets/ui/lastlight.judasui'
    path.write_text('JudasUI 2\n1280 720 1 1 0 ' + str(len(rows)) + '\n' + '\n'.join(rows) + '\n')
    track(path, 'ui')


if __name__ == '__main__':
    ids = json.loads((PROJECT / 'Assets/ids.json').read_text())
    def track(path, kind):
        asset_id = hashlib.md5(('lastlight/' + str(path.relative_to(PROJECT))).encode()).hexdigest()
        path.with_name(path.name + '.judasmeta').write_text(
            f'JudasAssetMeta 1\nid "{asset_id}"\ntype {kind}\nsource ""\n')
        ids[path.stem] = asset_id
        return asset_id
    author_ui(ids, track)
    (PROJECT / 'Assets/ids.json').write_text(json.dumps(ids, indent=2) + '\n')
