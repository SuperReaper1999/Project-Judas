import {world} from 'judas';
export default class {
 /** @param {{entity:import('judas').Entity}} context */
 constructor({entity}) {
  this.entity=entity;
  this.state={changed:false,reverted:false,appearance:false,stale:false,
   visibility:false,whole:false,textureRemoval:false,lighting:false,partAvailable:false,part:false};
 }
 start() {
  const material=this.entity.material();
  material.set({roughness:.8,baseColor:{x:.1,y:.2,z:.3,a:1}});
  this.state.changed=material.state.overridden&&Math.abs(material.state.roughness-.8)<1e-6;
  material.clearOverrides();
  this.state.reverted=!material.state.overridden;

  // Entity, Render component and imported-part gates retain independent state.
  const entityVisible=this.entity.renderVisible;
  const rendererVisible=this.entity.rendererVisible;
  this.entity.renderVisible=false;
  this.entity.rendererVisible=false;
  this.entity.renderVisible=true;
  this.state.visibility=this.entity.renderVisible&&!this.entity.rendererVisible;
  this.entity.renderVisible=entityVisible;
  this.entity.rendererVisible=rendererVisible;

  // Whole-instance alpha is real blend coverage; an opaque alpha stays opaque.
  const whole=this.entity.material('*');
  whole.assign('');
  whole.set({alphaMode:'blend',alphaCutoff:.4,baseColor:{x:.3,y:.5,z:.8,a:.45},
   normalStrength:1.25,occlusionStrength:.8,doubleSided:true,
   textures:{normal:''}});
  this.state.whole=whole.state.overridden&&whole.state.alphaMode==='blend';
  this.state.textureRemoval=whole.state.textures.normal==='';
  whole.clearOverrides(); // Restores authored assignment as well as parameters.

  // Imported identities are exact stable keys, not generated mesh filenames.
  const parts=this.entity.modelParts;
  if(parts&&parts.length) {
   this.state.partAvailable=true;
   const part=this.entity.material(parts[0].identity);
   part.set({alphaMode:'mask',alphaCutoff:.35});
   this.state.part=part.state.overridden&&part.state.alphaMode==='mask';
   part.clearOverrides();
  }

  const initial=world.appearance;
  const changed=world.setAppearance({exposure:1.5});
  this.state.appearance=changed&&world.appearance.exposure===1.5;
  // Project policy supplies the direction toward the sun; exposure is separate.
  world.setAppearance({sunEnabled:!initial.sunEnabled,sunIntensity:2,
   sunDirection:{x:1,y:2,z:-1},sunColor:{x:1,y:.8,z:.6},
   ambientColor:{x:.03,y:.04,z:.06},environmentIntensity:.5});
  this.state.lighting=world.appearance.sunEnabled===!initial.sunEnabled
   &&world.appearance.sunIntensity===2;
  world.resetAppearance();
  world.setAppearance(initial); // Read-only status/error fields are ignored.

  const other=world.spawnPrefab("49494949494949494949494949494903",this.entity.transform);
  if(other) {
   const handle=other.material();
   other.destroy();
   try {handle.state;} catch(error) {this.state.stale=error instanceof ReferenceError;}
  }
 }
}
