import { vehicleByReference } from './vehicle-session.js';
import { weight } from './inventory.js';
export function renderVehicle(hud) {
    const v = vehicleByReference(hud.vehicle),
        d = hud.doc;
    if (!v) return 'Vehicle unavailable.';
    d.get('title').text = v.def.name.toUpperCase();
    d.get('primary').text = 'Drive / pilot';
    d.get('primary').enabled = v.def.kind !== 'depot' && v.state.health > 0;
    d.get('secondary').text = 'Open cargo';
    d.get('tertiary').text = 'Refuel · 5 L can';
    d.get('drop').text = 'Repair · use kit';
    d.get('tertiary').enabled = d.get('drop').enabled = v.def.kind !== 'depot';
    return `${v.def.kind === 'depot' ? 'FINITE SERVICE SUPPLIES' : v.def.kind.toUpperCase()}\n\nFuel ${v.state.fuel.toFixed(1)} / ${v.def.tank} L\nCondition ${v.state.health.toFixed(0)}%\nCargo ${weight(v.state.bag).toFixed(1)} / ${v.def.capacity} kg · ${v.state.bag.slots.filter(Boolean).length}/${v.def.slots} slots\n\nStop beside the vehicle to transfer supplies. Refuelling uses one sealed 5 L can from your backpack. Repairs consume one repair kit and restore up to 35 condition. Cargo remains accessible if wrecked.`;
}
export function vehicleEvent(hud, e) {
    if (
        hud.page !== 'vehicle' ||
        !['primary', 'secondary', 'tertiary', 'drop'].includes(e.element)
    )
        return false;
    const p = hud.p;
    if (e.element === 'primary') p.transport.requestEnter(hud.vehicle);
    if (e.element === 'secondary') p.transport.openCargo(hud.vehicle);
    if (e.element === 'tertiary') p.transport.service(hud.vehicle, 'fuel');
    if (e.element === 'drop') p.transport.service(hud.vehicle, 'repair');
    return true;
}
