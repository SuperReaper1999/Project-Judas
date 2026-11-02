import {ui, input, scenes} from 'judas';

const POOL = 'Scenes/rectangular_pool.judas';
const AWKWARD = 'Scenes/awkward_planet.judas';

export default class FluidComparison {
    start() {
        this.hud = ui.get('game_ui');
        this.hud.modal = false;
        ui.debugOverlayVisible = false;
        for (const id of ['main', 'options', 'pause', 'image'])
            this.hud.get(id).visible = false;
        this.hud.get('hud').visible = true;
        const reference = scenes.current === POOL;
        this.hud.get('hud_title').text = reference ?
            'BIG RECTANGULAR POOL' : 'ORIGINAL AWKWARD BASIN';
        this.hud.get('counter').text = reference ?
            '12 x 8 m | 2 m initial water | uniform gravity\nDensity: green 500 / blue 1000 / grey 2000 kg/m3\nSteps at the near end; clear side walls.' :
            'Original Terrain planet / radial gravity\nOriginal terrain, props, lake and emitter.\nB adds water; F controls the spacecraft.';
        this.hud.get('hud_help').text =
            'WASD + mouse: move/look | Space: jump/swim\nG: grab/drop | H: throw | V: change view\nF1: big pool | F2: awkward planet | R: reset';
        this.hud.get('pause_options').text = reference ?
            'Visit awkward basin (F2)' : 'Visit big pool (F1)';
    }

    uiUpdate() {
        if (input.pressed('fluid_pool')) scenes.load(POOL);
        else if (input.pressed('fluid_planet')) scenes.load(AWKWARD);
        if (input.pressed('pause')) {
            this.hud.modal = !this.hud.modal;
            this.hud.get('pause').visible = this.hud.modal;
        }
    }

    onUI(event) {
        if (event.type === 'back' || (event.type === 'click' && event.element === 'resume')) {
            this.hud.modal = false;
            this.hud.get('pause').visible = false;
        }
        if (event.type === 'click' && event.element === 'pause_options')
            scenes.load(scenes.current === POOL ? AWKWARD : POOL);
        if (event.type === 'click' && event.element === 'pause_quit') ui.quit();
    }
}
