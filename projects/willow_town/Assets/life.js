// Small, deliberately compressed town-life rules. These are game data, not
// engine AI or biology: one town day is 432 game seconds, and ages use town days.
export const DAY_SECONDS = 432;
export const ABSENCE_LIMIT = 65;

const people = [
  ['301', 'Mara', .82, .22, .22, 28, 78],
  ['302', 'Eli', .22, .84, .18, 34, 82],
  ['303', 'Nia', .30, .22, .92, 24, 76],
  ['304', 'Theo', .24, .28, .70, 42, 80],
  ['305', 'Ada', .44, .34, .20, 22, 79],
  ['306', 'Sam', .30, .24, .56, 77, 80]
];
export const JOBS = [
  {id: 'baker', title: 'Baker', target: '1101'},
  {id: 'carpenter', title: 'Carpenter', target: '1102'},
  {id: 'gardener', title: 'Gardener', target: '1103'},
  {id: 'shopkeeper', title: 'Shopkeeper', target: '1104'},
  {id: 'courier', title: 'Courier', target: '1105'},
  {id: 'keeper', title: 'Park keeper', target: '1106'}
];
export const clamp = value => Math.max(0, Math.min(1, value));
export const hourAt = time => (8 + time / 18) % 24;
export const workingHours = time => hourAt(time) >= 8 && hourAt(time) < 18;

export function createState() {
  return {
    version: 2, time: 0, paused: false, speed: 1, controlled: null,
    selected: '301', message: 'Ada is looking for a job. Click a resident to see their life and decisions.',
    hiringOpen: true,
    jobs: JOBS.map((job, index) => ({...job, wage: 3, open: true,
      employeeId: index === 4 ? null : people[index][0], completedTasks: 0})),
    census: {deaths: 0, hires: 0, firings: 0},
    shop: {stock: 16, sales: 0, grain: 3, wood: 0, parcels: 0, open: true},
    events: [], socialPairs: {},
    residents: people.map((person, index) => ({
      id: person[0], name: person[1], alive: true,
      ageDays: person[5], lifespanDays: person[6], deathCause: '', deathTime: null,
      jobId: index === 4 ? null : JOBS[index].id, lastJobId: JOBS[index].id,
      job: index === 4 ? 'Unemployed' : JOBS[index].title,
      workTarget: index === 4 ? null : JOBS[index].target,
      jobLostReason: index === 4 ? 'Arrived in town seeking work' : '',
      lastEmploymentChange: 0, jobsLost: 0, jobsFound: 0,
      absenceWorkingSeconds: 0, lastWorkedAt: 0,
      starvationSeconds: 0, exhaustionSeconds: 0,
      hunger: person[2], fatigue: person[3], social: person[4], health: 1,
      money: 8, workEarnings: 0, spent: 0, workDone: 0, meals: 0, rests: 0, chats: 0,
      bedTarget: String(1201 + index), history: [],
      snapshot: null, lastDecision: null, service: {key: '', elapsed: 0},
      lastAction: '', lastTarget: '', explanation: 'Getting ready for the day.'
    }))
  };
}

// Return an actual terminal cause. High need bars alone do not kill instantly:
// sustained severe neglect first accumulates, then health is lost over time.
export function advanceVitals(person, seconds) {
  person.ageDays += seconds / DAY_SECONDS;
  person.hunger = clamp(person.hunger + seconds * .006);
  person.fatigue = clamp(person.fatigue + seconds * .0035);
  person.social = clamp(person.social + seconds * .003);
  const previousStarvation = person.starvationSeconds, previousExhaustion = person.exhaustionSeconds;
  person.starvationSeconds = person.hunger >= .98 ? person.starvationSeconds + seconds :
    Math.max(0, person.starvationSeconds - seconds * 2);
  person.exhaustionSeconds = person.fatigue >= .98 ? person.exhaustionSeconds + seconds :
    Math.max(0, person.exhaustionSeconds - seconds * 2);
  const starvationDamageTime = Math.max(0, person.starvationSeconds - 20) - Math.max(0, previousStarvation - 20);
  const exhaustionDamageTime = Math.max(0, person.exhaustionSeconds - 40) - Math.max(0, previousExhaustion - 40);
  if (starvationDamageTime > 0) person.health = clamp(person.health - starvationDamageTime * .006);
  if (exhaustionDamageTime > 0) person.health = clamp(person.health - exhaustionDamageTime * .0015);
  if (person.ageDays >= person.lifespanDays) return 'Old age';
  if (person.health <= 0) return person.starvationSeconds > 20 ? 'Starvation' :
    person.exhaustionSeconds > 40 ? 'Exhaustion' : 'Injuries';
  return '';
}
