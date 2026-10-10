"""Author a compact inventory using existing panel, text and button primitives."""
import json
import re


def augment_inventory_ui(lines, font):
    """Append a hidden menu; no new runtime widget or native inventory is used."""
    result = [line for line in lines
              if not line.split('"', 2)[1].startswith('inventory')]
    templates = {line.split('"', 2)[1]: line for line in result}

    def clone(source, element_id, label='', x=0, y=0, width=712, height=40,
              font_size=18):
        tokens = re.findall(r'"(?:\\.|[^"\\])*"|\S+', templates[source])
        tokens[0] = json.dumps(element_id)
        tokens[1] = json.dumps('inventory')
        tokens[9:13] = ['0', '0', '0', '0']  # parent content top-left
        tokens[13:17] = list(map(str, (x, y, width, height)))
        tokens[19:21] = ['0', '0']
        tokens[40] = str(font_size)
        tokens[-7] = json.dumps(label)
        tokens[-5] = json.dumps(font)
        return ' '.join(tokens)

    # Free layout keeps list and description in two columns. All children stay
    # within the original 1280x720 reference canvas and scale with normal UI.
    panel = re.findall(r'"(?:\\.|[^"\\])*"|\S+', templates['pause'])
    panel[0] = '"inventory"'
    panel[3] = '0'
    panel[15:17] = ['760', '558']
    panel[-5] = json.dumps(font)
    result.append(' '.join(panel))
    result.extend([
        clone('pause_title', 'inventory_title', 'PACK & EQUIPMENT', height=36,
              font_size=26),
        clone('pause_hint', 'inventory_hint', 'C collect | I close', y=40,
              height=52, font_size=17),
    ])
    for index in range(6):
        result.append(clone('resume', f'inventory_row_{index}', '', y=100+index*44,
                            width=326, height=40, font_size=16))
    result.extend([
        clone('pause_hint', 'inventory_detail', '', x=350, y=100,
              width=362, height=194, font_size=18),
        clone('resume', 'inventory_equip', 'EQUIP SELECTED', x=350, y=310,
              width=362, height=42),
        clone('resume', 'inventory_drop', 'DROP ONE INTO THE TOWN', x=350,
              y=362, width=362, height=42),
        clone('resume', 'inventory_previous', 'PREVIOUS', y=374,
              width=130, height=30, font_size=16),
        clone('pause_title', 'inventory_page', '1 / 1', x=137, y=374,
              width=52, height=30, font_size=16),
        clone('resume', 'inventory_next', 'NEXT', x=196, y=374,
              width=130, height=30, font_size=16),
        clone('pause_hint', 'inventory_help', 'The pack pauses the round. Drop closes it and places one physical pickup in front of you.',
              y=414, height=42, font_size=16),
        clone('resume', 'inventory_close', 'BACK TO THE TOWN / I', y=466,
              height=36),
    ])
    return result
