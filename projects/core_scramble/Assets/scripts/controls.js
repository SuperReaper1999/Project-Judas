export const NAMES=['CYAN','CORAL','GOLD','VIOLET'];
export const COLORS=[[.16,.85,.77,1],[1,.39,.35,1],[1,.72,.16,1],[.65,.41,1,1]];
export const KEYS=[['W','S','A','D','Space','Left Shift','Tab'],['Up','Down','Left','Right','Right Shift','Right Ctrl','Keypad 0'],['I','K','J','L','O','U','P'],['T','G','F','H','Y','R','B']];
export function bindings(slot){const k=KEYS[slot],rows=[];
 const bind=(name,axis,controls)=>rows.push({name,axis,bindings:controls.map(([control,scale])=>({control,scale,deadzone:control.startsWith('stick:')?.18:0}))});
 bind('x',true,[['key:'+k[2],-1],['key:'+k[3],1],['stick:LeftX',1]]);
 bind('z',true,[['key:'+k[0],-1],['key:'+k[1],1],['stick:LeftY',1]]);
 for(const [name,key,pad]of[['shove',k[4],'South'],['jump',k[5],'West'],['pause',k[6],'Start'],['ui_up',k[0],'DpadUp'],['ui_down',k[1],'DpadDown'],['ui_activate',k[4],'South']])bind(name,false,[['key:'+key,1],['pad:'+pad,1]]);
 bind('ui_click',false,[['mouse:Left',1]]);return rows;
}
export function viewport(slot,count,portrait=false){if(count===2)return portrait?{x:0,y:.13+slot*.36,width:1,height:.36}:{x:slot*.5,y:.13,width:.5,height:.72};return {x:(slot%2)*.5,y:.13+Math.floor(slot/2)*.36,width:.5,height:.36};}
