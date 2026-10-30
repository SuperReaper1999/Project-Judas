import {session} from 'judas';
export default class {
 constructor({entity}){this.entity=entity;this.state={collected:false};}
 onTriggerEnter(event){if(!this.state.collected&&event.other?.body2D?.kind==='capsule'){this.state.collected=true;this.entity.renderVisible=false;session.set('stars',(session.get('stars')??0)+1);}}
}