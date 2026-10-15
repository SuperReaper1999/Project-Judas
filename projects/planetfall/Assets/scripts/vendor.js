import {world,input} from 'judas';import {game} from './game.js';import {length,sub} from './math.js';
export default class Vendor {
 constructor(player){this.player=player;this.near=false;}
 tick(){this.near=world.queryTags(['vendor']).some(e=>e.valid&&length(sub(e.transform.position,this.player.entity.transform.position))<3.6);
  if(this.near&&input.pressed('vendor')&&!this.player.climb.active&&!this.player.grab.held)this.player.hud.shop(true);
 }
 buy(item){let price=0;if(item==='buy_rockets')price=250;else if(item==='buy_grenades')price=150;else if(item==='buy_heal')price=100;else return;
  if(game.points<price)return;
  if(item==='buy_rockets'&&game.rockets>10||item==='buy_grenades'&&game.grenades>10||item==='buy_heal'&&game.health>=100)return;
  game.points-=price;if(item==='buy_rockets')game.rockets=Math.min(12,game.rockets+2);else if(item==='buy_grenades')game.grenades=Math.min(12,game.grenades+2);else game.health=Math.min(100,game.health+50);
  this.player.hud.doc.get('shop_hint').text=`${game.points} POINTS / Rockets ${game.rockets} / Grenades ${game.grenades}`;
 }
}
