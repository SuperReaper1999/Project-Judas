import {source} from './source.js';
export default class {constructor({entity}){this.entity=entity} fixedUpdate(){if(!this.entity.terrain.state)this.entity.terrain.create({source,navigation:true})}}
