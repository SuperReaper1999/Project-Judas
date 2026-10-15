import {scenes} from 'judas';
import {rotate} from './math.js';
export const frames={earth:{w:1,x:0,y:0,z:0},'earth-frontier':{w:.9238795325,x:.3826834324,y:0,z:0},'earth-highlands':{w:.9659258263,x:0,y:0,z:.2588190451},'earth-coast':{w:.8660254038,x:0,y:0,z:-.5},moon:{w:1,x:0,y:0,z:0},mars:{w:1,x:0,y:0,z:0},derelict:{w:1,x:0,y:0,z:0}};
export const region=()=>scenes.current.split('/').pop().replace('.judas','');
export const frame=()=>frames[region()]||frames.earth;
export const location=p=>rotate(frame(),p);
