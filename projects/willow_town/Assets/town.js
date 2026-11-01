// Willow Town owns needs, jobs and movement policy. Judas owns utility decisions,
// physical collision, sensing, resource leases and persistence.
import {UtilityAgent, AISenses, AITargetReservations, entity, physics} from 'judas';
import definition from './citizen.ai.js';
import layout from './layout.js';

const ZERO = {x: 0, y: 0, z: 0};
const clamp = value => Math.max(0, Math.min(1, value));
const length = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);
const lookup = id => entity(String(id));
const people = [
  ['301', 'Mara', 'Baker', .82, .22, .22],
  ['302', 'Eli', 'Carpenter', .22, .84, .18],
  ['303', 'Nia', 'Gardener', .30, .22, .92],
  ['304', 'Theo', 'Shopkeeper', .24, .28, .70],
  ['305', 'Ada', 'Courier', .44, .34, .20],
  ['306', 'Sam', 'Park keeper', .30, .24, .56]
];
const ids = new Set(people.map(person => person[0]));

export function createState() {
  return {
    time: 0, paused: false, speed: 1, controlled: null,
    selected: '301', message: 'Click a resident to meet them and inspect their decisions.',
    shop: {stock: 16, sales: 0, grain: 3, wood: 0, parcels: 0, open: true},
    events: [], socialPairs: {},
    residents: people.map((person, index) => ({
      id: person[0], name: person[1], job: person[2],
      hunger: person[3], fatigue: person[4], social: person[5], health: 1,
      money: 8, workEarnings: 0, spent: 0, workDone: 0, meals: 0, rests: 0, chats: 0,
      workTarget: String(1101 + index), bedTarget: String(1201 + index),
      snapshot: null, service: {key: '', elapsed: 0},
      lastAction: '', lastTarget: '', explanation: 'Getting ready for the day.'
    }))
  };
}

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
    for (const resident of this.state.residents) {
      const actor = lookup(resident.id);
      if (!actor?.valid) throw Error('Willow Town is missing resident ' + resident.name);
      this.pedestrians.push(actor);
      const helpers = {
        claims: new AITargetReservations(),
        senses: new AISenses({interval: .5, maxResults: 64, maxCandidates: 128, maxRaycasts: 0})
      };
      this.helpers.set(resident.id, helpers);
      const handlers = {};
      for (const action of ['work', 'eat', 'rest', 'socialize', 'wander']) {
        handlers[action] = {
          start: (_context, choice) => this.begin(resident, choice),
          update: (dt, _context, choice) => this.perform(resident, choice, dt),
          stop: (_context, choice) => this.stop(resident, choice)
        };
      }
      const agent = new UtilityAgent({...definition, initialDelay: (Number(resident.id) - 301) * .04},
        handlers, resident.name + ' — ' + resident.job);
      this.agents.set(resident.id, agent);
      if (restoring && resident.snapshot) agent.restore(resident.snapshot);
    }
    if (this.state.controlled && !this.resident(this.state.controlled)) this.state.controlled = null;
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

  setNeed(id, key, value) {
    const person = this.resident(id);
    if (!person || !['hunger', 'fatigue', 'social', 'health'].includes(key) || !Number.isFinite(value)) return false;
    person[key] = clamp(value);
    this.log(person.name + ': ' + key + ' changed. Their AI will reconsider at its next evaluation.');
    return true;
  }

  setControlled(id) {
    const next = id === null ? null : String(id);
    if (next && !this.resident(next)) return false;
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
      resident.hunger = clamp(resident.hunger + elapsed * .006);
      resident.fatigue = clamp(resident.fatigue + elapsed * .0035);
      resident.social = clamp(resident.social + elapsed * .003);
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
    if (!id || this.state.paused || !Number.isFinite(dt) || dt <= 0) return;
    const size = Math.hypot(direction.x, direction.y);
    if (!Number.isFinite(size) || size < .01) { this.stopBody(id); return; }
    const actor = lookup(id);
    if (actor?.valid) this.move(actor, {x: direction.x / size, y: direction.y / size},
      3 * (this.state.speed === 3 ? 3 : 1), dt);
  }

  candidates(person) {
    const actor = lookup(person.id);
    if (!actor?.valid) return [];
    const position = actor.transform.position;
    const helpers = this.helpers.get(person.id);
    const make = (action, id, facts, eligible = true, reason = '') => {
      const target = lookup(id);
      const available = helpers.claims.available(this.claimKey(action, id));
      return {action, target: String(id), facts: {...facts, distance: target?.valid ? length(position, target.transform.position) : 65},
        eligible: eligible && !!target?.valid && available,
        reason: !target?.valid ? 'The destination no longer exists' : !eligible ? reason : !available ? 'Another resident is using this place' : ''};
    };
    const hour = (8 + this.state.time / 18) % 24;
    const onShift = hour >= 8 && hour < 18;
    const work = make('work', person.workTarget,
      {duty: onShift ? 1 : .12, readiness: Math.max(.05, 1 - person.hunger * .45 - person.fatigue * .55)},
      person.health > .1, 'Too unwell to work');
    const foodReady = this.state.shop.open && this.state.shop.stock > 0 && person.money >= 2;
    const foodReason = !this.state.shop.open ? 'The shop is closed' : this.state.shop.stock <= 0 ? 'Food is sold out' : 'A meal costs 2 coins';
    const output = [work,
      make('eat', '1301', {hunger: person.hunger}, foodReady, foodReason),
      make('eat', '1302', {hunger: person.hunger}, foodReady, foodReason),
      make('rest', person.bedTarget, {fatigue: person.fatigue})];
    const observation = helpers.senses.sample(position, 12, {visibility: false, planar: true});
    this.observations.set(person.id, observation);
    const neighbours = observation.targets.filter(candidate => ids.has(candidate.entityId) && candidate.entityId !== person.id);
    for (const neighbour of neighbours) {
      const other = this.resident(neighbour.entityId);
      const invitation = this.invitations.get(person.id) === other.id;
      const pair = [person.id, other.id].sort().join(':');
      const recent = this.state.socialPairs[pair];
      output.push(make('socialize', other.id,
        {company: invitation ? Math.max(.86, person.social) : person.social},
        (!recent || this.state.time - recent > 5) && other.health > .1,
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
    return action === 'socialize' ? 'company:' + id : action === 'wander' ? 'walk:' + id : 'place:' + id;
  }

  begin(person, choice) {
    const key = choice.action + ':' + choice.target;
    if (person.service.key !== key) person.service = {key, elapsed: 0};
    person.lastAction = choice.action; person.lastTarget = choice.target;
    person.explanation = choice.action === 'work' ? 'Going to work for wages and town supplies.' :
      choice.action === 'eat' ? 'Buying a meal because hunger matters more right now.' :
      choice.action === 'rest' ? 'Heading home to recover energy.' :
      choice.action === 'socialize' ? 'Looking for a real nearby neighbour to talk to.' : 'Taking a short walk.';
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
    const actor = lookup(person.id), destination = lookup(choice.target);
    if (!actor?.valid || !destination?.valid) { this.finish(person, 'failure'); return; }
    if (!this.helpers.get(person.id).claims.acquire(this.claimKey(choice.action, choice.target), 2)) {
      person.explanation = 'Waiting: another resident has this resource.';
      this.finish(person, 'failure'); return;
    }
    const delta = this.current?.gameDt ?? dt;
    if (choice.action === 'eat' && (!this.state.shop.open || this.state.shop.stock <= 0 || person.money < 2)) {
      person.explanation = 'The meal is no longer available. Reconsidering.';
      this.finish(person, 'failure'); return;
    }
    if (choice.action === 'socialize') {
      const pair = [person.id, choice.target].sort().join(':');
      if (this.state.socialPairs[pair] && this.state.time - this.state.socialPairs[pair] < 5) {
        this.finish(person, 'cancelled'); return;
      }
    }
    const social = choice.action === 'socialize';
    if (!this.walk(person, destination.transform.position, dt, social ? .9 : .22)) return;
    if (choice.action === 'wander') { this.finish(person, 'success'); return; }
    person.service.elapsed += delta;
    if (choice.action === 'work' && person.service.elapsed >= 5) {
      person.service.elapsed = 0; person.workDone++; person.money += 3; person.workEarnings += 3;
      person.fatigue = clamp(person.fatigue + .035); person.hunger = clamp(person.hunger + .025);
      const job = Number(person.id) - 301;
      if (job === 0) { const made = Math.min(3, this.state.shop.grain); this.state.shop.grain -= made; this.state.shop.stock += made; }
      if (job === 1) this.state.shop.wood += 2;
      if (job === 2) this.state.shop.grain += 3;
      if (job === 3) this.state.shop.stock += 1;
      if (job === 4) { this.state.shop.parcels++; this.state.shop.stock += 1; }
      this.log(person.name + ' finished a shift task and earned 3 coins.');
      this.finish(person, 'success');
    } else if (choice.action === 'eat' && person.service.elapsed >= 1.8) {
      // The precondition is rechecked above on every step, including publication.
      person.service.elapsed = 0; person.money -= 2; person.spent += 2;
      person.meals++; person.hunger = clamp(person.hunger - .7);
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
