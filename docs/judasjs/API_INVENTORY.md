# Current public JudasJS inventory

M70 candidate multi-target IK and partial physical animation review of the registered virtual module based on accepted starting checkpoint `934c5d3f0556c920cc7cae8b80dc4677d8cbf87b`. M67 human authoring review remains deferred.
Each row is a runtime export/member (constructors and plain handle fields included).
Native dispatcher operations are implementation details, not additional JS APIs.

| Runtime symbol | Native bridge operation | Type | Reference |
|---|---|---|---|
| `Animation` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.clips` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.configureIK` | `animationIKConfigure` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.crossFade` | `animationFade` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.entityId` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.ikStatus` | `animationIKStatus` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.ikTargets` | `animationIKTargets` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.info` | `animationInfo` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.jointTransform` | `animationJointPose` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.layer` | `animationLayer` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.layers` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.limb` | `animationLimb` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.loop` | `animationSet` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.pause` | `animationPause` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.play` | `animationPlay` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.playing` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.removeLayer` | `animationRemoveLayer` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.removeLimb` | `animationRemoveLimb` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.resume` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.rootMotion` | `animationRootMotion` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.seek` | `animationSeek` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.speed` | `animationSet` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.stop` | `animationStop` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.time` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Character` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.accelerate` | `characterAcceleration` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.actualDisplacement` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.configure` | `characterConfigure` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.enabled` | `characterEnabled` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.gravity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.ignore` | `characterIgnore` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.state` | `characterState` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.supportNormal` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.supportVelocity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.supported` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.up` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.velocity` | `characterVelocity` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Deformable` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.attach` | `deformableAttach` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.enabled` | `deformableEnabled` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.epoch` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.force` | `deformableForce` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.impulse` | `deformableImpulse` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.impulseAt` | `deformableHitImpulse` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.raycast` | `deformableRaycast` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.release` | `deformableRelease` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.reset` | `deformableReset` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.setMaterial` | `deformableMaterial` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.state` | `deformableState` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Deformable.valid` | `deformableValid` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Entity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.addTag` | `addTag` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.angularVelocity` | `angularVelocity`, `setAngularVelocity` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.animation` | `animationExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyForce` | `force` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyImpulse` | `impulse` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyImpulseAtPoint` | `impulseAtPoint` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyTorque` | `torque` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.audio` | `audioInfo` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.burst` | `burst` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.camera` | `cameraInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.character` | `characterExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.children` | `children` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.classification` | `classification` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.clearSocket` | `socketClear` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.collider` | `colliderInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.deformable` | `deformableExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.destroy` | `destroy` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.fracture` | `fractureExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.hasTag` | `hasTag` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.inertiaWorld` | `inertiaWorld` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.liquid` | `liquidOwner` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.mass` | `mass` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.material` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Entity.modelParts` | `modelParts` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.navigation` | `navAgentExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.navigationLink` | `navLinkInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.navigationObstacle` | `navObstacleInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.parent` | `parent` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.pauseAudio` | `pauseAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.physicalMaterial` | `physicalMaterialInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.playAudio` | `playAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.playAudioOneShot` | `audioOneShot` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.presentedTransform` | `presentedTransform` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.ragdoll` | `ragdollExists` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.removeTag` | `removeTag` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.resumeAudio` | `resumeAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.scriptState` | `scriptState` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.seekAudio` | `audioSeek` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudio` | `audioSettings` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudioEnabled` | `audioEnabled` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudioVelocity` | `audioVelocity` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setCameraEnabled` | `camera` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setCameraProjection` | `cameraProjection` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `Entity.setColliderEnabled` | `colliderEnabled` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setNavigationEnabled` | `navEnabled` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setPartVisible` | `modelPartVisible` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setParticles` | `particles` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setPhysicalMaterial` | `physicalMaterialSet` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setSocket` | `socketSet` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.sleeping` | `sleeping` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.stopAudio` | `stopAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.transform` | `setTransform`, `transform` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.valid` | `valid` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.velocity` | `setVelocity`, `velocity` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Fracture` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.epoch` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.force` | `fractureForce` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.impulse` | `fractureImpulse` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.release` | `fractureRelease` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.remove` | `fractureRemove` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.state` | `fractureState` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Fracture.valid` | `fractureValid` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Joint` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.configure` | `jointConfigure` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.destroy` | `jointDestroy` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.setEnabled` | `jointSet` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.setLimits` | `jointSet` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.setMotor` | `jointSet` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.setSpring` | `jointSet` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.state` | `jointState` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Joint.valid` | `jointValid` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `LiquidVolume` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.applyImpulse` | `liquidImpulse` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.enabled` | `liquidEnabled` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.handle` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.state` | `liquidState` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.surfaceEnabled` | `liquidSurfaceEnabled` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.transferTo` | `liquidTransfer` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `LiquidVolume.valid` | `liquidValid` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `Material` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.assign` | `materialAssign` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.clearOverrides` | `materialClear` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.entityId` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.set` | `materialOverride` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.slot` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Material.state` | `materialInfo` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `NavigationAgent` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.clear` | `navClear` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.completeLink` | `navCompleteLink` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.configure` | `navConfigure` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.enabled` | `navEnabled` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.remainingDistance` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.setDestination` | `navDestination` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.state` | `navAgentState` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.steering` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `NavigationAgent.stopped` | `navStopped` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `Ragdoll` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.active` | `ragdollActive` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.body` | `ragdollBody` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.configurePhysical` | `ragdollPhysicalConfigure` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.enabled` | `ragdollEnabled` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.enter` | `ragdollEnter` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.leave` | `ragdollLeave` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.physicalState` | `ragdollPhysicalState` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.receiveContactEvents` | `ragdollContactEvents` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.setMode` | `ragdollMode` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `UIDocument` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.enabled` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.get` | `uiGet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.handle` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.hide` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.modal` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.show` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.unload` | `uiUnload` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.visible` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.direction` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.enabled` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.font` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.handle` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.layout` | `uiLayout` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.setLayout` | `uiLayout` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.text` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.textAlignment` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.textKey` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.texture` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.value` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.visible` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `audio` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](audio.md) |
| `audio.diagnostics` | `audioDiagnostics` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `audio.group` | `audioGroup` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `audio.setGroup` | `audioGroupSet` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `console` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](input.md) |
| `console.log` | `log` | [declaration](../judas.d.ts) | [reference](input.md) |
| `entity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `input` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.axis` | `axis` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.held` | `held` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.pointerCapture` | `pointerCapture`, `setPointerCapture` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.pressed` | `pressed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.released` | `released` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stick` | `stick` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stickDelta` | `stickDelta` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stickSamples` | `stickSamples` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.vector` | `vector` | [declaration](../judas.d.ts) | [reference](input.md) |
| `liquid` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.accounting` | `liquidAccounting` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.connections` | `liquidConnections` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.errors` | `liquidErrors` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.sample` | `liquidSample` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.samplePresented` | `liquidPresentedSample` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `liquid.submerged` | `liquidSubmerged` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `localization` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.available` | `localeInfo` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.direction` | `localeInfo` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.format` | `localeFormat` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.locale` | `localeInfo` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.number` | `localeNumber` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.reload` | `localeReload` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.revision` | `localeInfo` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `localization.setLocale` | `localeChoose` | [declaration](../judas.d.ts) | [reference](localization.md) |
| `navigation` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.areas` | `navAreas` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.errors` | `navErrors` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.path` | `navPath` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.profiles` | `navProfiles` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.raycast` | `navRaycast` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.sample` | `navSample` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `physics` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.boxCast` | `cast` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.capsuleCast` | `cast` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.capsuleCastMany` | `castMany` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.closestPoint` | `closestPoint` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.createJoint` | `jointCreate` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.gravity` | `gravitySample` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.joint` | `joint` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.raycast` | `cast` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.raycastMany` | `castMany` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.sphereCast` | `cast` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `physics.sphereCastMany` | `castMany` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `profiler` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](profiling.md) |
| `profiler.counter` | `profileCounter` | [declaration](../judas.d.ts) | [reference](profiling.md) |
| `profiler.scope` | `profileScope` | [declaration](../judas.d.ts) | [reference](profiling.md) |
| `saves` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.cancel` | `saveCancel` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.delete` | `saveRequest` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.exclude` | `saveExclude` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.exists` | `saveList` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.list` | `saveList` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.load` | `saveRequest` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.reference` | `saveReference` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.refresh` | `saveRequest` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.resolve` | `saveResolve` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.save` | `saveRequest` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `saves.status` | `saveStatus` | [declaration](../judas.d.ts) | [reference](saves.md) |
| `scenes` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `scenes.activateRegion` | `regionActivate` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.adopt` | `regionAdopt` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.current` | `sceneCurrent` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `scenes.load` | `sceneLoad` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `scenes.owner` | `regionOwner` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.pinRegion` | `regionPin` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.regionStatus` | `regionStatus` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.regions` | `regionList` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.registered` | `sceneList` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `scenes.releaseRegion` | `regionRelease` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.reload` | `sceneReload` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `scenes.removeInterest` | `regionRemoveInterest` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.requestRegion` | `regionRequest` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.resolveRegionEntity` | `regionResolve` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.setInterest` | `regionInterest` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.streamingStats` | `regionStats` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `scenes.unloadRegion` | `regionUnload` | [declaration](../judas.d.ts) | [reference](streaming.md) |
| `session` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `session.delete` | `sessionDelete` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `session.get` | `sessionGet` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `session.set` | `sessionSet` | [declaration](../judas.d.ts) | [reference](scenes-state.md) |
| `time` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.delta` | `delta` | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.elapsed` | `elapsed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.fixed` | `fixed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `ui` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.debugOverlayVisible` | `uiDiagnostics` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.get` | `uiFind` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.load` | `uiLoad` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.quit` | `uiQuit` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `world` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.appearance` | `appearanceInfo` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `world.clearView` | `clearView` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.entity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.fluidSample` | `fluidSample` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.overlap` | `overlap` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.project` | `projectViewport` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.queryTags` | `queryTags` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.setAppearance` | `appearanceSet` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `world.setView` | `setView` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.spawnPrefab` | `spawn` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.sweepCapsule` | `sweep` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.viewRay` | `viewRay` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.viewport` | `viewportSize` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |

## Lifecycle and dynamic exceptions

`onFracture`, `start`, `restore`, `update`, `fixedUpdate`, `uiUpdate`, `presentationUpdate`, `destroy`, `onUI`, `onCollisionEnter`, `onCollisionStay`, `onCollisionExit`, `onTriggerEnter`, `onTriggerStay`, `onTriggerExit` are structural ScriptBehaviour callbacks, not module exports.

- **globalThis.__judas**: Internal native dispatcher; unsupported, not a public API declaration.
- **globalThis.console**: Alias of exported console; no extra API.
- **result objects / config / property schema**: Native structured fields (including JointConfiguration.rotationalResistance) and value validation reviewed manually; representative runtime/type examples cover shapes, not every invalid value.
- **types-only exports**: Interfaces/type aliases are tooling only; runtime export comparison excludes them.
- **setter-only accessors**: TypeScript cannot prohibit reads; docs state these return undefined.
- **callback ordering / phases**: Implementation traced manually; no AST checker proves temporal semantics.

The machine check fails missing/phantom exports/members, getter/setter drift and inventory drift.
Structured return/configuration types and behavioural semantics require source review; it is not full semantic certification.
