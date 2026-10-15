import {frames} from './regions.js';import {rotate} from './math.js';
// Authored chart. Absolute origins in each scene are double precision; all
// walking, contacts and ship forces use that scene's local metres.
export const destinations=[
 {name:'EARTH / LANTERN',scene:'Scenes/earth.judas',center:{x:0,y:-796375,z:0},radius:796375,dock:{x:0,y:.18,z:48}},
 {name:'EARTH / ROOK FRONTIER',scene:'Scenes/earth-frontier.judas',frame:frames['earth-frontier'],center:rotate(frames['earth-frontier'],{x:0,y:-796375,z:0}),radius:796375,dock:rotate(frames['earth-frontier'],{x:0,y:.18,z:48})},
 {name:'EARTH / FRACTURE HIGHLANDS',scene:'Scenes/earth-highlands.judas',frame:frames['earth-highlands'],center:rotate(frames['earth-highlands'],{x:0,y:-796375,z:0}),radius:796375,dock:rotate(frames['earth-highlands'],{x:0,y:.18,z:48})},
 {name:'EARTH / GLASS COAST',scene:'Scenes/earth-coast.judas',frame:frames['earth-coast'],center:rotate(frames['earth-coast'],{x:0,y:-796375,z:0}),radius:796375,dock:rotate(frames['earth-coast'],{x:0,y:.18,z:48})},
 {name:'LUNA / SILENT RELAY',scene:'Scenes/moon.judas',center:{x:0,y:-217175,z:0},radius:217175,dock:{x:0,y:35,z:48}},
 {name:'MARS / RED ARCHIVE',scene:'Scenes/mars.judas',center:{x:0,y:-423687.5,z:0},radius:423687.5,dock:{x:0,y:35,z:48}},
 {name:'ORBIT / DERELICT',scene:'Scenes/derelict.judas',center:{x:0,y:-796375,z:0},radius:796375,dock:{x:0,y:35,z:48}}
];
