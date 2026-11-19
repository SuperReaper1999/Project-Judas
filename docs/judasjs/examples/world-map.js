import {ui, world, WorldMap} from 'judas';

export const properties = {mapUi: {type: 'string', default: ''}};

const region = {min: {x: -16, z: -16}, max: {x: 16, z: 16}, orientation: 0};
const waypoint = {x: 4, z: -4};

export default class {
    constructor({entity, properties}) {
        this.entity = entity;
        this.properties = properties;
    }
    start() {
        this.document = ui.load(this.properties.mapUi, 'world_map');
        this.full = new WorldMap(this.document.get('overview'), region);
        this.local = new WorldMap(this.document.get('minimap'), region);
        this.full.placeMarker(this.document.get('overview_pin'), waypoint);
    }
    uiUpdate() {
        const player = world.toAbsolute(this.entity.transform.position);
        this.local.setView({center: player, width: 16, height: 16, heading: 0, circular: true});
        this.local.placeMarker(this.document.get('minimap_player'), player);
        this.local.placeMarker(this.document.get('minimap_pin'), waypoint);
    }
    destroy() {
        this.document?.unload();
    }
}
