#!/usr/bin/env python3
"""Author Hearthworld's native JudasUI document and original short PCM sounds."""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path
import random
import struct
import wave

ROOT = Path(__file__).resolve().parents[1]
FONT = "9c3ee9e1ef311e9c2417661910eb145b"
UI_ID = "f1000000000000000000000000000002"
AUDIO_IDS = {name: f"f103000000000000000000000000000{i}" for i, name in enumerate(
    ("chop", "dig", "build", "grab", "launch", "pickup", "stone"), 1)}
PARCHMENT = [0.95, 0.91, 0.79, 1]
CREAM = [0.96, 0.94, 0.85, 1]
TEAL = [0.08, 0.23, 0.22, 0.96]
DARK = [0.035, 0.075, 0.073, 0.84]
INK = [0.11, 0.20, 0.19, 1]
GOLD = [0.96, 0.73, 0.34, 1]


def meta(path, identity, kind):
    Path(str(path) + ".judasmeta").write_text(
        f'JudasAssetMeta 1\nid "{identity}"\ntype {kind}\nsource "Original Hearthworld authored content"\n')


def make_ui():
    elements = []

    def element(identity, text="", kind=2, parent="canvas", pos=(0, 0), size=(300, 32),
                anchor=(0, 0), align=(0, 0), color=CREAM, background=(0, 0, 0, 0),
                font_size=18, visible=True, wrap=False, text_align=(0, 0.5), padding=(0, 0, 0, 0)):
        item = dict(id=identity, parent=parent, text=text, texture="", font=FONT,
                    textKey="", textLogicalAlign=-1, mirrorRow=False, visible=visible,
                    enabled=True, clip=False, wrap=wrap, fit=True, spacing=8,
                    fontSize=font_size, value=0, minimum=0, maximum=1, kind=kind,
                    flow=0, direction=0, anchorMin=list(anchor), anchorMax=list(anchor),
                    offset=list(pos), size=list(size), relativeSize=[0, 0], align=list(align),
                    textAlign=list(text_align), margin=[0, 0, 0, 0], padding=list(padding),
                    background=list(background), color=list(color))
        elements.append(item)
        return item

    canvas = element("canvas", kind=0, parent="", size=(0, 0))
    canvas["anchorMax"] = [1, 1]
    element("brand_rule", kind=1, pos=(24, 22), size=(4, 46), background=GOLD)
    element("brand", "HEARTHWORLD", pos=(42, 18), size=(420, 36), font_size=27,
            color=CREAM)
    element("brand_subtitle", "A little world. A place to call home.", pos=(44, 54),
            size=(470, 23), font_size=14, color=[0.80, 0.87, 0.78, 1])
    element("resources_panel", kind=1, anchor=(1, 0), align=(1, 0),
            pos=(-24, 24), size=(440, 48), background=DARK)
    element("resources", "WOOD  0    /    STONE  0", parent="resources_panel",
            pos=(14, 8), size=(412, 32), font_size=18,
            color=PARCHMENT, text_align=(1, 0.5))
    element("objective_panel", kind=1, pos=(24, 95), size=(570, 74), background=DARK)
    element("objective", "Make yourself at home. Collect your tools inside the cottage.",
            parent="objective_panel", pos=(15, 9), size=(540, 56), font_size=17,
            color=PARCHMENT, wrap=True)
    element("reticle", "+", anchor=(0.5, 0.5), align=(0.5, 0.5),
            size=(30, 30), font_size=26, text_align=(0.5, 0.5), color=CREAM)
    element("context", "", anchor=(0.5, 0.5), align=(0.5, 0), pos=(0, 34),
            size=(670, 66), font_size=19, text_align=(0.5, 0.5), wrap=True,
            color=PARCHMENT)
    element("notice", "", anchor=(0.5, 1), align=(0.5, 1), pos=(0, -148),
            size=(1000, 50), font_size=18, color=GOLD, text_align=(0.5, 0.5), wrap=True)
    element("tools_panel", kind=1, anchor=(0.5, 1), align=(0.5, 1),
            pos=(0, -68), size=(990, 68), background=TEAL)
    element("tools", "1 Field gun    2 Axe    3 Pickaxe    4 Shovel    5 Build",
            parent="tools_panel", pos=(15, 8), size=(960, 52),
            font_size=19, color=PARCHMENT,
            text_align=(0.5, 0.5), wrap=True)
    element("footer", "WASD Move   Space Jump   E Collect   Tab Inventory   Esc Pause", anchor=(0.5, 1),
            align=(0.5, 1), pos=(0, -18), size=(1180, 30), font_size=16,
            text_align=(0.5, 0.5), color=CREAM)

    menu = element("menu_bg", kind=1, parent="canvas", size=(0, 0),
                   background=(0.017, 0.043, 0.041, 0.88), visible=False)
    menu["anchorMax"] = [1, 1]
    element("menu_card", kind=1, parent="menu_bg", anchor=(0.5, 0.5), align=(0.5, 0.5),
            size=(660, 590), background=PARCHMENT)
    element("menu_stripe", kind=1, parent="menu_bg", anchor=(0.5, 0.5), align=(0.5, 0.5),
            pos=(0, -285), size=(660, 20), background=TEAL)
    element("menu_title", "Welcome to Hearthworld", parent="menu_bg", anchor=(0.5, 0.5),
            align=(0.5, 0.5), pos=(0, -225), size=(580, 48), font_size=30,
            color=INK, text_align=(0.5, 0.5))
    element("menu_body", "Find the tools in your cottage. Chop trees, mine stone, dig a pool\nand build a home on your own small planet.",
            parent="menu_bg", anchor=(0.5, 0.5), align=(0.5, 0.5), pos=(0, -124),
            size=(570, 126), font_size=19, color=INK, text_align=(0.5, 0.5), wrap=True)

    def button(identity, text, x, y, size=(260, 48), parent="menu_bg", visible=True):
        element(identity, text, kind=4, parent=parent, anchor=(0.5, 0.5),
                align=(0.5, 0.5), pos=(x, y), size=size, background=TEAL,
                font_size=21, text_align=(0.5, 0.5), color=CREAM, visible=visible)

    button("start", "Begin your homestead", 0, 4, (550, 56))
    button("resume", "Continue", 0, 4, (550, 56))
    button("save", "Save game", -145, 76)
    button("load", "Load game", 145, 76)
    button("quit", "Quit to desktop", 0, 146, (550, 48))
    element("menu_hint", "Your buildings, inventory and excavations can be saved.\nEscape closes this menu.",
            parent="menu_bg", anchor=(0.5, 0.5), align=(0.5, 0.5), pos=(0, 226),
            size=(570, 62), font_size=15, color=INK, text_align=(0.5, 0.5), wrap=True)

    inventory = element("inventory_bg", kind=1, size=(0, 0), visible=False,
                        background=(0.017, 0.043, 0.041, 0.86))
    inventory["anchorMax"] = [1, 1]
    element("inventory_card", kind=1, parent="inventory_bg", anchor=(0.5, 0.5), align=(0.5, 0.5),
            size=(760, 570), background=PARCHMENT)
    element("inventory_title", "FIELD PACK", parent="inventory_bg", anchor=(0.5, 0.5),
            align=(0.5, 0.5), pos=(0, -222), size=(660, 52), font_size=30,
            color=INK, text_align=(0.5, 0.5))
    element("inventory_body", "Your materials and equipment will appear here.", parent="inventory_bg",
            anchor=(0.5, 0.5), align=(0.5, 0.5), pos=(0, -12), size=(650, 360),
            font_size=17, color=INK, wrap=True, text_align=(0, 0))
    button("inventory_close", "Back to the world", 0, 222, (650, 48), parent="inventory_bg")
    output = ROOT / "Assets/ui/hud.judasui"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"kind": "ui", "schema": 1, "data": {
        "reference": [1280, 720], "visible": True, "enabled": True, "modal": False,
        "elements": elements}}, indent=2) + "\n")
    meta(output, UI_ID, "ui")


def make_sounds():
    folder = ROOT / "Assets/audio"
    folder.mkdir(parents=True, exist_ok=True)
    sample_rate = 22050
    manifest = {}
    for index, (name, identity) in enumerate(AUDIO_IDS.items()):
        rng = random.Random(9100 + index)
        duration = {"chop": .22, "dig": .25, "build": .21, "grab": .23,
                    "launch": .28, "pickup": .26, "stone": .19}[name]
        count = int(duration * sample_rate)
        samples = []
        last_noise = 0
        for n in range(count):
            t = n / sample_rate
            envelope = min(1, t * 1000) * math.exp(-t * (17 if name in ("chop", "build", "stone") else 12))
            noise = rng.uniform(-1, 1)
            last_noise = last_noise * .7 + noise * .3
            if name == "chop":
                value = .7 * math.sin(2 * math.pi * 130 * t) + .65 * last_noise
            elif name == "dig":
                value = .8 * last_noise + .1 * math.sin(2 * math.pi * 75 * t)
            elif name == "build":
                value = math.sin(2 * math.pi * 185 * t) + .3 * math.sin(2 * math.pi * 420 * t) + .35 * noise
            elif name == "grab":
                value = .7 * math.sin(2 * math.pi * (140 * t + 700 * t * t)) + .1 * last_noise
            elif name == "launch":
                value = .8 * math.sin(2 * math.pi * (240 * t - 300 * t * t)) + .5 * last_noise
            elif name == "pickup":
                value = .6 * math.sin(2 * math.pi * (660 if t < .09 else 880) * t) + .15 * math.sin(2 * math.pi * 1320 * t)
            else:
                value = .4 * math.sin(2 * math.pi * 950 * t) + .2 * math.sin(2 * math.pi * 1531 * t) + .55 * noise
            # A short terminal fade makes every bounded sound end smoothly.
            fade = min(1, (duration - t) / .012)
            samples.append(value * envelope * fade)
        peak = max(abs(v) for v in samples)
        pcm = b"".join(struct.pack("<h", round(v / peak * 23000)) for v in samples)
        path = folder / (name + ".wav")
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(sample_rate)
            output.writeframes(pcm)
        meta(path, identity, "audio")
        manifest[name] = {"id": identity, "path": "Assets/audio/" + path.name,
                          "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                          "sampleRate": sample_rate, "channels": 1, "frames": count}
    (folder / "PROVENANCE.json").write_text(json.dumps({
        "source": "Original deterministic procedural sound design for Hearthworld; no recordings or external samples",
        "sounds": manifest}, indent=2) + "\n")


if __name__ == "__main__":
    make_ui()
    make_sounds()
    print("Authored native HUD/menus and seven original PCM sound effects.")
