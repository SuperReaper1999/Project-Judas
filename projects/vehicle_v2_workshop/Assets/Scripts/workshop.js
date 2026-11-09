import {world,input,ui,saves,scenes} from 'judas';
import {driveVehicle,chaseVehicle,wheelsOf} from './vehicle-driver.js';
import {vehicles} from './layout.js';

export default class {
    constructor(){this.state={selected:0};this.request=null;this.notice='';}
    start(){this.hud=ui.get('vehicle_workshop');this.hud.modal=false;}
    restore(){this.start();this.notice='Restored saved vehicle and drivetrain state.';}
    uiUpdate(){
        if(input.pressed('pause'))this.hud.modal=!this.hud.modal;
        for(let n=0;n<vehicles.length;n++)if(input.pressed('select_'+(n+1)))this.state.selected=n;
        if(input.pressed('reset'))scenes.reload();
        if(!saves.busy){
            if(input.pressed('save'))this.request=saves.save('vehicle-workshop');
            if(input.pressed('load'))this.request=saves.load('vehicle-workshop');
        }
        const chosen=vehicles[this.state.selected],chassis=world.entity(chosen.id);
        const wheels=wheelsOf(chassis),states=wheels.map(entity=>entity.wheel.state);
        const speed=Math.hypot(...Object.values(chassis.velocity));
        const loads=states.map(state=>Math.round(state.normalForce));
        const drive=chassis.powertrain?.state;
        const status=this.request?saves.status(this.request):null;
        this.hud.get('telemetry').text='VEHICLES V2 | '+chosen.name+(this.hud.modal?' | PAUSED':'')+'\n'+
            'W/S drive / reverse | A/D steer | Space brake | 1–4 change vehicle | K/L save/load | R restart | Esc pause\n'+
            'Speed '+(speed*3.6).toFixed(1)+' km/h | Support '+states.filter(s=>s.contact).length+'/'+states.length+
            ' | '+(states.every(s=>s.sleeping)?'SLEEPING':'ACTIVE')+' | Loads N: '+loads.join(' / ')+'\n'+
            'Grip: '+states.map(s=>s.gripUtilization.toFixed(2)).join(' / ')+' | Slip: '+states.map(s=>s.slipRatio.toFixed(2)).join(' / ')+'\n'+
            (drive?'Drivetrain '+(drive.converged?'converged':'FAILED: '+drive.diagnostic)+' | Residual '+drive.residual.toExponential(1):'Unpowered')+
            ' | '+(status?'Save/load: '+status.state+' '+(status.error||''):this.notice||'Ramp ahead; rough strips on the right; loose crates on the left.');
    }
    fixedUpdate(){
        const selected=vehicles[this.state.selected];
        for(const vehicle of vehicles)for(const id of vehicle.rig){
            const active=selected.id===vehicle.id;
            const isTractor=id===vehicle.id;
            driveVehicle(world.entity(id),active&&isTractor?input.axis('vehicle_throttle'):0,
                active&&isTractor?-input.axis('vehicle_steer'):0,
                !active||input.held('vehicle_brake')?vehicle.brakeTorque:0);
        }
    }
    presentationUpdate(){chaseVehicle(world.entity(vehicles[this.state.selected].id));}
    destroy(){world.clearView();}
}
