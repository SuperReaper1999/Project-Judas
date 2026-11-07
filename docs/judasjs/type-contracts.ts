/** Developer-only compile checks, never imported by the Judas runtime.
 * Positive calls must remain accepted; @ts-expect-error calls must remain errors.
 * Actual execution/value validation is covered by native tests and the cookbook.
 */
import {world, physics, Entity, LocalPlayer, engine} from 'judas';

declare const owner: Entity;
declare const player: LocalPlayer;
const origin = {x: 0, y: 0, z: 0};
const direction = {x: 0, y: 0, z: -1};

// Main/local-player camera ranges independently default omitted near/far members.
world.setView({position: origin});
world.setView({position: origin}, 70, {});
world.setView({position: origin}, 70, {far: 2000});
world.setView({position: origin}, 70, {near: 0.2});
player.setView({position: origin}, 70, {far: 2000});

// Secondary camera projection deliberately requires a complete clipping range.
owner.setCameraProjection({near: 0.2, far: 2000});
// @ts-expect-error Missing near is invalid for secondary camera projection.
owner.setCameraProjection({far: 2000});
// @ts-expect-error Unknown range keys must not silently become public API.
world.setView({position: origin}, 70, {distance: 2000});

physics.raycast(origin, direction, 20, {ignored: [owner]});
// @ts-expect-error Nearest cast requires its maximum travel distance.
physics.raycast(origin, direction);
// @ts-expect-error Query filters use safe Entity wrappers, not raw IDs.
physics.raycast(origin, direction, 20, {ignored: [owner.id]});
// @ts-expect-error No implicit shooter-specific convenience API exists.
world.shootGun();

if (owner.character) {
  owner.character.velocity = origin;
  // @ts-expect-error Ordinary motor vectors require all three object components.
  owner.character.velocity = {x: 1, y: 2};
}

// A render-only entity has no rigid physical material snapshot.
const material = owner.physicalMaterial;
if (material) {
  const friction: number = material.friction;
  void friction;
}
// @ts-expect-error Missing-body material reads must be checked for null.
const uncheckedFriction: number = owner.physicalMaterial.friction;
void uncheckedFriction;

const installedVersion: string = engine.buildInfo.version;
// @ts-expect-error build identity is immutable
engine.buildInfo.version = "invented";

// Stage 1 native sprite/projection calls preserve named-vector conventions.
owner.setSprite({size:{x:1,y:2},flipX:true,tint:{x:1,y:1,z:1,w:.8}});
world.setView({position:origin},60,{projection:'orthographic',height:8,pixelPerfect:true,referenceWidth:320,referenceHeight:180,pixelsPerUnit:32});
world.unproject({x:.5,y:.5});
// @ts-expect-error Sprite size is a named vector, not an array.
owner.setSprite({size:[1,2]});
// @ts-expect-error Unknown projection must not compile.
world.setView({position:origin},60,{projection:'ortho-ish'});

// Native utility decisions share targets/facts with project-owned action handlers.
import {UtilityAgent} from 'judas';
const brain = new UtilityAgent<{health:number}>({version:1,name:'Guard',interval:.2,
  actions:[{id:'hide',considerations:[{fact:'health',curve:'inverse'}]}]}, {
  hide:{update:(dt, context, choice)=>{const hp:number=context.health;const target:string=choice.target;void hp;void target;void dt;}}
});
brain.tick(.016, ()=>[{action:'hide',target:'cover',facts:{health:.2}}],{health:.2});
brain.restore(brain.snapshot());
brain.finish('success');
// @ts-expect-error outcomes are deliberately explicit
brain.finish('finished somehow');
// @ts-expect-error observations must be numerical
brain.tick(.016,[{action:'hide',target:'cover',facts:{health:'injured'}}],{health:.2});
// @ts-expect-error unsupported response curve
new UtilityAgent({actions:[{id:'hide',considerations:[{fact:'health',curve:'magic'}]}]});

// Optional sensor bounds and exclusive lease keys are explicit.
import {AISenses,AITargetReservations} from 'judas';
const senses = new AISenses({maxResults:8,maxRaycasts:4,interval:.2});
const observation=senses.sample(origin,20,{visibility:true,requiredTags:['food']});
const seen:boolean|null=observation.targets[0]?.visible;void seen;
const claim = new AITargetReservations();
claim.acquire('bed:42',1);claim.release('bed:42');
// @ts-expect-error leases take stable string resource keys
claim.acquire(42);
// @ts-expect-error spatial radius is numeric
senses.sample(origin,'nearby');

// Native drag routing carries identifiers; project code owns inventory mutations.
declare const menuDocument: import('judas').UIDocument;
declare const previewImage: import('judas').UIElement;
previewImage.configure({dragType:'item',dropType:'item'});
previewImage.setPreviewCamera(owner);
previewImage.setPreviewCamera(null);
const authoredFontSize:number=previewImage.fontSize;void authoredFontSize;
menuDocument.beginDrag('bag_0');menuDocument.cancelDrag();
if(menuDocument.drag){const source:string=menuDocument.drag.sourceElement;void source;}
// @ts-expect-error A camera binding takes a live Entity wrapper, not a raw ID.
previewImage.setPreviewCamera(42);
// @ts-expect-error Font size is read through the property and edited with configure.
previewImage.fontSize=24;
// @ts-expect-error Drag sources use stable element names.
menuDocument.beginDrag(42);

// V2 runtime surface authoring uses the same source as local excavation.
owner.terrain.surfaceBrush(origin,{kind:'paint',radius:2,layer:3});
owner.terrain.surfaceBrush(origin,{kind:'flatten',radius:3,height:1,strength:.5});
// @ts-expect-error Surface layers are the four authored slots.
owner.terrain.surfaceBrush(origin,{kind:'paint',radius:2,layer:4});
