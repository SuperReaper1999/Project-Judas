import {world, entity, input, ui, players, saves} from 'judas';
import {add, mul, cross, norm, look} from './math.js';
export const properties = {paintPrefab: {type: 'string', default: ''}};
const v=(x=0,y=0,z=0)=>({x,y,z});
const COLORS=[v(.05,.85,.75),v(1,.1,.42),v(1,.75,.05),v(.55,.25,1)];
const NAMES=['TEAL','PINK','GOLD','VIOLET'];
const CELL=.09, MAX_MARKS=1024, STOP_MARKS=MAX_MARKS-4*96;
function rotate(q,p){const u=v(q.x,q.y,q.z),a=mul(cross(u,p),2);return add(p,add(mul(a,q.w),cross(u,a)));}
export default class GraffitiWorkshop {
    constructor({properties}) {
        this.properties=properties;
        this.state={time:0,step:0,yaw:0,pitch:0,color:0,viewId:null,cells:{},pending:[],
            deposited:0,rejected:0,launched:0,receiverTotals:{},colorTotals:[0,0,0,0],lastSequence:{}};
        this.paused=false;this.holdDelivery=false;this.savePending=false;this.request=null;
        this.fireCredit=0;this.uiAge=1;
    }
    start(){this.bind();}
    restore(){this.bind();}
    bind(){
        this.player=entity('2');this.emitters=[10,11,12,13].map(i=>entity(String(i)));
        this.doc=ui.get('graffiti');this.doc.modal=false;ui.debugOverlayVisible=false;
        this.view=this.state.viewId?players.get(this.state.viewId):null;
        if(!this.view?.valid){this.view=players.create('Graffiti artist');this.state.viewId=this.view.id;}
        this.paused=false;input.pointerCapture=true;
        for(const cell of Object.values(this.state.cells))this.showMark(cell);
        this.uiAge=1;
    }
    forward(){return v(-Math.sin(this.state.yaw)*Math.cos(this.state.pitch),Math.sin(this.state.pitch),-Math.cos(this.state.yaw)*Math.cos(this.state.pitch));}
    eye(){return add(this.player.presentedTransform.position,v(0,1.58,0));}
    showMark(cell){
        const receiver=entity(cell.receiver);if(!receiver?.valid)return;
        const t=receiver.transform,normal=rotate(t.rotation,cell.normal);
        const position=add(add(t.position,rotate(t.rotation,cell.point)),mul(normal,.012));
        let mark=cell.id?entity(cell.id):null;
        if(!mark?.valid){mark=world.spawnPrefab(this.properties.paintPrefab,{position});cell.id=mark.id;}
        mark.transform={position,rotation:look(mul(normal,-1),Math.abs(normal.y)>.9?v(0,0,1):v(0,1,0)),scale:v(.075,.075,.075)};
        mark.material().set({baseColor:{...cell.color,a:1},roughness:.75,doubleSided:true});
    }
    applyImpact(impact){
        const receiver=entity(impact.receiver);
        if(!receiver?.valid||!receiver.hasTag('paintable')){this.state.rejected+=impact.count;return true;}
        const p=impact.localPosition,n=impact.localNormal,color=v(impact.color.r,impact.color.g,impact.color.b);
        const key=[impact.receiver,impact.childKey,Math.round(n.x),Math.round(n.y),Math.round(n.z),Math.floor(p.x/CELL),Math.floor(p.y/CELL),Math.floor(p.z/CELL)].join(':');
        let cell=this.state.cells[key];
        if(!cell&&Object.keys(this.state.cells).length>=MAX_MARKS)return false;
        const before=cell?.count||0,total=before+impact.count;
        if(!cell)cell=this.state.cells[key]={receiver:impact.receiver,point:p,normal:n,color,count:0,id:null};
        cell.color=mul(add(mul(cell.color,before),mul(color,impact.count)),1/total);
        cell.point=mul(add(mul(cell.point,before),mul(p,impact.count)),1/total);cell.count=total;
        this.showMark(cell);
        this.state.deposited+=impact.count;
        this.state.receiverTotals[impact.receiver]=(this.state.receiverTotals[impact.receiver]||0)+impact.count;
        const colour=COLORS.findIndex(c=>Math.abs(c.x-color.x)+Math.abs(c.y-color.y)+Math.abs(c.z-color.z)<.001);
        if(colour>=0)this.state.colorTotals[colour]+=impact.count;
        return true;
    }
    drain(){
        if(this.holdDelivery)return;
        // Receiving state owns the detached native records before applying them.
        // If the authored mark budget fills, keep this pending batch durable and
        // stop draining/emitting; do not discard deposits or recycle old paint.
        if(!this.state.pending.length){
            for(const emitter of this.emitters){
                for(const impact of emitter.takeParticleImpacts()){
                    const last=this.state.lastSequence[emitter.id]||'0';
                    if(BigInt(impact.sequence)<=BigInt(last))throw new Error('duplicate particle delivery');
                    this.state.lastSequence[emitter.id]=impact.sequence;
                    this.state.pending.push(impact);
                }
            }
        }
        while(this.state.pending.length){
            if(!this.applyImpact(this.state.pending[0]))break;
            this.state.pending.shift();
        }
    }
    update(dt){
        if(input.pressed('pause')){this.paused=!this.paused;input.pointerCapture=!this.paused;}
        if(this.paused)return;
        this.state.yaw-=input.axis('look_x')*.0022;
        this.state.pitch=Math.max(-1.25,Math.min(1.25,this.state.pitch-input.axis('look_y')*.0022));
        for(let i=0;i<4;i++)if(input.pressed('color_'+(i+1)))this.state.color=i;
        if(input.pressed('delivery'))this.holdDelivery=!this.holdDelivery;
        if(input.pressed('save')){this.savePending=true;this.holdDelivery=false;}
        if(input.pressed('load')&&!this.savePending)this.request=saves.load('graffiti-workshop');
        if(input.pressed('clear')){
            for(const cell of Object.values(this.state.cells))if(cell.id&&entity(cell.id)?.valid)entity(cell.id).destroy();
            this.state.cells={}; // An explicit user-requested cleanup, not eviction.
        }
        this.uiAge+=dt;
        if(this.uiAge>.15){this.uiAge=0;this.hud();}
    }
    paintHash(){
        let h=2166136261;
        for(const key of Object.keys(this.state.cells).sort()){
            const c=this.state.cells[key],text=key+JSON.stringify([c.count,c.color,c.point,c.normal]);
            for(let i=0;i<text.length;i++)h=Math.imul(h^text.charCodeAt(i),16777619)>>>0;
        }
        return String(h);
    }
    fixedUpdate(dt){
        this.state.step++;this.state.time+=dt;
        const board=entity('103');board.setKinematicVelocity(v(Math.cos(this.state.time*.5)*.35,0,0),v());
        this.drain();
        const motor=this.player.character;
        const f=v(-Math.sin(this.state.yaw),0,-Math.cos(this.state.yaw)),r=cross(f,v(0,1,0));
        let direction=add(mul(f,input.axis('move_z')),mul(r,input.axis('move_x')));
        if(Math.hypot(direction.x,direction.z)>1)direction=norm(direction);
        const speed=input.held('sprint')?5.5:3.2,current=motor.velocity;
        motor.velocity=v(this.paused?0:direction.x*speed,current.y,this.paused?0:direction.z*speed);
        if(!this.paused&&input.pressed('jump')&&motor.state.supported)motor.velocity=add(motor.velocity,v(0,5,0));
        if(!this.paused&&!this.savePending&&!this.state.pending.length&&Object.keys(this.state.cells).length<STOP_MARKS&&input.held('spray')){
            this.fireCredit+=dt*90;
            const count=Math.min(6,Math.floor(this.fireCredit));this.fireCredit-=count;
            const forward=this.forward(),emitter=this.emitters[this.state.color];
            emitter.transform={position:add(add(this.eye(),mul(forward,.48)),add(mul(r,.15),v(0,-.13,0))),rotation:look(forward,v(0,1,0))};
            if(count&&emitter.burst(count))this.state.launched+=count;
        }else this.fireCredit=0;
        if(this.savePending&&this.emitters.every(e=>!e.particleStats.pendingStep&&e.particleStats.live===0)&&!this.state.pending.length){
            this.drain();this.savePending=false;this.request=saves.save('graffiti-workshop',{name:'My courtyard painting'});
        }
        if(this.state.step%30===0){
            const stats=this.emitters.map(e=>e.particleStats);
            console.log('GRAFFITI_STATE '+JSON.stringify({step:this.state.step,position:this.player.transform.position,color:this.state.color,
                deposited:this.state.deposited,rejected:this.state.rejected,launched:this.state.launched,cells:Object.keys(this.state.cells).length,
                paintHash:this.paintHash(),marks:world.queryTags(['paintmark']).length,colorTotals:this.state.colorTotals,receiverTotals:this.state.receiverTotals,live:stats.reduce((a,s)=>a+s.live,0),
                pending:stats.some(s=>s.pendingStep),queued:stats.reduce((a,s)=>a+s.queuedImpacts,0),held:this.holdDelivery,
                save:this.request?saves.status(this.request):null}));
        }
    }
    hud(){
        const stats=this.emitters.map(e=>e.particleStats),sum=k=>stats.reduce((a,s)=>a+(s[k]||0),0);
        let text='SPRAY: '+NAMES[this.state.color]+'  [1 teal / 2 pink / 3 gold / 4 violet]\n';
        text+='Deposited '+this.state.deposited+' | rejected by steel '+this.state.rejected+' | marks '+Object.keys(this.state.cells).length+'/'+MAX_MARKS;
        text+='\nDroplets live '+sum('live')+' | queued deposits '+sum('queuedImpacts')+' | query failures '+sum('queryFailures');
        if(this.paused)text+='\nPAUSED — Esc to resume';
        if(this.holdDelivery)text+='\nReceiver delivery HELD — B resumes; captured contacts are retained';
        if(Object.keys(this.state.cells).length>=STOP_MARKS)text+='\nPaint budget reached — C cleans; space reserved for airborne droplets';
        if(this.state.pending.length)text+='\nPaint budget full — C clears marks; pending deposits retained';
        if(this.savePending)text+='\nFinishing airborne droplets and reliable contacts before saving…';
        if(this.request){const op=saves.status(this.request);text+='\nSave/load: '+op.state+(op.error?' '+op.error:'');}
        this.doc.get('status').text=text;
    }
    presentationUpdate(){
        const eye=this.eye(),forward=this.forward(),r=cross(forward,v(0,1,0));
        this.view.setView({position:eye,rotation:look(forward,v(0,1,0))},70,{near:.06,far:100});
        const can=entity('30');can.transform={position:add(add(eye,mul(forward,.5)),add(mul(r,.27),v(0,-.3,0))),rotation:look(forward,v(0,1,0))};
        can.material().set({baseColor:{...COLORS[this.state.color],a:1},roughness:.35});
        // Only the moving receiver's marks need per-frame placement updates.
        for(const cell of Object.values(this.state.cells))if(cell.receiver==='103')this.showMark(cell);
    }
    destroy(){if(this.view?.valid)this.view.leave();input.pointerCapture=false;}
}
