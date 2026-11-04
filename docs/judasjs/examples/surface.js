import * as api from 'judas';
export default class {
  /** @type {{surface:string[]}} */
  state={surface:[]};
  start(){
    for(const [key,value] of Object.entries(api)){
      this.state.surface.push(key);
      if(typeof value==='function'&&/^class /.test(Function.prototype.toString.call(value))){
        const args=key==='UtilityAgent'
          ? [{version:1,name:'API probe',actions:[{id:'idle'}]}]
          : key==='AISenses'||key==='AITargetReservations' ? [] : ['0','element'];
        const instance=Reflect.construct(value,args);
        for(const member of Object.getOwnPropertyNames(value.prototype))this.state.surface.push(key+'.'+member);
        for(const member of Object.keys(instance))this.state.surface.push(key+'.'+member);
        if(key==='UtilityAgent'||key==='AISenses'||key==='AITargetReservations')instance.dispose();
      }else if(value&&typeof value==='object')for(const member of Object.getOwnPropertyNames(value))this.state.surface.push(key+'.'+member);
    }
    this.state.surface=[...new Set(this.state.surface)].sort();
  }
}
