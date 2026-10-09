/** Current JudasJS through M73 candidate. M72 has Linux human acceptance.
 * Windows VM checks are bounded evidence; hardware acceptance remains outstanding.
 * Tooling only, no TS runtime. See JUDASJS.md.
 * Ordinary returned objects are detached snapshots.
 */
declare module "judas" {
  export type SignalPhase = 'fixed' | 'ui';
  /** Disposable world/slot-generation token. Never save for reuse after restore. */
  export type SignalSubscription = string;
  export interface SignalAcceptance {recipients:number;sequence:string|null}
  export interface SignalEvent {name:string;payload:JSONValue;phase:SignalPhase;sequence:string;senderId:EntityId;senderSlot:string;sender:Entity|null}
  export interface SignalStats {subscriptions:number;queuedEvents:number;queuedBytes:number;queuedRecipients:number;accepted:number;delivered:number;skipped:number;rejected:number}
  export const signals: {
    subscribe(name:string,phase?:SignalPhase):SignalSubscription;
    unsubscribe(token:SignalSubscription):boolean;
    emit(name:string,payload?:JSONValue,phase?:SignalPhase):SignalAcceptance;
    send(target:Entity,name:string,payload?:JSONValue,phase?:SignalPhase):SignalAcceptance;
    readonly stats:SignalStats;
  };

  export interface LiquidState {entityId:EntityId;entity:Entity|null;enabled:boolean;equilibriumValid:boolean;container:boolean;material:string;density:number;volume:number;capacity:number;stableCapacity:number;coordinate:number;surface:LiquidSurfaceState|null}
  export interface LiquidSurfaceState {enabled:boolean;cells:number;faces:number;volume:number;iterations:number;retries:number;limitedFaces:number;residual:number;partitionError:number;stepSeconds:number}
  export interface LiquidAccounting {reservoirs:number;containers:number;detached:number;total:number;expected:number;error:number;tolerance:number}
  export interface LiquidSample {entityId:EntityId;entity:Entity|null;material:string;density:number;depth:number;coordinate:number;surfacePoint:Vec3;normal:Vec3;up:Vec3;velocity:Vec3}
  export class LiquidVolume {constructor(handle:string);handle:string;readonly valid:boolean;readonly state:LiquidState;set enabled(value:boolean);transferTo(destination:LiquidVolume,volume:number,options?:{sourcePoint?:Vec3;destinationPoint?:Vec3}):number;applyImpulse(point:Vec3,impulse:Vec3):boolean;set surfaceEnabled(value:boolean)}
  export const liquid:{sample(point:Vec3):LiquidSample|null;samplePresented(point:Vec3):LiquidSample|null;accounting(material?:string):LiquidAccounting;readonly errors:{entityId:EntityId;message:string}[];readonly connections:{entityId:EntityId;active:boolean}[];submerged(target:Entity):{volume:number;center:Vec3;buoyancy:Vec3}};
  export type EntityId = string;
  export type AssetId = string;
  export type JSONValue = null | boolean | number | string | JSONValue[] | { [key: string]: JSONValue };
  export interface Vec3 { x: number; y: number; z: number }
  export interface Quat extends Vec3 { w: number }
  export interface Transform { position: Vec3; rotation: Quat; scale: Vec3 }
  export interface TransformPatch { position?: Vec3; rotation?: Quat; scale?: Vec3 }
  export type BodyMotionType = "static" | "dynamic" | "kinematic";
  /** Complete world authored-pivot pose; COM follows a linear segment. */
  export interface KinematicTarget {position:Vec3;rotation:Quat}
  export interface MotionTypeOptions {preserveVelocity?:boolean}
  /** Durable command snapshot; actual interval velocities are on Entity. */
  export interface KinematicMotion {
    control:"stopped"|"target"|"velocity";
    target:KinematicTarget;
    linearVelocity:Vec3;angularVelocity:Vec3;
    remainingSeconds:number;targetNextStep:boolean;
  }
  export interface CastPose { position: Vec3; rotation?: Quat }
  export interface Ray { origin: Vec3; direction: Vec3 }
  /** One nearest-hit request, using the scalar cast's coordinates and units. */
  export interface RaycastRequest extends Ray { maximum: number }
  export interface SphereCastRequest extends RaycastRequest { radius: number }
  export interface CapsuleCastRequest { pose: CastPose; radius: number; halfHeight: number; direction: Vec3; maximum: number }
  export interface QueryFilter {
    includeLayers?: string[]; excludeLayers?: string[];
    requiredTags?: string[]; excludedTags?: string[];
    ignored?: Entity[]; includeSensors?: boolean;
  }
  export interface Classification { renderLayer: number; collisionLayer: number; collisionMask: string }
  export interface CameraRange {near:number;far:number}
  export interface ViewportPoint {x:number;y:number;depth:number;distance:number;behind:boolean;inside:boolean}
  export interface UIElementLayout {offset:{x:number;y:number};size:{x:number;y:number};anchorMin:{x:number;y:number};anchorMax:{x:number;y:number};relativeSize:{x:number;y:number};align:{x:number;y:number}}
  export interface PrefabScriptInit {source?:EntityId;slot:string|number;properties?:Record<string,number|boolean|string|{entity:EntityId}|null>;state?:JSONValue}
  export interface PrefabSpawnOptions {velocity?:Vec3;angularVelocity?:Vec3;scripts?:PrefabScriptInit[]}
  export interface CameraInfo { near:number;far:number; enabled: boolean; width: number; height: number }
  export interface AudioInfo {
    enabled:boolean;playing:boolean;requested:boolean;ready:boolean;streamed:boolean;loop:boolean;starved:boolean;
    state:"loading"|"seeking"|"ready"|"playing"|"paused"|"ended"|"starved"|"failed";
    error:string|null;position:number|null;duration:number|null;bufferBytes:number;underruns:number;
    dopplerRatio:number;occlusionGain:number;cutoff:number;distanceGain:number;
  }
  export interface AudioSettingsPatch {
    loading?:"buffered"|"streamed";streamPageFrames?:number;group?:string;loop?:boolean;spatial?:boolean;
    volume?:number;pitch?:number;doppler?:number;send?:number;occlusion?:boolean;bypass?:boolean;
    occlusionLayers?:string[];occludedGain?:number;occludedCutoff?:number;
    referenceDistance?:number;maximumDistance?:number;rolloff?:number;attenuation?:"none"|"inverse"|"linear";
  }
  export interface AudioGroup {gain:number;mute:boolean;paused:boolean}
  export interface AudioDiagnostics {voices:number;streams:number;bufferedBytes:number;streamBytes:number;streamHighWater:number;pendingRetirements:number;reverbProcessors:number;underruns:number;decodedFrames:number;occlusionQueries:number;maximumDetachMilliseconds:number}
  export const audio:{group(name:string):AudioGroup|null;setGroup(name:string,settings:Partial<AudioGroup>,fadeSeconds?:number):boolean|null;readonly diagnostics:AudioDiagnostics|null};
  export interface ParticleSettingsPatch { enabled?: boolean; rate?: number }
  export interface CastHit {
    physicalMaterial:AssetId|null;
    entity: Entity | null; entityId: EntityId; bodyId: number;
    point: Vec3; normal: Vec3; distance: number; fraction: number;
    primitiveIndex: number; initialOverlap: boolean; shape: "capsule" | "sphere" | "box" | "terrain" | "hull" | "triangle-mesh";
    childKey:number;feature:number|null;
  }
  export interface LegacySweepHit { hit: boolean; distance: number; normal: Vec3; entityId: EntityId }
  export interface FluidSample { immersion: number; density: number; velocity: Vec3; acceleration: Vec3 }
  /** Plain wrappers reacquire native state. Treat the writable ID as opaque. */
  export type ResourceStatus = "unloaded"|"queued"|"loading"|"cpu-ready"|"ready"|"failed"|"cancelled";
  export interface AppearanceSettings {
    backgroundColor:Vec3;linearRendering:boolean;exposure:number;
    sunEnabled:boolean;sunIntensity:number;
    /** Finite nonzero world-space direction toward the light; normalized on write. */
    sunDirection:Vec3;sunColor:Vec3;ambientColor:Vec3;
    environmentAsset:AssetId;environmentIntensity:number;environmentRotation:Quat;environmentBackground:boolean;
  }
  export interface Appearance extends AppearanceSettings {readonly environmentStatus:ResourceStatus;readonly environmentError:string}
  export type MaterialTarget = number|string;
  export interface MaterialTextures {baseColor:AssetId;metallicRoughness:AssetId;normal:AssetId;occlusion:AssetId;emissive:AssetId}
  export interface MaterialParameters {
    uvScale?:{x:number;y:number};uvOffset?:{x:number;y:number};baseColor?:Vec3 & {a:number};
    metallic?:number;roughness?:number;emissive?:Vec3;emissiveIntensity?:number;
    alphaMode?:"opaque"|"mask"|"blend";alphaCutoff?:number;normalStrength?:number;occlusionStrength?:number;doubleSided?:boolean;
    /** Registered image IDs; an empty string explicitly removes that map. */
    textures?:Partial<MaterialTextures>;
  }
  export interface MaterialState {
    uvScale:{x:number;y:number};uvOffset:{x:number;y:number};asset:AssetId;ready:boolean;
    status:"ready"|"pending"|"failed";error:string;model:"legacy"|"pbr"|"unlit";
    alphaMode:"opaque"|"mask"|"blend";alphaCutoff:number;normalStrength:number;occlusionStrength:number;doubleSided:boolean;
    baseColor:Vec3 & {a:number};metallic:number;roughness:number;emissive:Vec3;emissiveIntensity:number;
    textures:MaterialTextures;overridden:boolean;
  }
  export class Material {
    constructor(entityId:EntityId,slot?:MaterialTarget);
    entityId:EntityId;slot:MaterialTarget;
    readonly state:MaterialState;
    assign(asset:AssetId):boolean;
    set(parameters:MaterialParameters):boolean;
    clearOverrides():boolean;
  }
  export interface ColliderShapeInfo {
    type:"box"|"sphere"|"compound"|"hull"|"triangle-mesh"|"terrain"|"capsule";
    key:number;position:Vec3;rotation:Quat;halfExtents:Vec3|null;radius:number|null;halfHeight:number|null;
    asset:AssetId|null;vertexCount:number;triangleCount:number;twoSided:boolean;
  }
  export interface ColliderInfo extends ColliderShapeInfo {
    centerOfMassOffset:Vec3;enabled:boolean;sensor:boolean;children:ColliderShapeInfo[];
  }
  export interface ClosestPointHit {
    entity:Entity|null;entityId:EntityId;bodyId:number;point:Vec3;
    normal:Vec3|null;distance:number;contains:boolean|null;
    primitiveIndex:number;childKey:number;feature:number|null;
  }
  export interface ModelPart { readonly identity: string; readonly materialSlot: number; readonly triangles: number; readonly visible: boolean }
  export interface RootMotion { readonly translation: Vec3; readonly rotation: Quat; readonly extracted: boolean }
  export interface EntityGravityState {
    mode:"spatial"|"uniform"|"field";
    available:boolean;
    /** Live selected source ID, null for other modes or missing source. */
    sourceId:EntityId|null;
    source:Entity|null;
    acceleration:Vec3;
  }
  /** Optional owner-local intent, no automatic child/articulation inheritance. */
  export class EntityGravity {
    constructor(id:EntityId);id:EntityId;
    readonly state:EntityGravityState;
    readonly acceleration:Vec3;
    select(source:Entity):boolean;
    setUniform(acceleration:Vec3):boolean;
    clear():boolean;
  }
  export class Entity {
    /** Local entity render gate; does not hide children or change simulation. */
    renderVisible:boolean;
    /** Independent Render component gate; throws when that component is absent. */
    rendererVisible:boolean;
    readonly modelParts: readonly ModelPart[] | null;
    setPartVisible(identity: string, visible: boolean): boolean;
    readonly collider:ColliderInfo|null;
    readonly sleeping:boolean;
    readonly physicalMaterial:{asset:AssetId|null;friction:number;restitution:number};
    setPhysicalMaterial(asset?:AssetId|null,parameters?:{friction?:number;restitution?:number}):boolean;
    setSocket(target:Entity,joint:string,offset?:TransformPatch):boolean;
    clearSocket():boolean;
    material(slot?:MaterialTarget):Material;
    readonly liquid:LiquidVolume|null;
    readonly gravity:EntityGravity;
    constructor(id: string | number | bigint);
    id: EntityId;
    readonly valid: boolean;
    get transform(): Transform;
    /** Render-interpolated world pose in presentationUpdate; authoritative pose otherwise. */
    readonly presentedTransform: Transform;
    set transform(value: TransformPatch);
    readonly parent: Entity | null;
    readonly children: Entity[];
    setColliderEnabled(enabled: boolean): boolean;
    destroy(): boolean;
    hasTag(tag: string): boolean;
    addTag(tag: string): boolean;
    removeTag(tag: string): boolean;
    readonly classification: Classification;
    readonly animation: Animation | null;
    readonly navigation: NavigationAgent | null;
    readonly navigationObstacle: NavigationObstacleInfo | null;
    readonly navigationLink: NavigationLinkInfo | null;
    setNavigationEnabled(component: "agent" | "obstacle" | "link", enabled: boolean): boolean;
    readonly character: Character | null;
    readonly fracture:Fracture|null;
    readonly deformable: Deformable | null;
    readonly ragdoll: Ragdoll | null;
    readonly audio: AudioInfo | null;
    setAudio(settings:AudioSettingsPatch):boolean;
    seekAudio(seconds:number):boolean;
    setAudioVelocity(velocity?:Vec3|null):boolean;
    playAudioOneShot():boolean;
    setAudioEnabled(enabled: boolean): boolean;
    readonly camera: CameraInfo | null;
    scriptState(slot: string | number): JSONValue;
    applyForce(value: Vec3): void;
    applyImpulse(value: Vec3): void;
    /** World-space impulse (N s) at a world-space point (metres). */
    applyImpulseAtPoint(impulse: Vec3, point: Vec3): void;
    applyTorque(value: Vec3): void;
    readonly mass: number;
    readonly inertiaWorld: {x: Vec3; y: Vec3; z: Vec3};
    velocity: Vec3;
    angularVelocity: Vec3;
    readonly motionType: BodyMotionType | null;
    setMotionType(type:BodyMotionType,options?:MotionTypeOptions):boolean;
    /** 0 advances over the next physics interval; positive duration <=60 seconds. */
    moveKinematic(target:KinematicTarget,seconds?:number):boolean;
    /** Persistent world COM m/s and world rad/s; angular defaults to zero. */
    setKinematicVelocity(linear:Vec3,angular?:Vec3):boolean;
    stopKinematic():boolean;
    readonly kinematicMotion:KinematicMotion|null;
    /** Actual world contact-point velocity, including rotation about COM. */
    pointVelocity(point:Vec3):Vec3;
    playAudio(): boolean;
    stopAudio(): boolean;
    pauseAudio(): boolean;
    resumeAudio(): boolean;
    burst(count: number): boolean;
    setParticles(settings: ParticleSettingsPatch): boolean;
    setCameraEnabled(enabled: boolean): boolean;
    setCameraProjection(range:CameraRange):boolean;
  }
  /** Does not check existence; null for absent/falsy input or string "0". Use .valid. */
  export function entity(id: string | number | bigint | null | undefined): Entity | null;
  export const world: {
    entity: typeof entity;
    readonly appearance:Appearance;
    setAppearance(settings:Partial<AppearanceSettings>):boolean;
    resetAppearance():boolean;
    setView(pose: TransformPatch, fov?: number, range?: CameraRange): boolean;
    project(point:Vec3):ViewportPoint|null;
    readonly viewport:{width:number;height:number};
    clearView(): boolean;
    fluidSample(point: Vec3, up: Vec3, halfHeight: number, radius: number, tangent: Vec3): FluidSample;
    readonly viewRay: Ray | null;
    queryTags(required?: string[], excluded?: string[]): Entity[];
    spawnPrefab(asset: AssetId, transform?: TransformPatch, options?: PrefabSpawnOptions): Entity;
    /** Conservative broadphase candidates, not exact overlap results. */
    overlap(min: Vec3, max: Vec3, filter?: QueryFilter): Entity[];
    /** Legacy fixed player-sized capsule; prefer physics.capsuleCast for explicit dimensions. */
    sweepCapsule(from: Vec3, displacement: Vec3, rotation?: Quat, filter?: QueryFilter): LegacySweepHit;
  };
  export interface CharacterSettingsPatch {
    radius?: number; halfHeight?: number; offset?: Vec3;
    stepHeight?: number; supportDistance?: number; skin?: number; maxSlopeDegrees?: number;
    gravityScale?: number; reorientationDegreesPerSecond?: number;
    interactionMass?: number; maxPushImpulse?: number;
    collisionLayer?: string; collisionMask?: string[]; requiredTags?: string[]; excludedTags?: string[];
  }
  export interface CharacterState {
    velocity: Vec3; actualDisplacement: Vec3; supportNormal: Vec3; supportVelocity: Vec3;
    gravity: Vec3; up: Vec3; supported: boolean; collided: boolean;
    supportEntityId: EntityId; supportEntity: Entity | null;
  }
  export interface NavigationFilter {
    profile?: number | string; includeAreas?: string[]; excludeAreas?: string[];
    /** Traversal costs >= 1, keyed by project area name. */
    costs?: { [area: string]: number };
  }
  export interface NavigationLocation { position: Vec3; surfaceId: EntityId; surface: Entity | null; area: number }
  export interface NavigationCorner { position: Vec3; linkId: EntityId; link: Entity | null; linkEnd: Vec3 }
  export interface NavigationPath { status: "complete" | "partial" | "failed"; corners: NavigationCorner[]; distance: number; revision: number; areas: number[] }
  export interface NavigationAgentState {
    stopped: boolean; hasDestination: boolean; destination: Vec3; steering: Vec3;
    remainingDistance: number; reached: boolean; nextCorner: Vec3 | null;
    onLink: boolean; linkId: EntityId; link: Entity | null; linkEnd: Vec3;
    /** Agent path corners have native linkId, not the top-level path's link wrappers. */
    path: Omit<NavigationPath, "corners"> & { corners: Omit<NavigationCorner, "link">[] };
  }
  export interface NavigationAgentPatch extends NavigationFilter { speed?: number; arrival?: number; repathSeconds?: number; avoidance?: boolean }
  export interface NavigationObstacleInfo { enabled: boolean; cylinder: boolean; halfExtents: Vec3; radius: number; height: number }
  export interface NavigationLinkInfo { enabled: boolean; bidirectional: boolean; start: Vec3; end: Vec3; area: number }
  export class NavigationAgent {
    constructor(id: EntityId);
    id: EntityId;
    readonly state: NavigationAgentState;
    set enabled(value: boolean);
    setDestination(point: Vec3): boolean;
    clear(): boolean;
    get stopped(): boolean;
    set stopped(value: boolean);
    readonly steering: Vec3;
    readonly remainingDistance: number;
    completeLink(): boolean;
    configure(settings: NavigationAgentPatch): boolean;
  }
  export const navigation: {
    sample(point: Vec3, range?: number, filter?: NavigationFilter): NavigationLocation | null;
    path(start: Vec3, end: Vec3, filter?: NavigationFilter): NavigationPath;
    raycast(start: Vec3, end: Vec3, filter?: NavigationFilter): NavigationLocation | null;
    readonly areas: {id: number; name: string}[];
    readonly profiles: {id: number; name: string; radius: number; height: number}[];
    readonly errors: {entityId: EntityId; message: string}[];
  };
  export interface DeformableMaterialOptions {
    density?:number;stretchCompliance?:number;shearCompliance?:number;bendCompliance?:number;volumeCompliance?:number;
    damping?:number;thickness?:number;friction?:number;airDrag?:number;airVelocity?:Vec3;
    yieldStrain?:number;plasticRate?:number;maximumPlasticStrain?:number;
  }
  export interface DeformableLocation {epoch:string;triangle:number;weights:Vec3;revision:number}
  export interface DeformableHit {point:Vec3;normal:Vec3;distance:number;location:DeformableLocation}
  export interface DeformableState {enabled:boolean;sleeping:boolean;error:string;mass:number;minimum:Vec3;maximum:Vec3;nodes:number;contacts:number;minimumJacobian:number;maximumStrain:number;groups:string[]}
  export interface DeformableAttachmentOptions {kind:'world'|'body'|'bone';target?:Entity;joint?:string;offset?:Vec3}
  export interface FracturePartState {key:string;index:number;component:number|null;removed:boolean;entity:Entity|null;mass:number}
  export interface FractureInterfaceState {key:string;a:number;b:number;broken:boolean;cause:'physical'|'explicit'|null;tension:number;shear:number}
  export interface FractureState {revision:number;rigid:boolean;error:string;parts:FracturePartState[];interfaces:FractureInterfaceState[];tensileStrength:number;shearStrength:number}
  export interface FractureEvent {revision:number;interfaces:{key:string;cause:'physical'|'explicit'}[]}
  export class Fracture {
    constructor(id:EntityId,epoch:string);
    id:EntityId;epoch:string;
    readonly valid:boolean;readonly state:FractureState;
    release(interfaceKey:string,revision:number):boolean;
    remove(part:number,revision:number):boolean;
    force(part:number,value:Vec3):boolean;
    impulse(part:number,value:Vec3):boolean;
  }
  export class Deformable {
    constructor(id:EntityId,epoch:string);
    id:EntityId;epoch:string;
    readonly valid:boolean;readonly state:DeformableState;
    get enabled():boolean;set enabled(value:boolean);
    reset():boolean;force(value:Vec3,group?:string):boolean;impulse(value:Vec3,group?:string):boolean;
    impulseAt(location:DeformableLocation,value:Vec3):boolean;
    release(group:string):boolean;attach(group:string,options:DeformableAttachmentOptions):boolean;
    setMaterial(options:DeformableMaterialOptions):boolean;
    raycast(origin:Vec3,direction:Vec3,maximum:number):DeformableHit|null;
  }
  export class Character {
    constructor(id: EntityId);
    id: EntityId;
    readonly state: CharacterState;
    get velocity(): Vec3;
    set velocity(value: Vec3);
    readonly supported: boolean;
    readonly supportNormal: Vec3;
    readonly supportVelocity: Vec3;
    readonly actualDisplacement: Vec3;
    readonly gravity: Vec3;
    readonly up: Vec3;
    /** Setter only in JS; reading returns undefined. TS cannot enforce write-only access. */
    set enabled(value: boolean);
    configure(settings: CharacterSettingsPatch): boolean;
    accelerate(value: Vec3): boolean;
    ignore(entities: Entity[]): boolean;
  }
  /**
   * M70 serializable configuration tuple, not an ordinary object Vec3.
   * configureIK/ikTargets and physical-mode placement require exactly three finite components.
   * Live transforms, physics/motor vectors and returned snapshots still use Vec3 objects.
   */
  export type PoseVector = [x: number, y: number, z: number];
  /** M70 configuration quaternion tuple in x,y,z,w order, not an object Quat. */
  export type PoseQuaternion = [x: number, y: number, z: number, w: number];
  export type PhysicalAnimationMode = "animation" | "partial" | "active" | "passive";
  export interface PhysicalAnimationRegion {
    id: string; joints: string[]; enabled?: boolean;
    /** N m / rad, default 30. */ stiffness?: number;
    /** N m s / rad, default 4. */ damping?: number;
    /** Magnitude cap in N m, default 20. */ maxTorque?: number;
    effortWeight?: number; poseWeight?: number;
  }
  export interface PhysicalAnimationSettings { enabled?: boolean; regions: PhysicalAnimationRegion[] }
  export interface PhysicalAnimationModeOptions {
    fade?: number; motorHandoff?: boolean; resumeMotor?: boolean;
    /** Explicit world placement uses M70 array tuples, not object TransformPatch fields. */
    placement?: {position: PoseVector; rotation: PoseQuaternion};
  }
  export interface PhysicalAnimationDrive {
    joint: string; region: string; angleError: number; torque: number;
    saturated: boolean; sleeping: boolean;
  }
  export interface PhysicalAnimationState {
    mode: PhysicalAnimationMode; pending: PhysicalAnimationMode | null;
    diagnostic: string; drives: PhysicalAnimationDrive[];
  }
  export class Ragdoll {
    constructor(id: EntityId);
    id: EntityId;
    readonly active: boolean;
    enter(): boolean;
    leave(seconds?: number): boolean;
    /** Setter only; read is undefined. */
    set enabled(value: boolean);
    /** Opt in to external mapped-body M42 callbacks on owner scripts. Default false; not aggregate events. */
    receiveContactEvents: boolean;
    body(joint: string): Entity | null;
    configurePhysical(settings: PhysicalAnimationSettings | null): boolean;
    setMode(mode: PhysicalAnimationMode, options?: PhysicalAnimationModeOptions): boolean;
    readonly physicalState: PhysicalAnimationState;
  }
  export interface ClipInfo { name: string; duration: number }
  export interface AnimationLayerInfo { id: string; clip: string; weight: number; enabled: boolean; additive: boolean }
  export interface AnimationInfo {
    ready: boolean; playing: boolean; loop: boolean; speed: number; time: number; clip: string;
    clips: ClipInfo[]; transitioning: boolean; transitionFraction: number; error: string;
    joints: string[]; layers: AnimationLayerInfo[];
  }
  export interface AnimationLayerPatch {
    clip?: string; referenceClip?: string; weight?: number; speed?: number; time?: number;
    referenceTime?: number; enabled?: boolean; additive?: boolean; mask?: string[];
  }
  /** Legacy position-only two-bone IK. World target/pole are object Vec3; no orientation field. */
  export interface LimbIKPatch {root?:string;middle?:string;end?:string;target?:Vec3;pole?:Vec3;weight?:number;enabled?:boolean;order?:number}
  export interface FullBodyIKChain { id: string; joints: string[] }
  export interface FullBodyIKJointLimit {
    joint: string; frame?: PoseQuaternion;
    /** Radians in the declared local rotation-vector frame. */
    min?: PoseVector; max?: PoseVector; preferred?: PoseVector;
    preferenceWeight?: number;
  }
  export interface FullBodyIKTarget {
    id: string; chain: string; enabled?: boolean; space?: "model" | "world";
    /** M70 array tuples in the selected space; orientation participates only when weighted. */
    position?: PoseVector; orientation?: PoseQuaternion;
    positionWeight?: number; orientationWeight?: number;
    /** Effector-local contact frame. */ offset?: PoseVector; frame?: PoseQuaternion;
  }
  export interface FullBodyIKSettings {
    bodyRoot: string; chains: FullBodyIKChain[]; enabled?: boolean; rootRotation?: boolean;
    rootMin?: PoseVector; rootMax?: PoseVector; spine?: string[];
    limits?: FullBodyIKJointLimit[]; targets?: FullBodyIKTarget[];
    iterations?: number; damping?: number;
    positionTolerance?: number; orientationTolerance?: number; orientationScale?: number;
    maxAngularStep?: number; maxTranslationStep?: number;
  }
  export interface FullBodyIKResidual {
    id: string; status: "disabled" | "reached" | "limited";
    positionError: number; orientationError: number;
    /** Model-space contact frame, not a body/collision result. */
    actualPosition: Vec3; actualOrientation: Quat;
  }
  export interface FullBodyIKStatus {
    ready: boolean; enabled: boolean; diagnostic: string;
    converged: boolean; iterations: number; solveMicroseconds: number;
    /** Detached snapshot objects; convert explicitly to tuples when resubmitting configuration. */
    rootCorrection: Vec3; targets: FullBodyIKResidual[];
  }
  export class Animation {
    /** Independent clip interval in model-local motion space; never moves physics implicitly. */
    rootMotion(clip: string, from: number, to: number, loop?: boolean): RootMotion | null;
    constructor(entityId: EntityId);
    entityId: EntityId;
    readonly info: AnimationInfo;
    readonly clips: ClipInfo[];
    readonly playing: boolean;
    readonly time: number;
    speed: number;
    loop: boolean;
    play(clip?: string): boolean;
    pause(): boolean;
    resume(): boolean;
    stop(): boolean;
    seek(time: number): boolean;
    crossFade(clip: string, seconds?: number): boolean;
    readonly layers: AnimationLayerInfo[];
    layer(id: string, settings: AnimationLayerPatch): boolean;
    removeLayer(id: string): boolean;
    jointTransform(key:string,space?:"local"|"model"|"world",presented?:boolean):Transform|null;
    limb(id:string,settings:LimbIKPatch):boolean;
    removeLimb(id:string):boolean;
    configureIK(settings: FullBodyIKSettings | null): boolean;
    ikTargets(targets: FullBodyIKTarget[]): boolean;
    readonly ikStatus: FullBodyIKStatus;
  }
  export interface JointState { active: boolean; enabled: boolean; coordinate: number; motorImpulse: number; type: 0 | 1 | 2 | 3 }
  export interface JointConfiguration {type?:"fixed"|"hinge"|"ball"|"slider";anchorA?:Vec3;anchorB?:Vec3;frameA?:Quat;frameB?:Quat;enabled?:boolean;limits?:boolean;motor?:boolean;spring?:boolean;lower?:number;upper?:number;speed?:number;maxForce?:number;rest?:number;stiffness?:number;damping?:number;rotationalResistance?:number}
  export class Joint {
    constructor(id: string | number | bigint);
    id: string;
    readonly valid: boolean;
    readonly state: JointState;
    configure(settings:JointConfiguration):boolean;
    destroy():boolean;
    setEnabled(enabled: boolean): boolean;
    setLimits(lower: number, upper: number, limits?: boolean): boolean;
    setMotor(speed: number, maxForce: number, motor?: boolean): boolean;
    setSpring(rest: number, stiffness: number, damping: number, spring?: boolean): boolean;
  }
  export const physics: {
    gravity(point:Vec3):Vec3;
    createJoint(owner:Entity,settings:JointConfiguration & {bodyA:Entity;bodyB?:Entity|null}):Joint;
    closestPoint(point:Vec3,maximum:number,filter?:QueryFilter):ClosestPointHit|null;
    joint(owner: Entity): Joint | null;
    raycast(origin: Vec3, direction: Vec3, maximum: number, filter?: QueryFilter): CastHit | null;
    /** Synchronous, ordered nearest hits; at most 256 requests, one native bridge crossing. */
    raycastMany(rays: RaycastRequest[], filter?: QueryFilter): (CastHit | null)[];
    sphereCast(origin: Vec3, radius: number, direction: Vec3, maximum: number, filter?: QueryFilter): CastHit | null;
    sphereCastMany(casts: SphereCastRequest[], filter?: QueryFilter): (CastHit | null)[];
    capsuleCast(pose: CastPose, radius: number, halfHeight: number, direction: Vec3, maximum: number, filter?: QueryFilter): CastHit | null;
    capsuleCastMany(casts: CapsuleCastRequest[], filter?: QueryFilter): (CastHit | null)[];
    boxCast(pose: CastPose, halfExtents: Vec3, direction: Vec3, maximum: number, filter?: QueryFilter): CastHit | null;
  };
  /** Opaque request token; only valid in the requesting played-world session. */
  export type RegionRequest = string;
  export interface RegionStatus { id:string;state:"unloaded"|"preparing"|"prepared"|"installing"|"active"|"unloading"|"cancelled"|"failed"|"blocked";error:string;pins:string[];visualReady:boolean;entities:number;installed:number;bytes:number;retained:number;demands:number;preparationMs:number;integrationMs:number;largestUnitMs:number;loadMs:number }
  export interface StreamingStats {active:number;pending:number;pendingBytes:number;liveBytes:number;retainedBytes:number;resourceResidentBytes:number;resourceCacheBudget:number;integrationMs:number;largestUnitMs:number}
  export const scenes: {
    readonly regions:RegionStatus[];
    readonly streamingStats:StreamingStats;
    requestRegion(name:string,options?:{preload?:boolean}):RegionRequest;
    regionStatus(token:RegionRequest):RegionStatus|null;
    activateRegion(token:RegionRequest):boolean;
    releaseRegion(token:RegionRequest):boolean;
    unloadRegion(name:string):boolean;
    owner(entity:Entity):string;
    resolveRegionEntity(name:string,local:string|number):Entity|null;
    pinRegion(name:string,reason:string,pin?:boolean):boolean;
    adopt(entity:Entity,name?:string):boolean;
    setInterest(name:string,position:Vec3,options:{load:number;retain:number;priority?:number}):boolean;
    removeInterest(name:string):void;
    readonly current: string;
    readonly registered: string[];
    load(name: string): boolean;
    reload(): boolean;
  };
  export interface NumberOptions { minimumFraction?: number; maximumFraction?: number; grouping?: boolean }
  export const localization: {
    readonly locale: string;
    readonly available: string[];
    readonly revision: number;
    readonly direction: "ltr" | "rtl";
    /** True means request queued. Current locale/revision change only after all resources validate. */
    setLocale(locale: string): boolean;
    format(key: string, args?: Record<string, string | number>): string;
    number(value: number, options?: NumberOptions): string;
    reload(): void;
  };
  export const session: { get(key: string): JSONValue; set(key: string, value: JSONValue): void; delete(key: string): void };
  export type StickSide = "left" | "right";
  export interface InputVector { x: number; y: number }
  /** Copied backend observation: receipt time in monotonic seconds, not simulation time. */
  export interface StickSample extends InputVector { sequence: number; time: number }
  export interface StickHistory {
    samples: StickSample[];
    /** Global observation cursor, including observations of the other stick. */
    sequence: number;
    /** Cursor predates a reset; discard recognizer state instead of treating the gap as motion. */
    reset: boolean;
    /** Requested side has dropped observations; returned history is incomplete. */
    overflow: boolean;
    capacity: 128;
  }
  export const input: {
    pointerCapture: boolean;
    held(name: string): boolean; pressed(name: string): boolean; released(name: string): boolean; axis(name: string): number;
    /** Raw normalized components, before Judas processing. X right+, Y down+; no unit-circle clamp. */
    stick(side: StickSide): InputVector;
    /** Net raw movement observed during the latest render pump, independent of fixed-step count. */
    stickDelta(side: StickSide): InputVector;
    /** Processed named paired binding, or neutral if absent/consumed. */
    vector(name: string): InputVector;
    /** Non-destructive ordered snapshot; cursor must be a nonnegative safe integer. */
    stickSamples(side: StickSide, afterSequence?: number): StickHistory;
  };
  export const time: { readonly elapsed: number; readonly delta: number; readonly fixed: boolean };
  export const console: { log(...args: unknown[]): void };
  export class UIElement {
    constructor(handle: number, id: string);
    handle: number;
    id: string;
    readonly layout:UIElementLayout;
    setLayout(patch:Partial<UIElementLayout>):UIElementLayout;
    text: string;
    /** Assigning text clears an authored textKey. Assigning textKey restores localization binding. */
    textKey: string;
    font: AssetId;
    direction: "auto" | "ltr" | "rtl";
    /** Empty on legacy numeric alignment; writes must use a named value. */
    get textAlignment(): "" | "left" | "right" | "center" | "start" | "end";
    set textAlignment(value: "left" | "right" | "center" | "start" | "end");
    visible: boolean;
    enabled: boolean;
    value: number;
    texture: AssetId;
  }
  export class UIDocument {
    constructor(handle: number);
    handle: number;
    get(id: string): UIElement;
    visible: boolean;
    enabled: boolean;
    modal: boolean;
    show(): void;
    hide(): void;
    unload(): void;
  }
  export const ui: {
    get(name: string): UIDocument | null;
    load(asset: AssetId, name: string): UIDocument;
    quit(): void;
    debugOverlayVisible: boolean;
  };
  export interface UIEvent { document: string; element: string; type: "click" | "change" | "focus" | "back"; value: number }
  export interface ContactEvent { other: Entity | null; point: Vec3; normal: Vec3; relativeVelocity: Vec3; physicalMaterial:AssetId|null; normalImpulse: number | null; selfBody: Entity | null; selfJoint: string | null; otherArticulation: Entity | null; otherJoint: string | null }
  export type ScriptProperties = Record<string, number | boolean | string | Entity | null>;
  /** Authored defaults; entity references become Entity|null in ScriptContext.properties. */
  export type PropertySchema = Record<string,
    { type: "number"; default?: number } | { type: "boolean"; default?: boolean } |
    { type: "string"; default?: string } | { type: "entity"; default?: {entity:EntityId} | null }>;
  export interface SaveOptions { name?: string; metadata?: { [key: string]: JSONValue } | JSONValue[] }
  export interface SaveSlot {
    readonly id: string; readonly name: string; readonly scene: string; readonly timestamp: number;
    readonly metadata: JSONValue; readonly status: "compatible" | "incompatible" | "corrupt";
    readonly error: string; readonly recovered: boolean;
  }
  export interface SaveRequest {
    readonly id: number; readonly operation: "save" | "load" | "delete" | "list"; readonly slot: string;
    readonly state: "queued" | "writing" | "reading" | "working" | "preparing" | "restoring" | "completed" | "failed" | "cancelled";
    readonly error: string; readonly recovered: boolean; readonly captureMs: number; readonly workerMs: number;
    readonly restoreMs: number; readonly bytes: number;
  }
  export const saves: {
    save(slot: string, options?: SaveOptions): number;
    load(slot: string): number;
    delete(slot: string): number;
    refresh(): number;
    /** Cached metadata; refresh or a completed operation updates it asynchronously. */
    list(): SaveSlot[];
    exists(slot: string): boolean;
    status(request: number): SaveRequest | null;
    cancel(request: number): boolean;
    exclude(entity: Entity, excluded?: boolean): boolean;
    reference(entity: Entity): string | null;
    resolve(key: string): Entity | null;
  };
  export interface ScriptContext<P extends ScriptProperties = ScriptProperties> {
    entity: Entity; properties: P;
    /** Modern save-load construction; saved state is installed after the constructor. */
    readonly restored: boolean;
    /** Prefab construction data, otherwise null; not automatically assigned to this.state. */
    readonly initialState:JSONValue;
  }
  /** Structural tooling interface, not a runtime-exported base class. */
  export interface ScriptBehaviour {
    state?: JSONValue;
    start?(dt: number): void;
    /** Runs INSTEAD of start after modern slot load; state is already restored. */
    restore?(dt: number): void;
    update?(dt: number): void;
    fixedUpdate?(dt: number): void;
    uiUpdate?(dt: number): void;
    /** After fixed steps, before camera/audio/render; alpha is the renderer interpolation fraction. */
    presentationUpdate?(dt: number, alpha: number): void;
    destroy?(dt: number): void;
    onSignal?(event:SignalEvent):void;
    onUI?(event: UIEvent): void;
    onFracture?(event:FractureEvent):void;
    onCollisionEnter?(event: ContactEvent): void;
    onCollisionStay?(event: ContactEvent): void;
    onCollisionExit?(event: ContactEvent): void;
    onTriggerEnter?(event: ContactEvent): void;
    onTriggerStay?(event: ContactEvent): void;
    onTriggerExit?(event: ContactEvent): void;
  }
  /** Diagnostics only: never use measured timings as game/simulation input. */
  export const profiler: {
    /** Calls synchronously exactly once; preserves the callback result/exception, even when capture is off. */
    scope<T>(label: string, callback: () => T): T;
    /** Finite value; sum resets per captured outer frame, latest is newest timestamp, max is largest observation. */
    counter(label: string, value: number, mode?: "sum" | "latest" | "max"): void;
  };

}
