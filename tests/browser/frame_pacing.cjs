// Execute the actual embedded browser wait body against a deterministic GPU
// and callback scheduler. No browser installation or compiled runtime required.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const source = fs.readFileSync(path.join(__dirname, '../../src/Window.cpp'), 'utf8');
const match = source.match(/EM_ASYNC_JS\(void, AwaitBrowserFrame, \(\), \{([\s\S]*?)\n\}\);/);
assert(match, 'browser frame wait is present');
const AsyncFunction = Object.getPrototypeOf(async function() {}).constructor;
const wait = new AsyncFunction('GLctx', 'requestAnimationFrame',
  'cancelAnimationFrame', 'setTimeout', 'clearTimeout', match[1]);
let checks = 0;
function check(value, message) { assert(value, message); ++checks; }
function fixture(states, options = {}) {
  let next = 1, lost = false, created = 0, deleted = 0, flushed = 0;
  const pending = new Map(), fence = {};
  const gl = {
    SYNC_GPU_COMMANDS_COMPLETE: 1, ALREADY_SIGNALED: 2,
    CONDITION_SATISFIED: 3, WAIT_FAILED: 4, TIMEOUT_EXPIRED: 5,
    fenceSync(condition, flags) {
      assert.equal(condition, 1); assert.equal(flags, 0); ++created;
      return options.noFence ? null : fence;
    },
    flush() { ++flushed; if (options.flushThrows) throw Error('flush failed'); },
    clientWaitSync(sync, flags, timeout) {
      assert.equal(sync, fence); assert.equal(flags, 0); assert.equal(timeout, 0);
      return states.length ? states.shift() : this.TIMEOUT_EXPIRED;
    },
    isContextLost() { return lost; },
    deleteSync(sync) { assert.equal(sync, fence); ++deleted; }
  };
  function schedule(kind, callback, delay) {
    const id = next++; pending.set(id, {kind, callback, delay}); return id;
  }
  let settled = false;
  const promise = wait(gl, cb => schedule('raf', cb), id => pending.delete(id),
    (cb, delay) => schedule('timer', cb, delay), id => pending.delete(id));
  // Observe rejection immediately, retaining it for the explicit assertion.
  const outcome = promise.then(() => { settled = true; }, error => {
    settled = true; return error;
  });
  return {
    pending, outcome, stats: () => ({created, deleted, flushed, settled}),
    lose: () => { lost = true; },
    async tick(kind) {
      const entry = [...pending].find(([, value]) => value.kind === kind);
      assert(entry, 'requested callback is scheduled');
      pending.delete(entry[0]); entry[1].callback();
      await Promise.resolve(); await Promise.resolve();
    }
  };
}
(async () => {
  for (const state of [2, 3]) {
    const f = fixture([state]);
    check(f.stats().created === 1 && f.stats().flushed === 1, 'one fence and flush');
    check(!f.stats().settled, 'no new frame before presentation boundary');
    await f.tick('raf'); await f.outcome;
    check(f.stats().deleted === 1 && f.pending.size === 0, 'successful frame cleans both wakeups and fence');
  }
  const slow = fixture(Array(12).fill(5).concat(3));
  for (let i = 0; i < 12; ++i) {
    await slow.tick(i % 2 ? 'timer' : 'raf');
    check(!slow.stats().settled && slow.stats().created === 1 && slow.pending.size === 2,
      'slow GPU cannot admit another frame or accumulate callbacks');
  }
  await slow.tick('raf'); await slow.outcome;
  check(slow.pending.size === 0 && slow.stats().deleted === 1, 'slow frame eventually releases resources');
  const hidden = fixture([3]);
  check([...hidden.pending.values()].some(v => v.kind === 'timer' && v.delay === 250), 'hidden-tab lifecycle fallback retained');
  await hidden.tick('timer'); await hidden.outcome;
  check(hidden.pending.size === 0, 'timer fallback cancels unavailable RAF');
  for (const lost of [false, true]) {
    const f = fixture([lost ? 5 : 4]); if (lost) f.lose();
    await f.tick('raf'); const error = await f.outcome;
    check(error instanceof Error && f.pending.size === 0 && f.stats().deleted === 1,
      'failed or lost context rejects and releases resources');
  }
  for (const options of [{noFence: true}, {flushThrows: true}]) {
    const f = fixture([], options), error = await f.outcome;
    check(error instanceof Error && f.pending.size === 0 &&
      f.stats().deleted === (options.noFence ? 0 : 1), 'setup failure does not leak a fence or timer');
  }
  console.log(`${checks} browser frame-pacing checks passed`);
})().catch(error => { console.error(error); process.exitCode = 1; });
