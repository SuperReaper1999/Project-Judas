"""Author the supply shop using the same ordinary UI primitives as pause."""
import json
import re


def augment_ui(lines, font):
    """Append a hidden shop panel; player JS owns visibility, prices and actions."""
    owned = {'shop', 'shop_title', 'shop_hint', 'buy_rockets', 'buy_grenades',
             'buy_heal', 'shop_close'}
    result = [line for line in lines if line.split('"', 2)[1] not in owned]
    templates = {line.split('"', 2)[1]: line for line in result}

    def clone(source, element_id, parent, label=None):
        # Keep the authored primitive/layout fields exactly as in the pause UI.
        # Its quoted strings are tokenized separately so labels may contain spaces.
        tokens = re.findall(r'"(?:\\.|[^"\\])*"|\S+', templates[source])
        tokens[0] = json.dumps(element_id)
        tokens[1] = json.dumps(parent)
        tokens[-5] = json.dumps(font)
        if label is not None:
            tokens[-7] = json.dumps(label)
        return ' '.join(tokens)

    result += [
        clone('pause', 'shop', 'canvas'),
        clone('pause_title', 'shop_title', 'shop', 'NEIGHBOURHOOD SUPPLIES'),
        clone('pause_hint', 'shop_hint', 'shop',
              'Spend earned points. Buy ammunition or restore health, then return to the fight.'),
        clone('resume', 'buy_rockets', 'shop', '2 ROCKETS / 250 POINTS'),
        clone('resume', 'buy_grenades', 'shop', '2 GRENADES / 150 POINTS'),
        clone('resume', 'buy_heal', 'shop', 'RESTORE HEALTH / 100 POINTS'),
        clone('resume', 'shop_close', 'shop', 'BACK TO THE TOWN'),
    ]
    return result
