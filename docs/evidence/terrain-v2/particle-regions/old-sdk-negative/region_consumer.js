export default class {
 constructor({entity}) { this.entity=entity; this.state={contacts:0,reported:false}; }
 require(value,message) { if(!value)throw new Error(message); }
 start() {
  const p=this.entity.particles;
  this.require(p.collisionRegion&&p.collisionRegionOffset.x===1&&p.collisionRegionHalfExtents.x===8,'authored region');
  this.require(this.entity.setParticles({collisionRegionOffset:{x:0,y:-4,z:0},
    collisionRegionHalfExtents:{x:12,y:16,z:12}}),'region patch');
  this.require(this.entity.particles.collisionRegionHalfExtents.y===16,'region readback');
  this.require(this.entity.burst(1),'burst');
 }
 fixedUpdate() {
  for(const hit of this.entity.takeParticleImpacts()) {
   this.require(hit.receiver==='1','logical terrain receiver');
   this.state.contacts+=hit.count;
  }
  if(this.state.contacts&&!this.state.reported&&this.entity.particleStats.live===0) {
   this.state.reported=true;
   console.log('REGION_CONSUMER_CONTACT '+JSON.stringify({contacts:this.state.contacts,stats:this.entity.particleStats}));
  }
 }
 restore() {
  const p=this.entity.particles;
  this.require(p.collisionRegion&&p.collisionRegionOffset.y===-4&&p.collisionRegionHalfExtents.y===16,'restored region');
  this.require(this.state.contacts===1,'restored game-owned contact counter');
  console.log('REGION_CONSUMER_RESTORED '+JSON.stringify({contacts:this.state.contacts,settings:p}));
 }
}