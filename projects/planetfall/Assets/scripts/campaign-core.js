// Pure project rules: no native dispatch or engine gameplay types.
export const missions=[
 ['First Light','earth','talk','mara',1,'Mara Venn','You inherited more than a wreck. The people here need a reason to stay.',{scrap:4}],
 ['A Roof and a Promise','earth','build','shelter',1,'Mara Venn','Salvage the old street and build a shelter at the outpost.',{credits:100}],
 ['The Infected Road','earth-frontier','kill','road',4,'Ivo Rook','Clear the road. I will not send a caravan past the infected.',{scrap:8}],
 ['Stone Beneath the Scars','earth-highlands','mine','iron',8,'Dr Sen Vale','The quarry is exposed. Bring back iron, and watch for the blue seams.',{credits:120}],
 ['Power to the People','earth','build','generator',1,'Mara Venn','A generator turns a campsite into a settlement.',{fuel:6}],
 ['The Lost Survey','earth-coast','recover','survey',1,'Dr Sen Vale','Climb the research roof. The survey recorder survived the first shock.',{scrap:10}],
 ['Night at Lantern','earth','defend','outpost',6,'Ivo Rook','They are following the lights. Hold the outpost; barricades buy time.',{credits:220}],
 ['A Ship Worth Saving','earth','upgrade','ship',1,'Ada Quill','The Wayfarer needs a repaired drive before we can leave.',{fuel:10}],
 ['Trade Winds','earth-frontier','drive','caravan',1,'Ivo Rook','Take the rover to the western road marker. A convoy is waiting.',{credits:180}],
 ['Beneath the Fracture','earth-highlands','mine','aether',5,'Dr Sen Vale','The cave seam sings near powered equipment. Collect five samples.',{credits:160}],
 ['The Silent Relay','moon','recover','relay',1,'Ada Quill','Land at the abandoned lunar relay. Recover its navigation core.',{alloy:8}],
 ['Scavenger Rights','moon','kill','pirates',5,'Ivo Rook','Pirates are stripping the relay. Keep them away from our ship.',{credits:300}],
 ['Thin Air, Heavy Metal','mars','mine','alloy',10,'Ada Quill','Martian alloy will reinforce the outpost and the Wayfarer.',{fuel:12}],
 ['A Home That Holds','earth','build','defenses',1,'Mara Venn','Bring the alloy home. Build defenses before the next raid.',{credits:250}],
 ['The Red Archive','mars','recover','archive',1,'Dr Sen Vale','The research archive records who ordered the orbital experiment.',{scrap:12}],
 ['No One Left Behind','mars','rescue','survivor',1,'Mara Venn','A technician is trapped beyond the blocked passage. Break a way in.',{credits:250}],
 ['The Graveyard Run','derelict','recover','blackbox',1,'Ada Quill','Board the wreck and retrieve its flight recorder.',{alloy:12}],
 ['Boarding Party','derelict','kill','pirates',7,'Ivo Rook','Clear the scavengers. The recorder must reach Earth.',{credits:400}],
 ['The Last Siege','earth','defend','outpost',10,'Mara Venn','They know what we recovered. Hold Lantern one last time.',{aether:4}],
 ['What We Carry Forward','earth','choice','fracture',1,'Dr Sen Vale','The experiment fractured a gravity reference network. Contain the mineral, or use it to reconnect the scattered settlements?',{credits:1000}]
].map((m,i)=>({id:i+1,title:m[0],region:m[1],kind:m[2],target:m[3],need:m[4],npc:m[5],brief:m[6],reward:m[7]}));
export const recipes={shelter:{scrap:4},generator:{scrap:6,iron:8},defenses:{scrap:8,alloy:8},ship:{scrap:10,aether:0},rover:{iron:8,scrap:6}};
export function initialCampaign(){return {version:1,mission:0,progress:0,resources:{scrap:0,iron:0,aether:0,alloy:0,fuel:3,credits:0},built:[],collected:[],discovered:[],completed:[],choice:null,region:'earth',kills:0,health:100,hunger:100,food:5,deposits:{}};}
export function canAfford(s,cost){return Object.entries(cost).every(([k,v])=>(s.resources[k]||0)>=v);}
export function pay(s,cost){if(!canAfford(s,cost))return false;for(const [k,v]of Object.entries(cost))s.resources[k]-=v;return true;}
export function build(s,name){if(s.built.includes(name)||!recipes[name]||!pay(s,recipes[name]))return false;s.built.push(name);return true;}
export function collect(s,key,item,quantity){if(s.collected.includes(key)||!Number.isFinite(quantity)||quantity<=0)return false;s.collected.push(key);s.resources[item]=(s.resources[item]||0)+quantity;return true;}
export function credit(s,event){const m=missions[s.mission];if(!m||event.region!==m.region||event.kind!==m.kind||event.target!==m.target)return false;const n=event.amount??1;if(!Number.isInteger(n)||n<1||n>100)return false;s.progress=Math.min(m.need,s.progress+n);return true;}
export function complete(s){const m=missions[s.mission];if(!m||s.progress<m.need)return null;for(const[k,v]of Object.entries(m.reward))s.resources[k]=(s.resources[k]||0)+v;s.completed.push(m.id);s.mission++;s.progress=0;return m;}
