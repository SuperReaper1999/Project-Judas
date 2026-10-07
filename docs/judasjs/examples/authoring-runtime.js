import {world,ui} from 'judas';
/** Run on an entity in World Workshop. These are generic presentation and
 * construction primitives; the project chooses markers and spawn meaning. */
export default class {
 /** @param {import('judas').ScriptContext} context */
 constructor({entity}){this.entity=entity;this.state={projected:false,behind:false,viewRay:false,layout:false,invalidLayout:false,spawned:false,invalidInit:false,invalidRange:false,camera:false,stale:false,first:'',second:''};}
 start(){
  world.setView({position:{x:0,y:80,z:0},rotation:{w:1,x:0,y:0,z:0}},70,{near:.15,far:2000});
  const p=world.project({x:0,y:80,z:-800});
  if(!p)return; // Viewport is supplied by the normal presentation host.
  this.state.projected=!!p?.inside&&Math.abs(p.x-.5)<1e-6&&Math.abs(p.y-.5)<1e-6;
  this.state.behind=world.project({x:0,y:80,z:10})?.behind===true;
  const ray=world.viewRay;this.state.viewRay=ray?.direction.z===-1&&ray.origin.y===80;
  this.hud=ui.load('67676767676767676767676700000004','api_example');
  const marker=this.hud.get('marker');
  marker.setLayout({anchorMin:{x:p.x,y:p.y},anchorMax:{x:p.x,y:p.y},offset:{x:12,y:8},size:{x:180,y:40}});
  const copy=marker.layout;copy.offset.x=999;
  this.state.layout=marker.layout.offset.x===12;
  try{marker.setLayout({size:{x:-1,y:3}});}catch(e){this.state.invalidLayout=e instanceof TypeError;}
  /** @param {number} serial @returns {import('judas').PrefabSpawnOptions} */
  const init=serial=>({velocity:{x:3,y:0,z:0},angularVelocity:{x:0,y:1,z:0},scripts:[{slot:1,properties:{label:`Example ${serial}`,owner:{entity:this.entity.id}},state:{serial}}]});
  this.first=world.spawnPrefab('67676767676767676767676700000003',{position:{x:10,y:4,z:0}},init(1));
  this.second=world.spawnPrefab('67676767676767676767676700000003',{position:{x:14,y:4,z:0}},init(2));
  this.state.spawned=this.first.id!==this.second.id;
  this.state.first=this.first.id;this.state.second=this.second.id;
  try{world.spawnPrefab('67676767676767676767676700000003',{}, {scripts:[{slot:99,state:{}}]});}catch(e){this.state.invalidInit=e instanceof TypeError;}
  try{world.setView({position:{x:0,y:0,z:0},rotation:{w:1,x:0,y:0,z:0}},70,{near:3,far:2});}catch(e){this.state.invalidRange=e instanceof TypeError;}
  const camera=world.entity('9');if(camera){camera.setCameraProjection({near:.2,far:1500});this.state.camera=camera.camera?.far===1500;}
  const stale=this.hud.get('marker');this.hud.unload();
  try{stale.layout;}catch(e){this.state.stale=e instanceof ReferenceError;}
 }
 destroy(){world.clearView();}
}
