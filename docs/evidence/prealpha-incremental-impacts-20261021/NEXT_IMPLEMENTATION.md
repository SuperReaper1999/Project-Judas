# Authorized ongoing redesign: implementation state and next architecture

Current production Experimental HEAD6059bafb clean tracked. Main3ecf26c untouched.
User explicitly authorizes proper redesign and thorough tests. No subagents allowed.
All code so far PRIVATE cache overlay only; earlier investigation docs/evidence untracked.

## Private prototypes
- defect-v1: ImpactContact.normalVelocityDefect (default0); ContactSolver.SetInelasticVelocityTarget; ImpactSolver validates negative targets consume <=existing5mm over dt, no positive targets. Offline replay all 10 native originals exact; own reference matching idealized: selectedzero, nonseed accepted residual => changed bodies 810->3(event18),751->0(event28), all energy guards pass. Strict remaining-current-penetration allowance gives almostfullglobal changes; no claimed adoption.
- world-v1: immutable global initial reference momentum field; initialrecord bookkeeping StartContact.partA/B. Mean40060.45ms vs6059 fresh67.35, endpoint24.92mm vs6.60mm; lacks cap of reference error and accidentally enrolls speculative gaps. Rejected version preserved.
- world-v2: per-contact feature birth epochs; zero-error records kept, immutable maximum negative initial defect, speculative gaps excluded; newly accepted actual contacts registered only once after full trial. Uses existing warm match .05m dual anchors and .9local normal dots. Per-feature reference bodies (velocities/q) transported angularmomentum onto current orientation; negative reference target capped by original accepted defect, bounded5mm/dt. Selected seed always target0. Mean40061.61/p95170.71/worst227.11; endpoint27.46mm. Needs actual physical alltime qualification, not claimed correct/perf complete.
- world-v3: same physical law asv2 plus exact active-row scheduling in ContactSolver. Extracted original row arithmetic into SolveVelocityConstraint shared with full scan. CSR dynamic-body adjacency excludes static hub, active ordered heaps retain exact row order +10sweeps. Self/neighbor requeued only when row impulse state changes. Dense fallback scans SAME remaining rows/iterations, representation-only. No physics cutoff. 300 randomized full-vs-active cases (1..12sweeps, arbitraryq/masses/friction, zero/nonzero bias/warm) pass velocitybit equality and impulse numeric equality, 878345->502791 visits. Raw ROW_ORACLE.json. Full world-v3 400 currently running session92991; compare allphysical/query counts againstv2, expect exact parity. Need signed-zero constraint caveat documented: zero impulse cache sign may differ but body bits must exact.

build.py in each overlay replaces ContactSolver.cpp.o,ImpactSolver.cpp.o,PhysicsWorld.cpp.o,WorldImpactEvents.cpp.o in exact frozen6059-corec724... archive, links original benchmark. Private WorldInternal.h copies at root andphysics/, StartContact appendpart fields no Implobjectlayout change(vectorlayout same). Normaldefault newfield ImpactContact changes sizeof only explicit source usages WorldImpactEvents +testsImpactSolver must recompile; otherbenchmarkobjects don'tuseImpactContact. Existingproduction core/binaries NOT modified.

## Essential next step: lazy contact-domain transactional response

Do NOT stop at another10% cleanup. Full touching-graph discovery remains dominant.
Implement staged incremental domain expansion at one immutable eventtime, with
trial RigidBody copies. No World motion/sleep/cache publication before success.
No numerical cutoffs or body caps. This avoids complex append-during-PGS logic.

1. Add optional ACTIVATION tracking to ContactSolver incremental solve: collect every dynamic body receiving any nonzero row impulse/state change DURING iterations, not just bodies whose final velocity differs. A body may transmit impulses and return to original velocity; final-net-only expansion would miss that. Track static bodies never; no propagation across infinite mass wall. Ensure dense fallback tracks too. Keep existing SolveVelocities signature, add SolveTrackedVelocities or optional new method. Active method optional vector<RigidBody*>*. Exact arithmetic/order/10 sweeps retained. Dedup via existing frame IDs, optional vectorseen. Existing ImpactSolver.Solve may add optional activated vectorpointer after joints arg(defaultnullptr), passes through. Safety fallback result triggers legacy path for transactional mode, not trial acceptance.

2. New cohesive private WorldIncrementalImpact module, not another giant World function. Nested Impl::IncrementalImpact transaction declared in WorldInternal and defined in WorldIncrementalImpact.h included AFTERWorldInternal; contains trialBodies vector and incomingBodies sizedWorldSlots (stable addresses), initialized/expanded/gravityConsumed flags, slots, pair visited set, rows, observationKeys, eventPairs, and LOCAL ImpactSolver declared AFTER vectors so destructor never dereferences dead scratch. Local solver used for newmode; existing member impactSolver remains for legacy. Common publication reads responseSolver->Constraints(). No scratch pointers survive AdvanceImpacts.

3. TryIncrementalImpact(selected,now,dt,reference,transaction) does not mutate World physical state. Query canonical selected pair explicitly; expand BOTH seed bodies' current actual contacts before solve. Query each expanded body through QuerySlots(ActualImpactBound(...ImpactPoseAt...)); use CanCollide/layers/sensors/queryOnly/true-static guards instead of CanRespond (world sleeping flags cannot hide contacts of newly activated trial bodies). PairAt allparts canonicalascending; fixedeventgeometry; set visitedbodypair identity to avoidduplicate rows. Unknown/uncertain/unsupported actualtouch => fallbackwholelegacy. Limitinitialsupportto free simpleBox/Sphere, staticbox ground allowed. Any actually touched dynamic joint member, kinematic, mesh/asset/non-simple body =>fallback. Could prebuild activejointmaskonce (includesworld-anchor joints!) or testincident list; no full island preflight required. No World Wake/Observe/motion changes in failed trial.

4. Initialize scratch body from World PoseAt(now) and originalstate, with physical angulartransport. Sleeping neighbor initially gets NO deferred gravity until expanded/actuallyimpulsed; expanding markswake and consumes remainingdt gravity into INCOMING snapshot ONCE, causing restart. Otherwise passive undiscovered neighbor could fall through unseen support from unjustified gravity consumption. Seeds considered expanded/awake candidates. Copy actualevent poses, capture originalpre-gravityvel forlaterChangeVelocity detection.

5. Freeze ordered rowset; assign normal targets from immutable StepContactReference for matched old features, selectedseedalwayszero; unknown/newfeatureszero. Only apply model where capture ormultiplecontacts givesinelastic; elastic isolated casefallbackoldpolicy. Set resultcapture if actualdiscovered cachedsupport, preservesexistingambiguity. Trial body refs, not World refs, go to solver. Reset all initialized trial bodies from incoming snapshots BEFORE EVERY domain solve. Solve full10sweeps on known rows using activekernel + collect EVERY activated body. Newly activated unexpanded bodies expand their geometry and possibly consume deferred gravity; then RESTART trial from incoming with enlarged rowset. Monotone finite expansion, no arbitrary bound. Accept only when EVERY activated body expanded. Entire solvedphysicaldomain gets all10 sweeps in final trial, not partialbudgetfornewconstraints. New undiscovered untouched rest constraints do not get another independent rest solve. If any hardjoint/unsupported feature orenergyfallback encountered, discard copies and execute original full legacy island path. This fallback never rolls back mutated World because trial is private.

6. Commit success at same eventtime: Wake expanded sleeping dynamics via existing Wake (maywakeconnectedrest graph asbefore); mark gravityConsumed false on originals. Publish all initialized eventposes+incoming velocities for Observe pre-response relativevelocity; Observe/lastStepContacts canonicalrows, then publish POSTtrial velocities. island/oldVelocities common arrays include dynamic scratchslots only, static notjoined. included=1 fordynamicdiscovered slots. Originalcache/history/support/motion/frontend invalidation then common publication; changed onlyexactactualvelocitychange. Preserve full legacy code in elsebranch. responseSolver points LOCAL trialsolver forobserved/cacherows. Common statsimpactCounts incrementdynamics solveddomain, capstillonlyinelasticnot skip. Register previously unseen contacts' accepted postsolve residuals ONCE aftercommit, never trial/fallback contamination. Existingepochsnever refreshed.

7. Once physicalmodel/schema-qualified, extract tidy components into real src modules and full normalReleasebuild, not shipping cache-overlay code. Optional engineonly capabilities no new gameplayflags, no JSbindingsneeded.

## Required qualification
- Exactrow dependency proof: 300random + sharedstatic-ground independentassemblies + longtrueimpactchain + signedzero/zero-bodyΔ but nonzeroλ cases. Full default-target world parity against6059 is independent refactor check.
- Accepted reference epoch/lifetime tests: zero-effectarrival=>no unrelatedresponse, genuinechain transmits, invalidlarge residual fallback, no accumulating/rebaselining defect, speculativegaps never admitted, changedfeature/opposedsurface rejects, stepreset, no historic impulse double-count, arbitraryorientation.
- Nativebodyenergy/momentum/angularmomentum, actualpenetration alltime—not justonechaoticendpoint. No testthresholdweakening. Existing curvedmesh372, CCDtiming/robustness, contacts,ragdolls/joints/motor/kinematic, M70/M77 actualcoldendpoints, lifecycle/save/stream; repeated determinism.
- Frozenoriginalroom1024 short+600long +visible1FPSmatchedrun, same60Hz/objects/materials/CCD. Record median/p95/worst, events/queries, activated/domain sizes and memory. No objectcountcuts, forcesleep, lowcadence orarbitraryimpulsecutoffs.
- Mainuntouched, protectedhistory/independentresearch untouched. Existing8GB =>serialcompiles/benchmarks; noWindows/Androidworkloads. Oldinvestigationevidence untracked mustpreserve.

Potential model risk: deferring opportunistic extra globalPGS can change trajectories and observeddepth. This is an explicit numerical splitting proposal, NOT yetqualified; if physicalaccuracy gates fail, correct mechanism, do not hide with largerallowances. Current state allnewsourceincache, no production modification/commit/push.

## Progress update after NEXT plan was written
- world-v3 full400 finishes: ALL physical/work CSV fields (including all400 digests) EXACTLYmatch v2. Mean63.964vs61.607ms, activeworklist alone slower slightly; no claimed speedup.
- world-v4 implements the transactional monotonically expanding domain with RESTART trials, newly activating bodies tracked DURING every constraint iteration. NewmoduleWorldIncrementalImpact.(cpp/h), newContactSolver tracked/full/active methods optionalpointer, ImpactSolver::Solve has optional activated-bodyvector. Private World coordinator commonlegacy else maintained, solverpointer directs history/cache tolocalsolver. Corebuild succeeds. All400 completed but132.455mean/647.612p95/1022.256worst ms! Restartscosttoo much. Endpoint19.214mm. Rejected iterationmethod, preservedsources/logs; do notship.
- world-v5 removes all domain restarts using ContactSolver::PrepareAppended(dt,firstRow,firstFrame) and SolveTrackedVelocityRange(first,1,...). ImpactSolver::SolveGrowing preparesinitialrows, retainsALREADYAPPLIEDimpulses atSAMEphysicaleventtime, processes10GSsweeps. Eachsweep: solveknownrangeonce, expandallactivatedbodies' geometry, appendpreparednewrowswithzeroimpulses, solveonlynewrangeatendofSAMEsweep; repeatnewrowsuntilnone. Newframespreparedonceexistingframesreusedfixedposeepoch. Everyactivationincludestransientnonzeroimpulses—notjustnetvelocity. Growingenergybefore remembersallbodiesBEFOREfirstimpulse (sameorderasContactSolverframes;assertmapping), newbodiesaddedafterlaterwaves were neverimpulsedearlier. Staticbodiesmustzero prescribedvelocity; elsefallbackold. Isolatedelastic, kinematic, nonprimitive/mesh/asset, dynamichardjoint =>fallbackoldphysicalpath. Newqualitybudgetenergyguard same64float-epsilon; resulthas no externalboundarywork inthisscopebecausemovingboundariesfallback. All400 completes mean36.045 median21.900p95113.333 worst203.394ms (~halfcurrent6059 mean67.35; notyetqualifiedandnoFPSclaim). EndpointqualitynotyetprintedlookupRESULTS. Compile1benign-Wparentheses, fixedv6. Currentlyonlyfallbacksleeping/deferredbody =>canmakecost worsewhenhittinginactiverestneighbor; correctv6 below.
- world-v6 copiesv5, fixesoperatorparentheses, deferredgravity/sleep transactional handling: Initialize any actuallytouching eligiblefreebody copiesPoseAt(now), ifdeferred consumesremainingdtgravityINTRIALincomingBEFOREfirstremember/impulse, marksgravityConsumedandforcedExpansionqueue. ExpandseedsandforcedqueuebeforeSolveGrowing; iflatergrowthinitializesnewforcedbody, growcallbackdrainsexpandqueuebeforeappend, guaranteeingallnewforcekickedbodies' actualcontactscovered. Worldawake/sleep unchangeduntilsuccessfulcommit; fallbacktrialdiscarded. FinalexistingcoordinatorWakeexpanded andclearstheflagonlyafteraccept. Noaccelerationdoublecount: removedoldexpansiontimekick. Excludeinfinite-massstaticanchorfromjointbodyfallback (staticwallcannotmechanicallyjoinotherdynamics, asoriginalBFS); dynamichardjointstillfallback. AddedpostGrowingvelocitydefectguard: noacceptednewmodecontactclosing*fulldt>existing5mm; failedtrialfallbackoldphysics, notweakeningtest. PrivateTraceenvJUDAS_PRIVATE_DOMAIN_TRACE printsONEsummaryperAdvance step whenenabled, accepted/failed/reasons/domainvisits/rows/peak, offbydefault. Needrunfirst400withtrace tolocate remainingcost. LazyStepContactReferencefreezingnowonlyatfirstactualeventBEFOREWorldposescurrtimeupdate; zero-eventworldpaysnorefmap. jointBodiesmaskstillallocatesNperstep, canmovelazynextfinalsource.
- world-v6 originallyfull5TUbuildsucceededzerowarnings; thenstaticjointguardwasnarrowedafterbuild. SavedBUILD-before-static-guard.json andWorldIncrementalImpact-before-static.cpp. Currently SINGLEmodule rebuildrunning session51100 via build_module.py, arretainsallotherfourmatchingobjects. Uponfinishcreateprivate snapshot/updatehashasotherversions, run400 `JUDAS_PRIVATE_DOMAIN_TRACE=1` (serialnativeGL requireescalated), preservecounterlogs. No production editsyet.

Important v5/current code risks to investigate beforeadoption:
- Epochcapinitialnegative +immutablehistorymaystillcauseglobalcorrection frompost-NEWimpact10-sweepresidual. Potentialreaction-drivenepochupdateonlywhenactualacceptedphysicalresponsechangesbodymomentum; NOTunconditionallyoneveryarrival/zeroimpulserow. Keepabsoluteexisting5mm/dtbudget, speculativegapsnevereligible. No suchupdateimplementedyet—currentv6usesv2immutableperfeaturecap. Needevidence/countersbeforechangingagain.
- Full-worldnewtrajectoriesdifferintentionally; isolatedrowkernelparityisnotentirephysicsqualification. Needalltimegeometry/energyqualityandoriginalsuites/cold savesunchangedthresholds. Prototypeendpointdifferentchaoticstatealonecan'tcertifyequalityorregression. Originalcurrent400endpoint6.598mm; oldalltimebaselineknown63.58mmfloor/57.8mmpair earlier270, comparematchedactualfullstate.
- SourceinputexactheaderprivatefieldImpactContact.size changed; recompileImpactSolverTests directcaller; othersexistingpublicPhysicsWorld testobjsdon'tuseImpactContact,canlinkprivatecoreforfastsmoke,butfinalnormalcmakefullcorrectrebuildrequired.
- Finalgrowingheader/methodnativeinternalnewAPI only,noJudasJSbindingsneeded; noJSdocsphantomexports.
- New World legacycoordinatorfunction grewfrom600to~700lines; refactorcohesivelylater ifaccept—notshipprivatedensecode. PublicPhysicsWorld.cppstill~500lines; no4kgiantviolation.
- Usercommentsalreadydelivered: firstfullprototype modestgain notqualified; immutablefeatureepochs needtightening; activekernel300cases+400worldbitparityproved; restartingdomainmadegenuineimpactsworse, nowretainingimpulses/growingrowswithoutWorldpartialcommit. Continue meaningfulupdates~60s.
- Allnewsource remainsignored `.cache/contact-redesign-20261021`; trackedworktreeclean; previousinvestigation93fileevidence+reportuntrackedpreserved. No newcommits/pushes. Mainuntouched.

## LATEST — world-v8 promising, still private/unqualified

Read this update INSTEAD of treating oldimmutable-velocityreference plan asfinal.
The growing solver now uses existing authoritative native `supportPairs` for
continuing-contactownership; no parallel point-historyregistry isneeded.

- world-v7 changed normal-target law to event-local accepted incoming reference:
  newlyselected/new/unknowncontacttarget0; continuing knownsupported contacttarget
  min(incoming_normal_velocity,0) ONLYifclosing*fulldt<=existing5mm. Snapshotis
  capturedONCEinm_savedBEFOREanyeventimpulse; appendedrowsreuseoldbodyentry, never
  rebaselinepartiallysolvedvelocity. Scalarhistoricalmomentumreferencecausedthe
  originalnumericalerrorreprojection. Newlawis δ-impulseconstraint, notdifference
  ofprojections. Maxallowedolddefectphysicalqualitybound; largeolddefectstrict0.
  v7stillusedparallelStepContactReference.Knownfeaturelookup, falselyclassified
  manyalready-ownedcachedsupportsasnew(gaps/pointtopology). Mean30.132/p9583.088/
  worst135.339ms; domainsstillnear900late. Allprivatepreserved.
- world-v8 FIXEDownershipintegration: .continuing=key!=selected &&binary_search
  EXISTINGsupportPairs. Spinningpairsnoteligible,newarrivalsstrict; original
  supportclassification/invalidation/coldcacheownershipunchanged. No separate
  `StepContactReference` atall; removedconstructor/Register loops. Restored
  PhysicsWorld.cpp EXACToriginalsources (StartContact.partfieldsnotneeded).
  WorldInternal.cppcopiesrestoreoriginalStartContactABI, onlynestedtrialforward
  +Trydeclarationadded. HeaderforwardStepContactReference removed. NormalSolver
  helperbudget now `ImpactContact.continuing` defaultfalse +maxDefectDisplacement
  defaultContactSolver::kPenetrationSlop; Worldsetsmax=min(existing5mm,
  SpinPenetrationTarget(key)) (quarter-smallestprimitivefeature existingbound).
  No normalVelocityDefect field retained. Legacy ImpactSolver.cpp::Solve and
  signature/restpolicy RESTOREDexactoriginal, newSolveGrowing appendedseparate.
  Defaultlegacyphysicsnotweakened; movingkinematic/joint/mesh/asset/coupled paths
  retainlegacyfullisland. ContactSolvernewrangeandtrackedfunctionsstillusedby
  Growing only; optionalactivekernelnotcalledbylegacySolve.

  DynamicgrowingPGSalgorithm:
  1. privateWorldtransactioncopiesonlyactualeventposesandvelsintostableslotarray;
  provisionaldeferredgravitybeforefirstremember,forcedcontact-expansionqueue;
  noauthoritativewake/publicationuntilallnewphysicalcontactsclosedandbudgetpass.
  2. expandbothseedactualcontactsfirst; growing10GSsweeps preservesallaccumulated
  eventimpulses; afterrangeprocessedtrackEVERYbodytouchedbynonzeroimpulsestate
  (includingtransientnetzero),expandthoseactualcontacts,prepareonlynewrow+frames,
  appendnewrowsatendsameGSsweepwithoutrestartoroldwarm-replay.
  3. rememberincomingbodyvelocitybeforefirstimpulse; ordermatchesContactSolver
  FrameIDs (assertbijection). Forcontinuingsupportrowincomingclosingdefectwithin
  unchangedphysicalallowance,useincomingvn asnormalbias; else0. Freezeeachrow
  targetforentireevent. Newstrictcontactsfullytransmitforce. Outputenergyguard
  same64float-epsilonandpostclosingvelocity*dt<=rowphysicalallowance; rejection
  usesoldfullsolveviaWorldprivate-fallback,NOTmutatingfailedtrial.
  4. commonWorldcontact/history/support/motion/eventinvalidations retained. Local
  solverpointersdiebeforeownedtrialbodyvectors. Per-eventactualsolverpointer
  directscommoncachepublication. Deferredflagsconsumedonlyaccepted; allforced
  gravityparticipants' contactsdiscoveredbeforerowsappended. Staticjointanchors
  DONOTjoinfinite-massjointislands; dynamichardjointorunsupportedtouchfallback.

### Measured v8 first400 (nativeRelease, sameoriginalfixture)
Original6059fresh400: mean67.3476/median31.1778/p95201.9638/worst533.8422ms.
v8trace400: mean19.7464/median18.8301/p9532.7259/worst56.3091ms.
Endpointnativepenetration15.5798mm vsoriginal6.5978mm atdifferentchaoticendstate.
NOTaccuracyqualification—needevery-stepgeometry/energymatchedruns! Original
older270alltimefloor63.58mm/pairs57.8mm; donotcallnewendpointdisprooforproofalone.
v8trace: accepted2790, failed264 (232isolatedelastic,32unsupported; noenergyor
residualrejects), 85653domainbodyvisits,188813rows. Lateendstepsdomains400–725
bodyvisitsacross3–6events, peaks224–294 ratherthan~900EACHevent. Actualquerycounts
availableCSV; mustsummarizeagainstoriginal. Currenttracesourceopt-inenv
JUDAS_PRIVATE_DOMAIN_TRACE=1 recordsoneDOMAINlineperAdvance; offbydefault.

### Focused PRIVATE native qualification done
`.cache/contact-redesign-20261021/world-v8/qualify.py` ranSERIAL, all16invocations
pass (checks/RESULTS.json), includesnewGrowingImpactTests37checks, roworacle300
exactfull-vs-activecases, legacyImpactSolver,timing,robustness,continuationbounds,
collisioncontinuity,joints,kinematics,rigidcontacts,CharacterMotor,ragdolls,
physicscontinuation,contactlifecycle,native-mesh-growth,collisiongeometry.
M91continuityactualcurrentcount406/0 (notolder372). Newchainlength2/3/32/100:
velocities+normalimpulsesBITEXACTprebuiltfull10GSsolve;100thbodynonzero5.38e-23,
noarbitraryisland/impulsecutoff. Newtestszero-effectarrivalpreservesunrelatedsmall
accepteddefect, sameNEWcontactmustrespond,1m/scounterexamplecorrectedlargeold
closure strict, smallerfeatureclearanceforcescorrection, immutableincoming
snapshotforlateappendedrow, arbitraryorientation,transientnetzeroactivation.
Testsuseactualnewcoreprivatearchive, oldImpactSolverTests directImpactContact
caller RECOMPILEDwithproper -Dmain=JudasValidationMain andNOARGS beforelink;
othernativepublicPhysicsWorld testobjsABIunchanged. Newtest sourcesareinworld-v8.
NoASan/cold-process/editor/export/live/600long/fullfinalnormalCMakebuildyet.
No production sources changed orcommits/pushes.

### Next immediately
1. All-timeEND-OF-EACH-STEP physicalquality observer for current6059 vsnewv8:
reuse `.cache/dense-scalability-20261019/quality-prototype/TemporalObserver.h`,
`FloorOracle.h`, `OracleControls.cpp` andexistingobserverdriver/buildlogic.
COPYonlyintoNEWcache; originalsprotected. Changeowncopysamplingstep%10 totrue
=>nativegeometryeachof600steps+floor/energyall601states. Theseobservationsare
OUTSIDEfixedtiming,notFPSbenchmark. CompileONEdriverobjectwithactualcurrent
`tests/PhysicsStressLiveBenchmark.cpp` read-onlyobserverinsertion asoldbuild.py,
linkSAMEobjectagainstexactfrozencurrent6059archiveandv8archive, serial600runs.
Currentdriver/publicABIunchanged. Needverifyeachrunall600digestsagainstuntimed
baseline/candidate normalruns (candidate600notrunyet) soobservernophysicaleffects.
Comparemass/finite/insidefloor,mechanicalenergy,depthworst+quantiles+duration;
originalclearancelimitsareevidence,don'tinvent/weakenacceptancethresholds.

2. Potential safeperformance/readability improvement innewSolveGrowingbody
remember: currentlylinear std::none_of(m_saved) perrow (O(n²)). Reuse authoritative
ContactSolverframeIDs: AddContactFIRST, getnewConstraint.frameA/B, appendm_saved
onlywhenframeIndex==size; existing< size assertbodyptrmatches. Framesassignfirst
encounterA,B inexactSAMEorderasoldremember;budgetarithmeticorderunchanged.
Nojointsnewmode. Thisremovesduplicatepointerindexing,boundedperfwork,shouldretain
ALL400digestsagainstv8. LegacySolveunchanged! Notimplementedyet.

3. Ifqualitygood, integratecleanactualsourcefromv8 (orv9linearmapping) into
src/physicscohesivemodules,formatnewC++withclang-formatifavailable,extractlegacy
islandbuilderifcoordinatorbloated. AddnewCMakesource+registeredauthoritative
newtests+negativecontrols+docs. Allcurrentprototypessources/evidencepreserved.
Then normalfinalReleasebuild(jobs2max8GB),focusedfinalphysicsplusM70/M77ACTUAL
cold-processendpoints/save-stream/editorPlayStop/relocatedstressandHearthworld
smoke, source/binaryfingerprints,600longrepeatdeterminism/nativevisible35s.
WindowsAndroidnotavailable8GBconstraint; don'tlaunchVM orunrelatedprocesses.

4. Strengthennewtests: staticsharedfloor1000independentassembliesneverjoin,
truecoldwake/deferredgravityforcedexpansionfallbacktransactionunmutatedfailure,
simultaneouseventsurvive,smallfeature/adversarialfinitefeaturedeparture/recontact,
angularmomentum/frictionbudget,saveboundarynumericalstateephemeral,noAPIphantoms.
Oldtestsnotmodified! ASan/UBSanchangedmodulesnativeonlyifavailable,memoryownership
newcallbacksneedqualification. No fullproductionmarathonunlessevidencewarrants.

5. FinalmeasuredLinuxcandidatecheckpointExperimentalperAGENTS; mainrequires
humanacceptance. ReporthonestactualFPS (fixedstep20msaloneNOT30FPSproof),tail
limits,fallbackscope,newmodelnumericaltrajectorydifferences.Allprotectedhistory,
independentfluidresearch,unrelateduntrackedM78/M83filespreserved.

Recentcommentary: v8preliminary400mean67->20,p95202->33,worst534->56;
usesauthoritativesupportnotparallelhistory; nowcorrectnessregressions+long/live
next. Mustkeepupdates~60s ongoing. No finalresponseuntilproperqualificationor
actualunresolvablemechanism established(userexplicitnewredesignauthorization).

## Latest verified quality + work queued
- Native16qualificationallpassed, Growing37/0; roworacle300/0. Collisioncontinuity
actualcurrent406/0. Alllogs/commandschecksunderworld-v8.
- Built ONE every-step observerdriverobject againstunchanged publicsource and
linkedSAMEobjectagainstbaseline6059coreandv8core. Filesunder`temporal/`.
Observereveryfixedstep+initial601states, notoldone-per10sampling; outsidephysics.
Both600runsfinishedexit0. `QUALITY_COMPARISON.json`:
 baseline600mean67.681/p95177.987/worst533.657ms; new600mean31.749/p9573.133/
 worst111.909ms. Newlate201stepscostmorethanfirst400, beHONESTdon'treportonlyshort.
 baselinepair/floormax72.2727mm, p95depth19.5228mm; candidate55.1023mm max,
 p95depth24.3124mm (worsefrequency, notblanketaccuracyclaim), finaldepthbaseline
 15.3171mm vsnew21.9904mm. All601statesbothmass2048kg/pop1024, finite, insidefloor
 all1024,0gravitymismatch,0uncertain/unsupportednativegeometry. Mechanicalenergy
 initial/max32346.3175Jboth; final23899.8145baseline/22988.3955candidate, noexcess
 vsinitial. Scopedloose_pieceenergy,notwholeworlduniversalenergycertification.
- ObserverparityPROVED:`temporal/OBSERVER_PARITY.json`:baselineall600digestsmatch
 retainedunobserved6059candidate600-A; candidatefirst400digestsmatchv8run400trace
 exactly. Needcandidateunobserved600+repeatlaterforfullparity/determinism.
- world-v9 compiledSINGLEImpactSolver.cpp replacementintov8core: replacedquadratic
 rememberstd::none_of byexistingContactSolverframeIDs. AddContactfirst, body
 snapshotonlyifnewFrameIndex==m_saved.size elseassertptrsame. Firstencounter
 orderidentical, budgetsumorderunchanged, no seconddict. LegacySolveUNCHANGED.
 Allsourcescopiedv8; `build_incremental.py` actualreceipt, binarysnapshotmade.
 Currentv9unprofiled400 running session80796. WhenfinishedcompareALLphysical/work
 fields vsv8 (expectbitexact), recordparity. Noactualproductionchangesyet.

### Proposed next v10 — warm-cache ownership (not implementedyet)
Corev8stillclearsrestingFORCEguessesforallDISCOVEREDcontactgroups,evenunchanged
bodyvelocities/trajectories. ImpactimpulsesMUSTNOTwarmreplay; untouchedordinary
forceguessesremainvalidasifnoimpacthappened. Fix numericalwarmcacheONLY, don't
changecompletedContactObservationspublicsemantics (stilloriginalreplacepolicy).
ComputeexactvelocityChangedflagfromislandoldVelocities vsWorldpostsolve BEFORE
cachehandling (don'tpublishmotionsegmentsearly—SpinsThroughSupportusesoldledger).
Fornewincrementalmodeinvalidatewarmonlyifendpointvelocityactuallychanged;
legacyfallbackretainsoldfullincluded-islandpolicybitexact. Actualnewgroupsstill
needzeroloadidentity; unchangedexistinggroupskeepoldwarmrows.
Implementationwithoutnewcross-eventbookkeeping:
 useexistingunique sorted`replaced`observationKeysvector; parallelkeepExisting
 flagsinitialfalse. DuringcontactCacheerase scanlower_boundgroupidx; ifmatches
 andincremental andNEITHERendpointvelocityChanged, markkeepExisting[idx]=true,
 keeprow. Elseeraseobservedgroupasbefore. Outsideobservedgroups existingloop
 zeroswarmonlyforchangedendpoints(newmode),wholeincludedlegacy. Forobservedrows
 appendzeroidentityONLYwhenkeepExisting[groupidx]==false (changedornewabsent).
 NoextraMappersisted, sameconstantgroupwork; no staleinvalidationmetadata or
 repeatedoldimpulseapplication. Nextstepnativewarmmatcherhandlesfreenormal/pose
 changesnormally. Thisismomentumdependencyfix, mayimproveaccuracyANDconvergence.
 Needmatchedv10performance+every-stepqualityandunmodifiedtests, notassumebenefit.

### Possible next integration — prescribed boundaries (not implementedyet)
CurrentnewmodefallbacksALLkinematic/staticmovingboundaries, so2p-pusherconsumers
maystillhitgloballegacy. Genericextensionisstraightforwardenergyaccounting,
notshooterfeature: allowinfinitemassprescribedBox/Sphere surfaces, neverexpand
throughstaticbody. WorldalreadyupdatesALLkinematicposes/vels atnowBEFOREtrial.
GrowingmeasureexternalworkEXACToldImpactSolver::run loop:
 impulse=n*normalImpulse+tangentImpulse;
 staticAwork -=dot(impulse,PointVelocity(A,offsetA));
 staticBwork +=dot(impulse,PointVelocity(B,offsetB));
 energyTolerance=64floatEps*max(1,Kbefore,abs(work));
 Kafter<=Kbefore+work+tolerance. Staticm_savedkinetic=0unchanged.
 Dynamicjoint/unsupportedmesh/externalcouplingstillfallback. Unchanged prescribed
 trajectoriesnotinvalidatedbyreaction; newdynamictraj reschedulednormally.
 Nativekinematicregressions+newmoving/rotatingboundaryworkpositive/negative tests
 +actual2p game smoke ifadopt. Don'tclaimuntilvalidated. Avoidmixingwithv10inone
 unmeasuredexperiment; keepcoherentprivateversionsandfinalsourcequalification.

### Other final cleanup requirements
Zero-eventWorld currentlyconstructslocalTrialsolverwithPMRpoolbeforeknowingevent;
makeoptional/lazy (orWorld-ownedoptionalreusablescratchwithproperlifetime) so
inactiveunrelatedgamespaynone. **World-ownedmemberchangesImplABI** =>normalfull
CMakebuild/allWorldTUsrecompile required; privatefrozenpartialoverlayunsafeunless
memberappended+everyImplallocatorchecked. Preferoptionalstacklazyforfirstsource
integration, thencohesiveWorldownedoptionalworkspacewithfullbuildifneeded.
jointBodiesmaskalsoallocateonlyonfirstactualevent, notNperquietStep.
Newformat/source organizationcanextractcohesiveContactVelocitySolver andshared
contactmath/impactbudgethelpers, butdon'twholesalerewrite. Legacyislandblockstill
~200linescoordinator total~700—samecohesivepurpose, publicPhysicsWorld~500unchanged.
FinalCMakeactualsourcebuild+focusedregressions/apps/cold/ASan/fingerprintsrequired.
AllolduntrackedhistoryM78/M83unchanged, no gitstage/commits/push/newmainchanges.
