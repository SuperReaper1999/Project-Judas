import {scenes,session} from 'judas';
/** Run on a persistent root entity of a streaming scene that declares gallery-0. */
export default class {
 constructor(){this.token='';this.state={active:false};}
 start(){this.token=scenes.requestRegion('gallery-0',{preload:true});session.set('streamExample',0);}
 update(){const status=scenes.regionStatus(this.token);
  if(status?.state==='prepared')scenes.activateRegion(this.token);
  if(status?.state==='active'){this.state.active=true;session.set('streamExample',1);}
 }
 destroy(){if(this.token)scenes.releaseRegion(this.token);}
}
