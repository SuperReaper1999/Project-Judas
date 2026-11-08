import {input, ui, world, scenes, saves, physics} from 'judas';
import geography from './geography.js';

const add = (a,b) => ({x:a.x+b.x,y:a.y+b.y,z:a.z+b.z});
const mul = (a,s) => ({x:a.x*s,y:a.y*s,z:a.z*s});
const dot = (a,b) => a.x*b.x+a.y*b.y+a.z*b.z;
const length = a => Math.hypot(a.x,a.y,a.z);
const unit = a => length(a)>1e-8 ? mul(a,1/length(a)) : {x:0,y:1,z:0};
const cross = (a,b) => ({x:a.y*b.z-a.z*b.y,y:a.z*b.x-a.x*b.z,z:a.x*b.y-a.y*b.x});
const qmul = (a,b) => ({w:a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
    x:a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
    y:a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
    z:a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w});
const rotate = (q,v) => {const u={x:q.x,y:q.y,z:q.z};return add(v,add(mul(cross(u,v),2*q.w),mul(cross(u,cross(u,v)),2)));};
const look = (yaw,pitch) => {
    const cy=Math.cos(yaw/2),sy=Math.sin(yaw/2),cp=Math.cos(pitch/2),sp=Math.sin(pitch/2);
    return {w:cy*cp,x:cy*sp,y:sy*cp,z:-sy*sp};
};
const point = p => ({x:p[0],y:p[1],z:p[2]});

export default class PlanetExplorer {
    constructor({entity}) {
        this.entity=entity;
        this.state={yaw:0,pitch:-.05,tool:'dig',layer:2,surfaceDabs:0,queue:[]};
    }
    start() {
        this.owner=world.entity('1');
        this.planet=world.entity('3');
        this.hud=ui.get('game_ui');
        this.ready=false;
        ui.debugOverlayVisible=false;
        input.pointerCapture=true;
        if(!this.owner.terrain.state?.ready)throw new Error('Projected native terrain did not initialize.');
        if(this.hud) {
            this.hud.modal=false;
            for(const id of ['main','options','pause','image'])this.hud.get(id).visible=false;
            this.hud.get('hud').visible=true;
            this.hud.get('hud_title').text='TERRAIN V2 / PLANETARY WORKSHOP';
            this.hud.get('hud_help').text='WASD + mouse: explore | Space: jump | T: tunnel\n1: dig | 2: raise | 3: lower | 4: paint | 5: next layer\nLeft/right click: tool / add | O: origin shift\nF6/F7: save/load | R: reset | Esc: pause';
        }
    }
    restore() {
        this.start();
        console.log('TERRAIN_V2_RESTORED '+JSON.stringify({position:this.entity.transform.position,
            terrain:this.owner.terrain.state,surfaceDabs:this.state.surfaceDabs,remainingBrushes:this.state.queue.length,origin:world.origin}));
    }
    frame() {
        const up=unit(mul(physics.gravity(this.entity.transform.position),-1));
        const axis=cross({x:0,y:1,z:0},up), w=1+up.y;
        if(w<1e-8)return {w:0,x:1,y:0,z:0};
        const scale=1/Math.hypot(w,axis.x,axis.y,axis.z);
        return {w:w*scale,x:axis.x*scale,y:axis.y*scale,z:axis.z*scale};
    }
    orientation() {return qmul(this.frame(),look(this.state.yaw,this.state.pitch));}
    presentationUpdate() {
        const up=rotate(this.frame(),{x:0,y:1,z:0});
        world.setView({position:add(this.entity.presentedTransform.position,mul(up,.65)),
                       rotation:this.orientation()},70,{near:.08,far:2200});
    }
    update() {
        if(this.hud?.modal)return;
        this.ready=this.planet.preparePlanetCollision(world.toAbsolute(this.entity.transform.position));
        this.state.yaw-=input.axis('look_x')*.0021;
        this.state.pitch=Math.max(-1.45,Math.min(1.45,this.state.pitch-input.axis('look_y')*.0021));
        if(input.pressed('reset'))scenes.reload();
        if(input.pressed('origin_shift')){world.requestOrigin(world.toAbsolute(this.entity.transform.position));console.log('TERRAIN_V2_ORIGIN_REQUEST '+JSON.stringify(world.originState));}
        for(const tool of ['dig','raise','lower','paint'])if(input.pressed('mode_'+tool))this.state.tool=tool;
        if(input.pressed('paint_layer'))this.state.layer=(this.state.layer+1)%4;
        if(input.pressed('save')) {
            console.log('TERRAIN_V2_SAVE_REQUEST '+JSON.stringify({remainingBrushes:this.state.queue.length,busy:this.owner.terrain.state.busy}));
            this.request=saves.save('planetary');
        }
        if(input.pressed('load'))this.request=saves.load('planetary');
        // Store absolute brush positions so pending gameplay gestures survive an
        // origin transaction. Geometry/quantity still belong to native terrain.
        if(input.pressed('tunnel')&&!this.state.queue.length)
            for(const p of geography.tunnel)this.state.queue.push({point:point(p),kind:'remove',radius:1.65});
        if(input.pressed('dig')||input.pressed('build')) {
            const origin=add(this.entity.transform.position,rotate(this.frame(),{x:0,y:.65,z:0}));
            const direction=rotate(this.orientation(),{x:0,y:0,z:-1});
            const hit=physics.raycast(origin,direction,7,{ignored:[this.entity]});
            if(hit?.entity?.id===this.owner.id) {
                const surface=!input.pressed('build')&&this.state.tool!=='dig';
                this.state.queue.push({point:world.toAbsolute(hit.point),surface,
                    kind:surface?this.state.tool:(input.pressed('build')?'add':'remove'),
                    radius:surface?2:1.35,layer:this.state.layer});
            }
        }
        const s=this.owner.terrain.state;
        if(s?.ready&&!s.busy&&this.state.queue.length) {
            const b=this.state.queue[0], p=world.toLocal(b.point);
            if(b.surface) {
                if(this.owner.terrain.surfaceBrush(p,{kind:b.kind,radius:b.radius,
                    strength:b.kind==='paint'?1:.25,layer:b.layer})) {
                    this.state.queue.shift();this.state.surfaceDabs++;
                    console.log('TERRAIN_V2_SURFACE_DAB '+b.kind);
                }
            } else if(this.owner.terrain.brush(p,{kind:b.kind,radius:b.radius}))this.state.queue.shift();
        }
    }
    fixedUpdate(dt) {
        const motor=this.entity.character;
        if(!motor)return;
        if(!this.ready){motor.velocity={x:0,y:0,z:0};return;}
        const up=rotate(this.frame(),{x:0,y:1,z:0});
        let direction=rotate(qmul(this.frame(),look(this.state.yaw,0)),
                             {x:input.axis('move_x'),y:0,z:-input.axis('move_y')});
        if(length(direction)>1)direction=unit(direction);
        let velocity=motor.velocity;
        const tangent=add(velocity,mul(up,-dot(up,velocity)));
        const change=add(mul(direction,4),mul(tangent,-1));
        if(length(change)>1e-6)velocity=add(velocity,mul(unit(change),Math.min(length(change),dt*(motor.state.supported?28:10))));
        if(input.pressed('jump')&&motor.state.supported)velocity=add(velocity,mul(up,5-dot(up,velocity)));
        motor.velocity=velocity;
    }
    uiUpdate() {
        if(!this.hud)return;
        if(input.pressed('pause')){this.hud.modal=!this.hud.modal;this.hud.get('pause').visible=this.hud.modal;}
        input.pointerCapture=!this.hud.modal;
        this.hud.get('crosshair').visible=!this.hud.modal;
        const s=this.owner.terrain.state, p=this.planet.planetState;
        let text=`${s.ready?'Ready':'Preparing'} | ${s.chunks} chunks / ${s.volumeChunks} excavated\n`+
            `Prepare ${s.prepareMs.toFixed(2)} ms (worker) | publish ${s.publishMs.toFixed(2)} ms\n`+
            `Planet collision ${p.collisionReady?'ready':'preparing'} | origin revision ${world.originState.revision}\n`+
            `Tool: ${this.state.tool} | layer ${this.state.layer+1} | ${this.state.queue.length} cuts queued\n`+
            (s.error||p.error||'Press T, wait for cuts, then walk through the hill.');
        if(this.request){const r=saves.status(this.request);text+='\nSave/load: '+r?.state+' '+(r?.error||'');}
        this.hud.get('counter').text=text;
    }
    onUI(e) {
        if(e.type==='back'||(e.type==='click'&&e.element==='resume')){this.hud.modal=false;this.hud.get('pause').visible=false;}
        if(e.type==='click'&&e.element==='pause_options')scenes.reload();
        if(e.type==='click'&&e.element==='pause_quit')ui.quit();
    }
    destroy(){input.pointerCapture=false;world.clearView();}
}
