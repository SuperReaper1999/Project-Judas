// Willow Town owns needs, jobs and movement policy. Judas owns utility decisions,
// physical collision, sensing, resource leases and persistence.
import {UtilityAgent, AISenses, AITargetReservations, entity, physics} from 'judas';
import definition from './citizen.ai.js';
import layout from './layout.js';
import {createState, clamp, workingHours, advanceVitals, ABSENCE_LIMIT} from './life.js';
export {createState} from './life.js';

const ZERO = {x: 0, y: 0, z: 0};
const length = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const lookup = id => entity(String(id));

// This small static town uses a deterministic 0.5m four-neighbour route grid.
// Nodes inside authored solid footprints are excluded. Movement still casts the
// real actor shape every step; the grid is planning, never a second collider.
const GRID = .5;
const MIN_X = -15.5, MAX_X = 15.5, MIN_Y = -10.5, MAX_Y = 10.5;
const WIDTH = Math.round((MAX_X - MIN_X) / GRID) + 1;
const HEIGHT = Math.round((MAX_Y - MIN_Y) / GRID) + 1;
const obstacles = (layout.obstacles || layout.blocked || []).map(box => {
  if (Array.isArray(box)) return {x: box[0], y: box[1], hx: box[2], hy: box[3]};
  return {x: box.x, y: box.y, hx: box.hx ?? box.w / 2, hy: box.hy ?? box.h / 2};
});
const gridPoint = key => ({x: MIN_X + (key % WIDTH) * GRID,
  y: MIN_Y + Math.floor(key / WIDTH) * GRID, z: 0});
const blocked = point => obstacles.some(box =>
  Math.abs(point.x - box.x) < box.hx + .29 &&
  Math.abs(point.y - box.y) < box.hy + .29);
const freeGrid = Array.from({length: WIDTH * HEIGHT}, (_, key) => !blocked(gridPoint(key)));

function nearestNode(point) {
  const column = Math.max(0, Math.min(WIDTH - 1, Math.round((point.x - MIN_X) / GRID)));
  const row = Math.max(0, Math.min(HEIGHT - 1, Math.round((point.y - MIN_Y) / GRID)));
  const initial = row * WIDTH + column;
  if (freeGrid[initial]) return initial;
  // A character near a corner can be outside the inflated planning grid while
  // remaining physically valid. Search a bounded two-metre neighbourhood.
  let best = -1, bestDistance = Infinity;
  for (let y = Math.max(0, row - 4); y <= Math.min(HEIGHT - 1, row + 4); ++y) {
    for (let x = Math.max(0, column - 4); x <= Math.min(WIDTH - 1, column + 4); ++x) {
      const key = y * WIDTH + x;
      const distance = length(point, gridPoint(key));
      if (freeGrid[key] && distance < bestDistance) { best = key; bestDistance = distance; }
    }
  }
  return best;
}

function route(start, destination) {
  const first = nearestNode(start), last = nearestNode(destination);
  if (first < 0 || last < 0) return [];
  const parents = new Int32Array(WIDTH * HEIGHT); parents.fill(-1);
  const queue = [first]; parents[first] = first;
  for (let head = 0; head < queue.length && parents[last] < 0; ++head) {
    const key = queue[head], x = key % WIDTH, y = Math.floor(key / WIDTH);
    // Fixed neighbour order makes equal-cost routes reproducible.
    const adjacent = [x > 0 ? key - 1 : -1, x + 1 < WIDTH ? key + 1 : -1,
      y > 0 ? key - WIDTH : -1, y + 1 < HEIGHT ? key + WIDTH : -1];
    for (const next of adjacent) if (next >= 0 && freeGrid[next] && parents[next] < 0) {
      parents[next] = key; queue.push(next);
    }
  }
  if (parents[last] < 0) return [];
  const reverse = [];
  for (let key = last; key !== first; key = parents[key]) reverse.push(gridPoint(key));
  // The actor can be off-grid. First reach the start node before taking any
  // grid edge: skipping it cuts a diagonal across a building's inflated corner.
  reverse.push(gridPoint(first));
  reverse.reverse(); reverse.push({...destination, z: 0});
  return reverse;
}

export class Town {
  constructor(state) {
    this.state = state;
    this.agents = new Map();
    this.helpers = new Map();
    this.invitations = new Map();
    this.observations = new Map();
    this.routes = new Map();
    this.current = null;
    this.pedestrians = [];
  }

  resident(id) { return this.state.residents.find(person => person.id === String(id)); }

  start(restoring = false) {
    if (this.state.version !== 2 || !Array.isArray(this.state.jobs))
      throw Error('Willow Town life version 2 requires a new game or a version 2 save; the original town save is not migrated.');
    this.validateEmployment();
    for (const resident of this.state.residents) {
      const actor = lookup(resident.id);
      if (!actor?.valid) throw Error('Willow Town is missing resident ' + resident.name);
      this.pedestrians.push(actor);
      if (!resident.alive) { this.stopBody(resident.id); actor.setColliderEnabled(false); continue; }
      const helpers = {
        claims: new AITargetReservations(),
        senses: new AISenses({interval: .5, maxResults: 64, maxCandidates: 128, maxRaycasts: 0})
      };
      this.helpers.set(resident.id, helpers);
      const handlers = {};
      for (const action of ['work', 'eat', 'rest', 'socialize', 'wander', 'jobsearch']) {
        handlers[action] = {
          start: (_context, choice) => this.begin(resident, choice),
          update: (dt, _context, choice) => this.perform(resident, choice, dt),
          stop: (_context, choice) => this.stop(resident, choice)
        };
      }
      const agent = new UtilityAgent({...definition, initialDelay: (Number(resident.id) - 301) * .04},
        handlers, resident.name + ' — town resident');
      this.agents.set(resident.id, agent);
      if (restoring && resident.snapshot) agent.restore(resident.snapshot);
    }
    if (this.state.controlled && !this.resident(this.state.controlled)?.alive) this.state.controlled = null;
  }

  inspect(id) {
    const agent = this.agents.get(String(id));
    return agent ? agent.inspection : null;
  }

  log(message) {
    this.state.message = message;
    this.state.events.unshift({time: this.state.time, message});
    this.state.events.length = Math.min(12, this.state.events.length);
  }

  job(id) { return this.state.jobs.find(row => row.id === id); }

  jobFor(person) {
    const job = person.jobId ? this.job(person.jobId) : null;
    return job && job.employeeId === person.id ? job : null;
  }

  validateEmployment() {
    const occupied = new Set();
    for (const job of this.state.jobs) {
      if (!job.id || !job.target || !Number.isFinite(job.wage) || job.wage < 0)
        throw Error('Willow Town save has an invalid job record');
      if (!job.employeeId) continue;
      const person = this.resident(job.employeeId);
      if (!person?.alive || person.jobId !== job.id || occupied.has(person.id))
        throw Error('Willow Town save has inconsistent job ownership');
      occupied.add(person.id);
    }
    for (const person of this.state.residents) {
      if (person.jobId && !this.jobFor(person)) throw Error('Willow Town save has an unowned employee');
      if (!person.alive && person.jobId) throw Error('A deceased resident cannot retain a job');
    }
  }

  remember(person, type, jobId, reason) {
    person.history.unshift({time: this.state.time, type, jobId, reason});
    person.history.length = Math.min(8, person.history.length);
  }

  // Manager actions and ordinary attendance use the same employment boundary.
  // Cancelling is never a completed task: no wages or output are granted here.
  dismiss(id, reason = 'Dismissed by the town manager') {
    const person = this.resident(id);
    if (!person?.alive || !person.jobId) return false;
    const title = person.job;
    this.releaseJob(person, reason, true);
    this.log(person.name + ' lost their ' + title.toLowerCase() + ' job: ' + reason + '.');
    return true;
  }

  releaseJob(person, reason, dismissal) {
    const job = this.job(person.jobId);
    if (!job) return;
    const agent = this.agents.get(person.id);
    agent?.finish('cancelled', this);
    if (agent) person.snapshot = agent.snapshot();
    this.helpers.get(person.id)?.claims.releaseAll();
    if (job.employeeId === person.id) job.employeeId = null;
    this.remember(person, dismissal ? 'dismissed' : 'vacated', job.id, reason);
    if (dismissal) { person.jobsLost++; this.state.census.firings++; }
    person.lastJobId = job.id;
    person.jobId = null; person.job = 'Unemployed'; person.workTarget = null;
    person.jobLostReason = reason; person.lastEmploymentChange = this.state.time;
    person.absenceWorkingSeconds = 0; person.service = {key: '', elapsed: 0};
  }

  setJobOpen(id, open) {
    const job = this.job(id);
    if (!job || typeof open !== 'boolean') return false;
    job.open = open;
    if (!open && job.employeeId) this.dismiss(job.employeeId, 'Workplace closed');
    this.log(job.title + ' workplace ' + (open ? 'opened' : 'closed') + '.');
    return true;
  }

  acceptJob(person, job) {
    if (!person.alive || person.jobId || !this.state.hiringOpen || !job?.open || job.employeeId) return false;
    // Script callbacks run sequentially at the fixed boundary. The advertised
    // job also has an exclusive native lease; rechecking here prevents a stale
    // candidate from claiming a vacancy another applicant has already filled.
    job.employeeId = person.id;
    person.jobId = job.id; person.lastJobId = job.id; person.job = job.title; person.workTarget = job.target;
    person.jobsFound++; this.state.census.hires++;
    person.absenceWorkingSeconds = 0; person.lastWorkedAt = this.state.time;
    person.lastEmploymentChange = this.state.time; person.jobLostReason = '';
    this.remember(person, 'hired', job.id, 'Application accepted at the noticeboard');
    this.log(person.name + ' found a new job as ' + job.title.toLowerCase() + '.');
    return true;
  }

  hurt(id, amount) {
    const person = this.resident(id);
    if (!person?.alive || !Number.isFinite(amount) || amount <= 0 || amount > 1) return false;
    person.health = clamp(person.health - amount);
    this.log(person.name + ' was injured. Health is now ' + Math.round(person.health * 100) + '%.');
    if (person.health <= 0) this.die(person, 'Injuries');
    return true;
  }

  die(person, cause) {
    if (!person.alive) return false;
    const previousJob = this.job(person.jobId);
    const agent = this.agents.get(person.id);
    person.lastDecision = agent?.inspection.selected ?? null;
    if (person.jobId) this.releaseJob(person, 'Died: ' + cause, false);
    person.alive = false; person.health = 0; person.deathCause = cause;
    person.deathTime = this.state.time; person.snapshot = null;
    person.service = {key: '', elapsed: 0};
    person.explanation = 'Died from ' + cause.toLowerCase() + '.';
    this.remember(person, 'died', null, cause);
    if (this.state.controlled === person.id) this.state.controlled = null;
    this.stopBody(person.id);
    // Keep the authored entity and its exact physical endpoint for selection
    // and saves, but retire all decision work and stop it being a sensor target.
    agent?.dispose();
    const helper = this.helpers.get(person.id);
    helper?.claims.dispose(); helper?.senses.dispose();
    this.agents.delete(person.id); this.helpers.delete(person.id);
    this.routes.delete(person.id); this.observations.delete(person.id);
    for (const [recipient, sender] of this.invitations)
      if (recipient === person.id || sender === person.id) this.invitations.delete(recipient);
    const actor = lookup(person.id);
    if (actor?.valid) actor.setColliderEnabled(false);
    this.state.census.deaths++;
    this.log(person.name + ' died from ' + cause.toLowerCase() + '.' +
      (previousJob ? ' The ' + previousJob.title.toLowerCase() + ' position is now vacant.' : ''));
    return true;
  }

  setNeed(id, key, value) {
    const person = this.resident(id);
    if (!person?.alive || !['hunger', 'fatigue', 'social', 'health'].includes(key) || !Number.isFinite(value)) return false;
    person[key] = clamp(value);
    if (key === 'health' && person.health <= 0) { this.die(person, 'Injuries'); return true; }
    this.log(person.name + ': ' + key + ' changed. Their AI will reconsider at its next evaluation.');
    return true;
  }

  setControlled(id) {
    const next = id === null ? null : String(id);
    if (next && !this.resident(next)?.alive) return false;
    const previous = this.state.controlled;
    if (previous === next) return true;
    if (previous) this.stopBody(previous);
    if (next) {
      this.agents.get(next).finish('cancelled');
      this.helpers.get(next).claims.releaseAll();
      this.resident(next).snapshot = this.agents.get(next).snapshot();
    }
    this.state.controlled = next;
    this.log(next ? 'You control ' + this.resident(next).name + '. WASD / arrows to walk; return to AI whenever you like.' :
      'Resident returned to their own decisions.');
    return true;
  }

  stopBody(id) {
    const actor = lookup(id);
    if (actor?.valid) actor.setKinematicVelocity(ZERO, ZERO);
  }

  // Frame dt remains the engine's fixed step; game speed changes policy rates and
  // desired velocity, not the world's solver timestep or collision fidelity.
  step(dt) {
    for (const resident of this.state.residents) this.stopBody(resident.id);
    if (this.state.paused || !Number.isFinite(dt) || dt <= 0) return;
    const speed = this.state.speed === 3 ? 3 : 1;
    const elapsed = dt * speed;
    this.state.time += elapsed;
    for (const resident of this.state.residents) {
      if (!resident.alive) continue;
      const cause = advanceVitals(resident, elapsed);
      if (cause) { this.die(resident, cause); continue; }
      const job = this.jobFor(resident);
      if (job && workingHours(this.state.time)) {
        resident.absenceWorkingSeconds += elapsed;
        if (resident.absenceWorkingSeconds >= ABSENCE_LIMIT)
          this.dismiss(resident.id, 'Missed ' + ABSENCE_LIMIT + ' working seconds without completing a task');
      }
      if (resident.id === this.state.controlled) continue;
      this.current = {resident, physicalDt: dt, gameDt: elapsed};
      const agent = this.agents.get(resident.id);
      // Native utility time follows real fixed-step time. Accelerated needs and
      // service progress are game policy; no larger AI/physics timestep is used.
      agent.tick(dt, () => this.candidates(resident), this);
      resident.snapshot = agent.snapshot();
    }
    this.current = null;
  }

  manualMove(direction, dt) {
    const id = this.state.controlled;
    if (!id || !this.resident(id)?.alive || this.state.paused || !Number.isFinite(dt) || dt <= 0) return;
    const size = Math.hypot(direction.x, direction.y);
    if (!Number.isFinite(size) || size < .01) { this.stopBody(id); return; }
    const actor = lookup(id);
    if (actor?.valid) this.move(actor, {x: direction.x / size, y: direction.y / size},
      3 * (this.state.speed === 3 ? 3 : 1), dt);
  }

  candidates(person) {
    const actor = lookup(person.id);
    if (!actor?.valid || !person.alive) return [];
    const position = actor.transform.position;
    const helpers = this.helpers.get(person.id);
    const make = (action, id, facts, eligible = true, reason = '') => {
      const target = lookup(id);
      const available = helpers.claims.available(this.claimKey(action, id));
      return {action, target: String(id), facts: {...facts, distance: target?.valid ? length(position, target.transform.position) : 65},
        eligible: eligible && !!target?.valid && available,
        reason: !target?.valid ? 'The destination no longer exists' : !eligible ? reason : !available ? 'Another resident is using this place' : ''};
    };
    const job = this.jobFor(person);
    const onShift = workingHours(this.state.time);
    const work = make('work', job?.target ?? '1105',
      {duty: onShift ? 1 : .12, readiness: Math.max(.05, 1 - person.hunger * .45 - person.fatigue * .55)},
      !!job && job.open && person.health > .1,
      !job ? 'No job: visit the noticeboard to apply' : !job.open ? 'The workplace is closed' : 'Too unwell to work');
    const foodReady = this.state.shop.open && this.state.shop.stock > 0 && person.money >= 2;
    const foodReason = !this.state.shop.open ? 'The shop is closed' : this.state.shop.stock <= 0 ? 'Food is sold out' : 'A meal costs 2 coins';
    const output = [work,
      make('eat', '1301', {hunger: person.hunger}, foodReady, foodReason),
      make('eat', '1302', {hunger: person.hunger}, foodReady, foodReason),
      make('rest', person.bedTarget, {fatigue: person.fatigue})];
    for (const vacancy of this.state.jobs) {
      const eligible = !person.jobId && this.state.hiringOpen && vacancy.open && !vacancy.employeeId;
      const reason = person.jobId ? 'Already employed' : !this.state.hiringOpen ? 'Town hiring is paused' :
        !vacancy.open ? 'The workplace is closed' : vacancy.employeeId ? 'This vacancy has been filled' : '';
      // Job target identifies the advertised vacancy; every application is made
      // physically at the public noticeboard, not at an imaginary remote office.
      output.push({action: 'jobsearch', target: vacancy.target,
        facts: {needForWork: .7 + .3 * clamp((12 - person.money) / 12),
          distance: length(position, lookup('1105').transform.position)}, eligible,
        reason: eligible ? '' : reason});
    }
    const observation = helpers.senses.sample(position, 12, {visibility: false, planar: true});
    this.observations.set(person.id, observation);
    const neighbours = observation.targets.filter(candidate => this.resident(candidate.entityId)?.alive && candidate.entityId !== person.id);
    for (const neighbour of neighbours) {
      const other = this.resident(neighbour.entityId);
      const invitation = this.invitations.get(person.id) === other.id;
      const pair = [person.id, other.id].sort().join(':');
      const recent = this.state.socialPairs[pair];
      output.push(make('socialize', other.id,
        {company: invitation ? Math.max(.86, person.social) : person.social},
        (!recent || this.state.time - recent > 5) && other.alive && other.health > .1,
        recent && this.state.time - recent <= 5 ? 'Just finished a conversation' : 'This resident is unwell'));
    }
    for (let id = 1501; id <= 1504; ++id) {
      const destination = lookup(id);
      output.push(make('wander', id, {}, !!destination?.valid && length(position, destination.transform.position) > .5,
        'Already at this corner of town'));
    }
    return output;
  }

  claimKey(action, id) {
    return action === 'jobsearch' ? 'job:' + id : action === 'socialize' ? 'company:' + id : action === 'wander' ? 'walk:' + id : 'place:' + id;
  }

  begin(person, choice) {
    const key = choice.action + ':' + choice.target;
    if (person.service.key !== key) person.service = {key, elapsed: 0};
    person.lastAction = choice.action; person.lastTarget = choice.target;
    person.explanation = choice.action === 'work' ? 'Going to work for wages and town supplies.' :
      choice.action === 'eat' ? 'Buying a meal because hunger matters more right now.' :
      choice.action === 'rest' ? 'Heading home to recover energy.' :
      choice.action === 'socialize' ? 'Looking for a real nearby neighbour to talk to.' :
      choice.action === 'jobsearch' ? 'Going to the noticeboard to apply for a real vacancy.' : 'Taking a short walk.';
    if (choice.action === 'socialize') this.invitations.set(choice.target, person.id);
    this.routes.delete(person.id);
  }

  stop(person, choice) {
    this.stopBody(person.id);
    this.helpers.get(person.id)?.claims.release(this.claimKey(choice.action, choice.target));
    if (this.invitations.get(choice.target) === person.id) this.invitations.delete(choice.target);
    this.routes.delete(person.id);
  }

  finish(person, status) { this.agents.get(person.id).finish(status, this); }

  perform(person, choice, dt) {
    const actor = lookup(person.id), destination = lookup(choice.action === 'jobsearch' ? '1105' : choice.target);
    if (!person.alive || !actor?.valid || !destination?.valid) { this.finish(person, 'failure'); return; }
    if (!this.helpers.get(person.id).claims.acquire(this.claimKey(choice.action, choice.target), 2)) {
      person.explanation = 'Waiting: another resident has this resource.';
      this.finish(person, 'failure'); return;
    }
    const delta = this.current?.gameDt ?? dt;
    const job = this.jobFor(person);
    if (choice.action === 'work' && (!job || !job.open || job.target !== choice.target)) {
      this.finish(person, 'failure'); return;
    }
    if (choice.action === 'jobsearch') {
      const vacancy = this.state.jobs.find(row => row.target === choice.target);
      if (person.jobId || !this.state.hiringOpen || !vacancy?.open || vacancy.employeeId) {
        this.finish(person, 'failure'); return;
      }
    }

    if (choice.action === 'eat' && (!this.state.shop.open || this.state.shop.stock <= 0 || person.money < 2)) {
      person.explanation = 'The meal is no longer available. Reconsidering.';
      this.finish(person, 'failure'); return;
    }
    if (choice.action === 'socialize') {
      if (!this.resident(choice.target)?.alive) { this.finish(person, 'failure'); return; }
      const pair = [person.id, choice.target].sort().join(':');
      if (this.state.socialPairs[pair] && this.state.time - this.state.socialPairs[pair] < 5) {
        this.finish(person, 'cancelled'); return;
      }
    }
    const social = choice.action === 'socialize';
    if (!this.walk(person, destination.transform.position, dt, social ? .9 : .22)) return;
    if (choice.action === 'wander') { this.finish(person, 'success'); return; }
    person.service.elapsed += delta;
    if (choice.action === 'jobsearch' && person.service.elapsed >= 2.5) {
      const vacancy = this.state.jobs.find(row => row.target === choice.target);
      if (!this.acceptJob(person, vacancy)) { this.finish(person, 'failure'); return; }
      person.service.elapsed = 0;
      this.finish(person, 'success');
    } else if (choice.action === 'work' && person.service.elapsed >= 5) {
      person.service.elapsed = 0; person.workDone++; person.money += job.wage; person.workEarnings += job.wage;
      person.lastWorkedAt = this.state.time; person.absenceWorkingSeconds = 0; job.completedTasks++;
      person.fatigue = clamp(person.fatigue + .035); person.hunger = clamp(person.hunger + .025);
      if (job.id === 'baker') { const made = Math.min(3, this.state.shop.grain); this.state.shop.grain -= made; this.state.shop.stock += made; }
      if (job.id === 'carpenter') this.state.shop.wood += 2;
      if (job.id === 'gardener') this.state.shop.grain += 3;
      if (job.id === 'shopkeeper') this.state.shop.stock += 1;
      if (job.id === 'courier') { this.state.shop.parcels++; this.state.shop.stock += 1; }
      this.log(person.name + ' finished a ' + job.title.toLowerCase() + ' task and earned ' + job.wage + ' coins.');
      this.finish(person, 'success');
    } else if (choice.action === 'eat' && person.service.elapsed >= 1.8) {
      // The precondition is rechecked above on every step, including publication.
      person.service.elapsed = 0; person.money -= 2; person.spent += 2;
      person.meals++; person.hunger = clamp(person.hunger - .7);
      person.health = clamp(person.health + .05);
      this.state.shop.stock--; this.state.shop.sales++;
      this.log(person.name + ' bought lunch for 2 coins.'); this.finish(person, 'success');
    } else if (choice.action === 'rest' && person.service.elapsed >= 3.5) {
      person.service.elapsed = 0; person.rests++; person.fatigue = clamp(person.fatigue - .75);
      person.health = clamp(person.health + .2);
      this.log(person.name + ' rested at home.'); this.finish(person, 'success');
    } else if (choice.action === 'socialize' && person.service.elapsed >= 2) {
      const other = this.resident(choice.target);
      person.service.elapsed = 0; person.social = clamp(person.social - .65); person.chats++;
      if (other) { other.social = clamp(other.social - .65); other.chats++; }
      this.state.socialPairs[[person.id, choice.target].sort().join(':')] = this.state.time;
      this.log(person.name + ' chatted with ' + (other?.name ?? 'a neighbour') + '.');
      this.finish(person, 'success');
    }
  }

  // Cache routes only while their target remains unchanged. Moving neighbours
  // trigger a bounded replan; solid scenery always vetoes a desired step.
  walk(person, destination, dt, arrival) {
    const actor = lookup(person.id), position = actor.transform.position;
    if (length(position, destination) <= arrival) { this.stopBody(person.id); return true; }
    let plan = this.routes.get(person.id);
    if (!plan || length(plan.destination, destination) > .8 || plan.stuck > 1) {
      plan = {points: route(position, destination), destination: {...destination}, index: 0, stuck: 0};
      this.routes.set(person.id, plan);
    }
    if (!plan.points.length) { person.explanation = 'No clear route to the destination.'; this.stopBody(person.id); return false; }
    // Honour each grid corner within 1cm. The .29m planning inflation exceeds
    // the real .24m half-extent; a broad .12m turn radius consumed that clearance.
    while (plan.index + 1 < plan.points.length && length(position, plan.points[plan.index]) < .01) plan.index++;
    const waypoint = plan.points[plan.index], distance = length(position, waypoint);
    if (distance < .01) { this.stopBody(person.id); return false; }
    const direction = {x: (waypoint.x - position.x) / distance, y: (waypoint.y - position.y) / distance};
    const speed = Math.min(2.6 * (this.state.speed === 3 ? 3 : 1), distance / dt);
    if (this.move(actor, direction, speed, dt)) plan.stuck = 0;
    else plan.stuck += dt;
    return false;
  }

  move(actor, direction, speed, dt) {
    // Pedestrian passing is deliberate town policy, as in many top-down games.
    // These six kinematic residents do not obstruct one another. They remain
    // real published bodies for perception; walls, furniture and every other
    // physical object still block movement. Service leases still prevent two
    // residents using the same till/bed/workstation at the same time.
    // A forward-only sidestep heuristic could deadlock overlapping pedestrians
    // at the shop entrance; this game does not pretend to simulate crowd traffic.
    const ignored = this.pedestrians.filter(person => person.valid);
    const axis = {x: direction.x, y: direction.y, z: 0};
    const travel = speed * dt;
    const hit = physics.cast2D({kind: 'box', halfExtents: {x: .24, y: .24}},
      actor.transform, axis, travel + .025, {ignored, includeSensors: false});
    if (hit && hit.distance < travel + .02) {
      actor.setKinematicVelocity(ZERO, ZERO);
      return false;
    }
    actor.setKinematicVelocity({x: axis.x * speed, y: axis.y * speed, z: 0}, ZERO);
    return true;
  }

  dispose() {
    for (const agent of this.agents.values()) agent.dispose();
    for (const helper of this.helpers.values()) { helper.claims.dispose(); helper.senses.dispose(); }
    this.agents.clear(); this.helpers.clear(); this.routes.clear(); this.invitations.clear();
    this.pedestrians = [];
  }
}
export default Town;
