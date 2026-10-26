// Reuses the ordinary native modal/list/buttons; no engine-owned dialogue rules.
import { actors } from './people-session.js';
import { ITEMS } from './definitions.js';
import { PEOPLE } from './encounter-data.js';
import { count } from './inventory.js';
export const socialPage = (page) =>
    ['dialogue', 'trade', 'corpse'].includes(page);
export const speaker = (hud) =>
    [...actors.values()].find((a) => a.key === hud.person);
export function socialRows(hud) {
    const a = speaker(hud);
    if (!a) return [];
    if (hud.page === 'trade')
        return Object.keys(a.def.prices || {}).map((id) => ({
            id,
            text: `${ITEMS[id].name} · ${a.def.prices[id]} tokens · stock ${count(a.state.bag, id)}`,
        }));
    if (hud.page === 'corpse')
        return a.state.bag.slots.flatMap((s, i) =>
            s
                ? [{ id: String(i), text: `${ITEMS[s.item].name} ×${s.count}` }]
                : [],
        );
    return (a.def.lines || []).map((text, i) => ({
        id: String(i),
        text:
            ['About the island', 'Supplies and advice', 'Making a home'][i] ||
            'Talk',
    }));
}
export function renderSocial(hud) {
    const a = speaker(hud),
        d = hud.doc,
        p = hud.p;
    if (!a) {
        hud.voice.stop();
        return 'This person is no longer available.';
    }
    d.get('drop').visible = false;
    d.get('tertiary').visible = false;
    d.get('primary').enabled = a.near;
    d.get('secondary').enabled = a.near;
    d.get('title').text = a.def.name.toUpperCase();
    if (hud.page === 'trade') {
        d.get('primary').text = 'Buy one';
        d.get('secondary').text = 'Sell one';
        d.get('tertiary').visible = true;
        d.get('tertiary').text =
            ITEMS[hud.selected]?.category === 'ammunition'
                ? 'Buy 10 rounds'
                : 'Back to dialogue';
        const price = a.def.prices[hud.selected];
        return `Your tokens: ${count(p.state.bag, 'tokens')}\nTrader tokens: ${count(a.state.bag, 'tokens')}\n\n${ITEMS[hud.selected]?.name || 'Choose an item'}\nBuy: ${price || 0} · Sell: ${Math.max(1, Math.floor((price || 0) * 0.5))}\nYou carry: ${count(p.state.bag, hud.selected)}\n\nStock, capacity and money are finite. Failed trades spend nothing.`;
    }
    if (hud.page === 'corpse') {
        d.get('primary').text = 'Take selected stack';
        d.get('secondary').visible = false;
        return 'Recover supplies from the physical remains. Each stack transfers once; contents and depletion survive saves.\n\nPack space and weight still apply.';
    }
    d.get('primary').text = 'Task / collect reward';
    d.get('secondary').text = 'Trade supplies';
    hud.voice.show(a.key, Number(hud.selected) || 0);
    const q = PEOPLE.quests[a.def.quest],
        status = q ? p.state.combat.quests[q.id] || 'not accepted' : '';
    return (
        (a.def.lines[Number(hud.selected) || 0] || a.def.lines[0]) +
        '\n\n' +
        (q
            ? `${q.name.toUpperCase()}: ${status}\n${q.description}${q.objective.counter ? ' · ' + (p.state.combat[q.objective.counter] || 0) + '/' + q.objective.count : ''}`
            : 'No task available.') +
        '\n\nAsk about work, trade, or choose another topic. Conversations pause the world.'
    );
}
export function socialEvent(hud, e) {
    if (!socialPage(hud.page)) return false;
    const a = speaker(hud);
    if (!a) return false;
    if (e.element === 'primary') {
        if (hud.page === 'trade') a.trade(hud.p, hud.selected, true);
        else if (hud.page === 'corpse') a.loot(hud.p, Number(hud.selected));
        else hud.p.say(a.offer(hud.p));
    } else if (e.element === 'secondary') {
        if (hud.page === 'trade') a.trade(hud.p, hud.selected, false);
        else if (hud.page === 'dialogue') hud.open('trade');
    } else if (e.element === 'tertiary' && hud.page === 'trade') {
        if (ITEMS[hud.selected]?.category === 'ammunition')
            a.trade(hud.p, hud.selected, true, 10);
        else hud.open('dialogue');
    } else return false;
    hud.refresh();
    return true;
}
