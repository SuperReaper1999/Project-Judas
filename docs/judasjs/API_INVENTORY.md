# Current public JudasJS inventory

Current public surface identity: `Pre-Alpha current source; public module through M99`. Includes optional network sessions and NetworkEntity handles; see [networking](network.md). Human/platform acceptance is recorded separately in [the current documentation](../JUDASJS.md), not inferred from API coverage.
Each row is a runtime export/member (constructors and plain handle fields included).
Native dispatcher operations are implementation details, not additional JS APIs.

| Runtime symbol | Native bridge operation | Type | Reference |
|---|---|---|---|
| `Animation` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.advance` | `animationAdvance` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
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
| `Animation.sample` | `animationSample` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.seek` | `animationSeek` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.speed` | `animationSet` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.stop` | `animationStop` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Animation.time` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Character` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.accelerate` | `characterAcceleration` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.actualDisplacement` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.configure` | `characterConfigure` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.displace` | `characterDisplacement` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.enabled` | `characterEnabled` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.gravity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.ignore` | `characterIgnore` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Character.orient` | `characterOrientation` | [declaration](../judas.d.ts) | [reference](character.md) |
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
| `Entity.animation` | `animationExists` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Entity.applyForce` | `force` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyImpulse` | `impulse` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyImpulseAtPoint` | `impulseAtPoint` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.applyTorque` | `torque` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.audio` | `audioInfo` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.burst` | `burst` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.camera` | `cameraInfo` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `Entity.character` | `characterExists` | [declaration](../judas.d.ts) | [reference](character.md) |
| `Entity.children` | `children` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.classification` | `classification` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.clearPlanetCollision` | `planetClear` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `Entity.clearSocket` | `socketClear` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.collider` | `colliderInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.deformable` | `deformableExists` | [declaration](../judas.d.ts) | [reference](deformables.md) |
| `Entity.destroy` | `destroy` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.fracture` | `fractureExists` | [declaration](../judas.d.ts) | [reference](fracture.md) |
| `Entity.gravity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `Entity.hasTag` | `hasTag` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.inertiaWorld` | `inertiaWorld` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.kinematicMotion` | `kinematicMotion` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.light` | `lightInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.liquid` | `liquidOwner` | [declaration](../judas.d.ts) | [reference](liquid.md) |
| `Entity.mass` | `mass` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.material` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](materials.md) |
| `Entity.modelParts` | `modelParts` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.motionType` | `motionType` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.moveKinematic` | `kinematicTarget` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.navigation` | `navAgentExists` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `Entity.navigationLink` | `navLinkInfo` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `Entity.navigationObstacle` | `navObstacleInfo` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `Entity.parent` | `parent` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.pauseAudio` | `pauseAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.physicalMaterial` | `physicalMaterialInfo` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.planetSample` | `planetSample` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `Entity.planetState` | `planetInfo` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `Entity.playAudio` | `playAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.playAudioOneShot` | `audioOneShot` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.pointVelocity` | `pointVelocity` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.powertrain` | `powertrainExists` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Entity.preparePlanetCollision` | `planetCollision` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `Entity.presentedTransform` | `presentedTransform` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.ragdoll` | `ragdollExists` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Entity.removeTag` | `removeTag` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.renderVisible` | `renderVisible` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.rendererVisible` | `rendererVisible` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.resumeAudio` | `resumeAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.scriptState` | `scriptState` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.seekAudio` | `audioSeek` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudio` | `audioSettings` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudioEnabled` | `audioEnabled` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setAudioVelocity` | `audioVelocity` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.setCameraEnabled` | `camera` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setCameraProjection` | `cameraProjection` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `Entity.setColliderEnabled` | `colliderEnabled` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setKinematicVelocity` | `kinematicVelocity` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setLight` | `lightSettings` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setMotionType` | `motionTypeSet` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setNavigationEnabled` | `navEnabled` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `Entity.setPartVisible` | `modelPartVisible` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setParticles` | `particles` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setPhysicalMaterial` | `physicalMaterialSet` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.setSocket` | `socketClear`, `socketSet` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.sleeping` | `sleeping` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.stopAudio` | `stopAudio` | [declaration](../judas.d.ts) | [reference](audio.md) |
| `Entity.stopKinematic` | `kinematicStop` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.terrain` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Entity.transform` | `setTransform`, `transform` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.valid` | `valid` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.velocity` | `setVelocity`, `velocity` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `Entity.weather` | `weatherExists` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Entity.wheel` | `wheelExists` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `EntityGravity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.acceleration` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.clear` | `entityGravityClear` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.select` | `entityGravitySelect` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.setUniform` | `entityGravityUniform` | [declaration](../judas.d.ts) | [reference](physics.md) |
| `EntityGravity.state` | `entityGravityState` | [declaration](../judas.d.ts) | [reference](physics.md) |
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
| `LocalPlayer` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.assign` | `playerAssign` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.clearView` | `playerView` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.devices` | `playerInfo` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.entity` | `playerEntity` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.input` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.leave` | `playerRemove` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.name` | `playerInfo` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.project` | `playerProject` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.ray` | `playerRay` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.setInputMap` | `playerMap` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.setView` | `playerView` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.unassign` | `playerUnassign` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.valid` | `playerExists` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.viewError` | `playerInfo` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `LocalPlayer.viewport` | `playerViewport` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
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
| `NetworkEntity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.entity` | `networkEntry` | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.forget` | `networkForget` | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.generation` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.owner` | `networkEntry`, `networkOwner` | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.state` | `networkEntry`, `networkState` | [declaration](../judas.d.ts) | [reference](network.md) |
| `NetworkEntity.valid` | `networkValid` | [declaration](../judas.d.ts) | [reference](network.md) |
| `PlayerInput` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.axis` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.held` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.pointer` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.pressed` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.released` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.stick` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.touches` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `PlayerInput.vector` | `playerInput` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `Powertrain` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.enabled` | `powertrainEnabled` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.set` | `powertrainSet` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.settings` | `powertrainSettings` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.state` | `powertrainState` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Powertrain.valid` | `powertrainValid` | [declaration](../judas.d.ts) | [reference](powertrains.md) |
| `Ragdoll` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.active` | `ragdollActive` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.balanceDiagnostics` | `ragdollBalanceDiagnostics` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.balanceState` | `ragdollBalanceState` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.body` | `ragdollBody` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.cancelBalance` | `ragdollBalanceCancel` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.configureBalance` | `ragdollBalanceConfigure` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.configurePhysical` | `ragdollPhysicalConfigure` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.enabled` | `ragdollEnabled` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.enter` | `ragdollEnter` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.leave` | `ragdollLeave` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.physicalState` | `ragdollPhysicalState` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.receiveContactEvents` | `ragdollContactEvents` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.requestBalance` | `ragdollBalanceRequest` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Ragdoll.setMode` | `ragdollMode` | [declaration](../judas.d.ts) | [reference](animation-ragdolls.md) |
| `Terrain` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Terrain.brush` | `terrainBrush` | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Terrain.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Terrain.create` | `terrainCreate` | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Terrain.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `Terrain.state` | `terrainState` | [declaration](../judas.d.ts) | [reference](terrain.md) |
| `UIDocument` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.accessibility` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.captureState` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.enabled` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.focus` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.focused` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.get` | `uiGet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.handle` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.hide` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.modal` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.player` | `uiPlayer` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.referenceSize` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.restoreState` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.scale` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.show` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.unload` | `uiUnload` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIDocument.visible` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.configure` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.direction` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.enabled` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.font` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.handle` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.layout` | `uiLayout` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.scroll` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.selectedIndex` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.selection` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.setItems` | `uiControl` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.setLayout` | `uiLayout` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.text` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.textAlignment` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.textKey` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.texture` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.value` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `UIElement.visible` | `uiGet`, `uiSet` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `Weather` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.enabled` | `weatherEnabled` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.state` | `weatherState` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.strike` | `weatherStrike` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.transition` | `weatherTransition` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Weather.valid` | `weatherExists` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `Wheel` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.configure` | `wheelConfigure` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.constructor` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.enabled` | `wheelEnabled` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.id` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.set` | `wheelSet` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.settings` | `wheelSettings` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.state` | `wheelState` | [declaration](../judas.d.ts) | [reference](wheels.md) |
| `Wheel.valid` | `wheelValid` | [declaration](../judas.d.ts) | [reference](wheels.md) |
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
| `input.pointer` | `pointer` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.pointerCapture` | `pointerCapture`, `setPointerCapture` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.pressed` | `pressed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.released` | `released` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stick` | `stick` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stickDelta` | `stickDelta` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.stickSamples` | `stickSamples` | [declaration](../judas.d.ts) | [reference](input.md) |
| `input.touches` | `touches` | [declaration](../judas.d.ts) | [reference](input.md) |
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
| `mods` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](mods.md) |
| `mods.active` | `modCatalog` | [declaration](../judas.d.ts) | [reference](mods.md) |
| `mods.contributions` | `modCatalog` | [declaration](../judas.d.ts) | [reference](mods.md) |
| `mods.resolve` | `modCatalog` | [declaration](../judas.d.ts) | [reference](mods.md) |
| `navigation` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.areas` | `navAreas` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.errors` | `navErrors` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.path` | `navPath` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.profiles` | `navProfiles` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.raycast` | `navRaycast` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `navigation.sample` | `navSample` | [declaration](../judas.d.ts) | [reference](navigation.md) |
| `network` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.close` | `networkClose` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.entities` | `networkEntities`, `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.error` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.host` | `networkHost` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.input` | `networkInput` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.join` | `networkJoin` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.launch` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.mode` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.peerId` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.peers` | `networkPeers` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.port` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.replicate` | `networkInfo`, `networkReplicate` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.sendInput` | `networkSendInput` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.setInterest` | `networkInterest` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.stats` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
| `network.status` | `networkInfo` | [declaration](../judas.d.ts) | [reference](network.md) |
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
| `players` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `players.create` | `playerCreate` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `players.devices` | `playerDevices` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `players.get` | `playerExists` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
| `players.list` | `playerList` | [declaration](../judas.d.ts) | [reference](local-multiplayer.md) |
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
| `signals` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](signals.md) |
| `signals.emit` | `signalEmit` | [declaration](../judas.d.ts) | [reference](signals.md) |
| `signals.send` | `signalSend` | [declaration](../judas.d.ts) | [reference](signals.md) |
| `signals.stats` | `signalStats` | [declaration](../judas.d.ts) | [reference](signals.md) |
| `signals.subscribe` | `signalSubscribe` | [declaration](../judas.d.ts) | [reference](signals.md) |
| `signals.unsubscribe` | `signalUnsubscribe` | [declaration](../judas.d.ts) | [reference](signals.md) |
| `time` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.delta` | `delta` | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.elapsed` | `elapsed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `time.fixed` | `fixed` | [declaration](../judas.d.ts) | [reference](input.md) |
| `ui` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.debugOverlayVisible` | `uiDiagnostics` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.get` | `uiFind` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.load` | `uiLoad` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `ui.quit` | `uiQuit` | [declaration](../judas.d.ts) | [reference](ui.md) |
| `weather` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](weather.md) |
| `weather.quality` | `weatherQuality` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `weather.sample` | `weatherSample` | [declaration](../judas.d.ts) | [reference](weather.md) |
| `world` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.appearance` | `appearanceInfo` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `world.clearView` | `clearView` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.entity` | JS wrapper/data | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.fluidSample` | `fluidSample` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.origin` | `worldOrigin` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `world.originState` | `worldOriginState` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `world.overlap` | `overlap` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.project` | `projectViewport` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.queryTags` | `queryTags` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.requestOrigin` | `worldOriginRequest` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `world.resetAppearance` | `appearanceReset` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `world.setAppearance` | `appearanceSet` | [declaration](../judas.d.ts) | [reference](materials.md) |
| `world.setView` | `setView` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.spawnPrefab` | `spawn` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.sweepCapsule` | `sweep` | [declaration](../judas.d.ts) | [reference](entities.md) |
| `world.toAbsolute` | `worldAbsolute` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `world.toLocal` | `worldLocal` | [declaration](../judas.d.ts) | [reference](planetary-terrain.md) |
| `world.viewRay` | `viewRay` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |
| `world.viewport` | `viewportSize` | [declaration](../judas.d.ts) | [reference](effects-camera.md) |

## Lifecycle and dynamic exceptions

`onOriginShift`, `onSignal`, `onFracture`, `start`, `restore`, `update`, `fixedUpdate`, `uiUpdate`, `presentationUpdate`, `destroy`, `onUI`, `onCollisionEnter`, `onCollisionStay`, `onCollisionExit`, `onTriggerEnter`, `onTriggerStay`, `onTriggerExit` are structural ScriptBehaviour callbacks, not module exports.

- **globalThis.__judas**: Internal native dispatcher; unsupported, not a public API declaration.
- **globalThis.console**: Alias of exported console; no extra API.
- **result objects / config / property schema**: Native structured fields (including JointConfiguration.rotationalResistance, M83 DeformableAttachmentOptions/observations and PowertrainConnection/connected) and value validation reviewed manually; representative runtime/type examples cover shapes, not every invalid value.
- **types-only exports**: Interfaces/type aliases are tooling only; runtime export comparison excludes them.
- **setter-only accessors**: TypeScript cannot prohibit reads; docs state these return undefined.
- **callback ordering / phases**: Implementation traced manually; no AST checker proves temporal semantics.
- **callable argument counts**: Wrapper/declaration minimum and maximum counts (including aliases, rest arguments and overload bounds) are checked. Entity.setSocket has a null-target branch and Weather.strike explicitly accepts native undefined. Parameter types, overload gaps and native validation still require source/behavioural review.

The machine check fails missing/phantom exports/members, getter/setter drift, callable argument-count drift and inventory drift.
The manifest also records callable bounds and explicit argument-count exceptions. Structured return/configuration types and behavioural semantics require source review; it is not full semantic certification.
