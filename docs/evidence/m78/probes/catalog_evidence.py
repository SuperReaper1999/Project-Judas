#!/usr/bin/env python3
"""Audit index, not a semantic coverage generator or a product documentation fix.

Records exact source locations, suite assertion text and reverse-discovered menus.
Related-suite links never mean every API member or conditional branch was tested.
"""
import pathlib,json,re,collections
r=pathlib.Path(__file__).resolve().parents[4];e=r/'docs/evidence/m78'
groups={
 'A':('InteractivePlay Simulation RuntimeWorld WorldEntities WorldPresentation SceneSession Prefab ScriptSystem','lifecycle scene prefab scene_transition script script_application script_presentation signal boundary_migration project'),
 'B':('GravityContextMap FaithfulGravity RadicalGravity WorldGravity GravitySelection WorldCoordinates CelestialGravity RigidBodyGravity ReferenceFrame DynamicBody','gravity uniform_gravity_scene gravity_selection reference_frame world_coordinates celestial orbit radial_terrain boundary_gravity'),
 'C':('PhysicsWorld CollisionGeometry CollisionShapes CollisionAsset CollisionAuthoring RigidBody JointTypes Broadphase PhysicalMaterial','physics rigid_contact collision collision_geometry collision_application collision_filter classification broadphase physics_cast physics_batch joint joint_application sleeping kinematic impact_timing contact_geometry physical_material'),
 'D':('CharacterMotor WorldCharacters ModelLoader ModelImport ModelCook SkeletalAnimation PoseComposition WorldAnimation WorldRagdoll FullBodyIK PhysicalAnimation WorldPhysicalAnimation PhysicalBalance WorldPhysicalBalance','character_motor character_motor_application skeletal_animation animation_application pose_composition pose_composition_application ragdoll ragdoll_application multi_target_ik physical_animation physical_balance_foundation physical_balance_controller balance_lab animation_retarget animation_retarget_cook retarget_application retarget_editor retarget_lab model_import model_runtime script_presentation'),
 'E':('FluidWorld ProductionFluidCoupling FluidHydrostatics FluidSurface LiquidSystem LiquidSurface LiquidGeometry LiquidPersistence AtmosphereField CombustionWorld Deformable WorldDeformables Fracture WorldFracture','fluid production_fluid fluid_container_integration player_fluid_integration fluid_demo_integration liquid liquid_surface liquid_surface_application liquid_surface_refill liquid_surface_loop atmosphere atmospheric_flight combustion thermal_accounting deformable deformable_application deformable_coupling fracture fracture_application fracture_performance'),
 'F':('Renderer Material Environment WorldRender WorldPresentation WorldAnimation Camera ParticleEmitter VisualParticles Lighting Shadows Frustum','material material_application runtime_render runtime_render_authoring runtime_render_retained_region runtime_render_application lighting shadow render_target frustum visual_particle presentation script_presentation skeletal_animation'),
 'G':('RadialTerrain TerrainLibrary TerrainSource TerrainCook NavigationAsset NavigationBake NavigationSystem NavigationTypes','radial_terrain terrain_authoring terrain_pipeline terrain_application navigation navigation_game navigation_bake'),
 'H':('AudioSystem AudioStream AudioEnvironment WorldAudio RuntimeUI UIWidgets UIStack InputSystem NamedInput TextLayout Localization ScriptSystem WorldSignals','audio audio_application audio_acoustics runtime_ui runtime_ui_application input paired_input paired_input_script input_scene script script_application signal signal_application text_localization text_render localization_application'),
 'I':('SaveService SaveStorage SaveArchive WorldPersistence MotorPersistence LiquidPersistence WorldState SceneSession WorldStreaming ResourceManager JobSystem AssetDatabase','save_storage save_application save_streaming save_performance world_persistence world_state streaming streaming_application streaming_revisit streaming_performance resource resource_handoff job async_application'),
 'J':('editor authoring NamedAuthoring ModelCook ProjectExporter GamePackage modding','project developer_authoring developer_application developer_integration consumer_authoring editor_navigation editor_picking editor_widget editor_workspace model_import model_reimport model_runtime model_application world_building world_authoring_runtime world_authoring_application terrain_authoring terrain_pipeline terrain_application retarget_editor mod_content mod_application export windows_readiness'),
 'K':('PerformanceProfiler RuntimeDiagnostics RuntimeOptions TestHarness editor/EditorMilestoneNotice','profiler profiler_script profiler_application resource_integrity asset_integrity windows_readiness')}
targets=json.loads((e/'TEST_TARGETS.json').read_text());sources=list((r/'src').rglob('*.cpp'))+list((r/'src').rglob('*.h'))
traces={};suiteIndex=[]
for t in targets:
 p=r/t['source'];lines=p.read_text().splitlines();key=t['target'].removeprefix('judas_').removesuffix('_tests')
 assertions=[{'line':i,'text':line.strip()} for i,line in enumerate(lines,1) if re.search(r'\b(?:Check|check|require|Require|Expect|expect|assert)\s*\(',line) and not re.search(r'\b(?:void|bool)\s+(?:Check|check|require|Require|Expect|expect)\s*\(',line)]
 receipt=e/'linux'/(t['target']+'.json');row=json.loads(receipt.read_text()) if receipt.exists() else None
 suiteIndex.append({'target':t['target'],'source':t['source'],'assertionLocations':assertions,'receipt':str(receipt.relative_to(r)) if row else None,'status':row['status'] if row else 'not-executed','branchCoverage':'Not instrumented. Source text and successful process do not establish every conditional assertion executed.'})
for group,(stems,keys) in groups.items():
 files=[str(p.relative_to(r)) for p in sources if any(p.stem==s or '/'+s+'/' in str(p.relative_to(r)) or str(p.relative_to(r)).endswith('/'+s+'.cpp') for s in stems.split())]
 links=[x for x in suiteIndex if x['target'].removeprefix('judas_').removesuffix('_tests') in keys.split()]
 traces[group]={'sourceCandidates':files,'relatedSuites':[x['target'] for x in links],'scope':'Subsystem source/ownership trace with exact test assertion catalog; not call-graph or exhaustive combination coverage'}
(e/'SOURCE_TEST_INDEX.json').write_text(json.dumps({'groups':traces,'suites':suiteIndex},indent=2)+'\n')
reverse=[]
for p in (r/'src/editor').glob('*.cpp'):
 for i,line in enumerate(p.read_text().splitlines(),1):
  if re.search(r'ImGui::(?:MenuItem|Button|Checkbox)\(',line):reverse.append({'source':str(p.relative_to(r)),'line':i,'text':line.strip(),'evidence':'source-only; current automated authoring/picking tests cover subsets; human gesture/usability remains separate'})
tools=[]
for p in (r/'tools').glob('*.cpp'):
 options=[]
 for i,line in enumerate(p.read_text().splitlines(),1):
  if any(s in line for s in ['usage:','Usage:','argv[','throw std::runtime_error(']):options.append({'line':i,'text':line.strip()})
 tools.append({'source':str(p.relative_to(r)),'argumentLocations':options})
(e/'REVERSE_SURFACES.json').write_text(json.dumps({'editorControls':reverse,'tools':tools,'scope':'Reverse discovery beyond public JS. Controls are grouped under J authoring; raw entries are not additional pass counts.'},indent=2)+'\n')
x=json.loads((e/'CAPABILITIES.json').read_text());js=json.loads((r/'docs/judasjs/api-inventory.json').read_text());script=(r/'src/ScriptSystem.cpp').read_text().splitlines();symbolMap={s['symbol']:s for s in js['symbols']}
family={'Animation':'D','Character':'D','Ragdoll':'D','Joint':'C','physics':'C','Deformable':'E','Fracture':'E','LiquidVolume':'E','liquid':'E','Material':'F','NavigationAgent':'G','navigation':'G','audio':'H','input':'H','UIDocument':'H','UIElement':'H','ui':'H','localization':'H','signals':'H','scenes':'I','session':'I','saves':'I','mods':'J','profiler':'K','EntityGravity':'B'}
for f in x['features']:
 group=f['id'][0] if f['id'][0] in groups else family.get(f['feature'].split('.')[0],'A')
 if f['id'].startswith('JS.'):
  s=symbolMap[f['feature']];native=s.get('native',[]);locations=[]
  for op in native:
   locations.extend({'source':'src/ScriptSystem.cpp','line':i,'text':line.strip()} for i,line in enumerate(script,1) if '"'+op+'"' in line or "'"+op+"'" in line)
  f['implementation'].update(status='present',nativeOperations=native,bridgeLocations=locations,downstreamTrace=traces[group]['sourceCandidates'],traceStatus='binding AST and actual VM surface verified; downstream mechanisms inspected by subsystem; every branch not proven')
  f['contract']['envelope']='Exact declaration/reference at frozen hash; dynamic result/config validation and callback ordering are manual semantic exceptions, as listed in api-inventory.json.'
  f['evidence'].update(linux='partially verified',receipts=['api-live.log','VM_SURFACE.json'],behaviour='Member presence/access-kind verified; associated subsystem assertions do not individually prove this member in every phase/lifetime.',relatedSuites=traces[group]['relatedSuites'])
 elif f['id'].startswith('AUTHOR.'):
  key=f['feature'].split()[0];f['implementation'].update(status='present',sourceLocations=[{'source':'src/Scene.h','line':i,'text':line.strip()} for i,line in enumerate((r/'src/Scene.h').read_text().splitlines(),1) if re.search(r'\b'+re.escape(key)+r'\b',line)],downstreamTrace=['src/SceneSerialization.cpp','src/Prefab.cpp','src/RuntimeWorld.cpp','src/editor/EditorPanels.cpp'],traceStatus='declared authoring surface; component-specific rejection/round-trip evidence in related suite catalog')
  f['evidence'].update(linux='partially verified',relatedSuites=traces[group]['relatedSuites'],scope='No blanket complete-component acceptance; field combinations/editor gesture coverage remain uninstrumented.')
 elif f['id'].startswith('ASSET.'):
  f['implementation'].update(status='present',downstreamTrace=['src/AssetDatabase.cpp','src/ResourceManager.cpp','src/ProjectExporter.cpp'],traceStatus='registered asset family and normal loader/export ownership; format-specific assertions linked in source/test index')
  f['evidence'].update(linux='partially verified',relatedSuites=traces['I']['relatedSuites']+traces['J']['relatedSuites'],scope='Shared resource/export tests plus family-specific suites; not every malformed artifact or cancellation race.')
 else:
  f['implementation'].update(status='present',downstreamTrace=traces[group]['sourceCandidates'],traceStatus='current entry/ownership examined; read SOURCE_TEST_INDEX assertion locations and scoped report')
  receipts=[]
  for target in traces[group]['relatedSuites']:
   p=e/'linux'/(target+'.json')
   if p.exists():receipts.append(str(p.relative_to(r)))
  f['evidence'].update(linux='partially verified',receipts=receipts,scope='Only executed named assertions/input envelopes; breadth does not certify every shape/pair/lifecycle combination.',oracle='Existing suite assertions/catalog plus independent M78 probes where explicitly attached; no universal independence claim')
  f['contract']['envelope']='Frozen implementation and current subsystem guide; report records historical removals, default-off features, numerical approximations and unsupported combinations.'
 # A read-only audit never manufactures operator acceptance from automated checks.
 f['acceptance']={'human':'Prior acceptance retains its original hash/scenario; M67 specified review, M74, M76 and M77 pending','hardware':'Physical Windows GPU/input/audio pending; no VM inference'}
 f['evidence']['windows']='not-executed for this individual capability unless separately attached in final report'
x.update(status='PARTIAL - source/test/reverse-surface accounting established; final adjudication pending',denominatorWarning='485 overlapping discovery rows, comprising 89 mechanism groups, 345 API members, 34 authored components and 17 asset families. These are separate denominators, never summed into an engine pass percentage. Related-suite links are scope navigation, not per-member behavioural certification.')
(e/'CAPABILITIES.json').write_text(json.dumps(x,indent=2)+'\n')
print(json.dumps({'rows':len(x['features']),'mechanismGroups':89,'apiMembers':345,'components':34,'assets':17,'testSources':len(suiteIndex),'sourceAssertionLocations':sum(len(s['assertionLocations']) for s in suiteIndex),'editorControlLocations':len(reverse),'toolSources':len(tools)}))
