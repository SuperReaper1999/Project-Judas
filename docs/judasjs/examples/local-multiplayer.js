import {players} from 'judas';
// Flat-arena acceleration example. Gravity/support remain owned by the motor;
// a real game decides acceleration limits, braking and presentation separately.
export default class {
 /** @param {import('judas').ScriptContext} context */
 constructor({entity}){this.entity=entity;/** @type {import('judas').LocalPlayer|null} */this.player=null;}
 start(){this.player=players.create('Local pilot');this.player.assign('keyboard',{controls:['key:W','key:A','key:S','key:D']});this.player.entity=this.entity;}
 fixedUpdate(){const motor=this.player?.entity?.character;if(motor&&this.player)motor.accelerate({x:this.player.input.axis('move_x')*12,y:0,z:-this.player.input.axis('move_y')*12});}
 destroy(){if(this.player?.valid)this.player.leave();}
}
