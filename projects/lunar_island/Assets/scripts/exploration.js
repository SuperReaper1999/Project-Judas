import { REGIONS } from './region-data.js';
import { length, sub, vec } from './math.js';
export function explore(player) {
    const state = (player.state.exploration ||= { visited: {}, target: null });
    for (const [key, d] of Object.entries(REGIONS)) {
        if (
            !d.hidden &&
            !state.visited[key] &&
            length(sub(player.entity.transform.position, vec(d.position))) < 105
        ) {
            state.visited[key] = true;
            player.say(
                'Discovered ' + d.name + '. Open M for locations and supplies.',
            );
        }
    }
}
export function destinationRows(player) {
    return Object.entries(REGIONS)
        .filter(([, d]) => !d.hidden)
        .map(([id, d]) => ({
            id,
            text: `${player.state.exploration?.visited[id] ? '✓ ' : ''}${d.name} · ${(length(sub(player.entity.transform.position, vec(d.position))) / 1000).toFixed(1)} km`,
        }));
}
export function destinationText(player, key) {
    const d = REGIONS[key];
    return d
        ? `${d.name.toUpperCase()}\n\n${d.description}\n\nLocal coordinates: ${d.center[0]} east / ${d.center[1]} south\n\n${d.theme === 'islet' ? 'Boat or aircraft required. Land on the open island approach.' : 'Follow the surfaced island roads. Detailed buildings and inhabitants stream as you approach.'}\n\nSelect Track location for a persistent distance guide. No teleport or fast travel.`
        : 'Choose a location.';
}
export function guidance(player) {
    const d = REGIONS[player.state.exploration?.target];
    if (!d) return '';
    const delta = sub(vec(d.position), player.entity.transform.position);
    return `${d.name} · ${Math.round(length(delta))} m · ${delta.x >= 0 ? 'E' : 'W'} / ${delta.z >= 0 ? 'S' : 'N'}`;
}
