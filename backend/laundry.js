// Laundry timers. These live on the server, not the display, so a reboot or a
// firmware push does not lose a running cycle, and so the alert fires from the
// machine that actually has internet.
const fs = require('fs/promises');
const path = require('path');

const dataFile = process.env.LAUNDRY_FILE || path.join(__dirname, 'laundry.json');
const machines = [
  { id: 'wash', name: 'Wash', minutes: Number(process.env.WASH_MINUTES || 35) },
  { id: 'dry', name: 'Dry', minutes: Number(process.env.DRY_MINUTES || 60) }
];
// Laundry left in the machine is the whole problem, so a finished cycle nags.
const repeatEveryMs = Number(process.env.LAUNDRY_REPEAT_MINUTES || 15) * 60 * 1000;
const maxReminders = Number(process.env.LAUNDRY_MAX_REMINDERS || 3);

let state = null;
let writeQueue = Promise.resolve();

async function load() {
  if (state) return state;
  try { state = JSON.parse(await fs.readFile(dataFile, 'utf8')); }
  catch (error) {
    if (error.code !== 'ENOENT') throw error;
    state = {};
    await persist();
  }
  for (const { id } of machines) if (!state[id]) state[id] = { startedAt: null, minutes: null, reminders: 0, acknowledged: false };
  return state;
}
async function persist() {
  const temp = `${dataFile}.tmp`;
  await fs.writeFile(temp, JSON.stringify(state, null, 2) + '\n', 'utf8');
  await fs.rename(temp, dataFile);
}
function save() { writeQueue = writeQueue.then(persist); return writeQueue; }

function statusFor(machine) {
  const entry = state[machine.id] || {};
  if (!entry.startedAt) return { ...machine, state: 'idle', endsAt: null, remainingSeconds: 0 };
  const minutes = entry.minutes || machine.minutes;
  const endsAt = new Date(new Date(entry.startedAt).getTime() + minutes * 60000);
  const remaining = Math.round((endsAt.getTime() - Date.now()) / 1000);
  return {
    ...machine,
    minutes,
    state: remaining > 0 ? 'running' : 'done',
    startedAt: entry.startedAt,
    endsAt: endsAt.toISOString(),
    remainingSeconds: Math.max(0, remaining)
  };
}
async function all() { await load(); return machines.map(statusFor); }

async function start(id, minutes) {
  await load();
  const machine = machines.find((entry) => entry.id === id);
  if (!machine) return null;
  state[id] = { startedAt: new Date().toISOString(), minutes: minutes || machine.minutes, reminders: 0, acknowledged: false };
  await save();
  return statusFor(machine);
}
async function clear(id) {
  await load();
  const machine = machines.find((entry) => entry.id === id);
  if (!machine) return null;
  state[id] = { startedAt: null, minutes: null, reminders: 0, acknowledged: false };
  await save();
  return statusFor(machine);
}

// Called on a timer by the server. Returns the machines that just earned an alert.
async function due() {
  await load();
  const ready = [];
  let changed = false;
  for (const machine of machines) {
    const status = statusFor(machine);
    if (status.state !== 'done') continue;
    const entry = state[machine.id];
    if (entry.acknowledged || entry.reminders >= maxReminders) continue;
    const doneAt = new Date(status.endsAt).getTime();
    const nextDue = doneAt + entry.reminders * repeatEveryMs;
    if (Date.now() < nextDue) continue;
    entry.reminders += 1;
    changed = true;
    ready.push({ ...status, reminder: entry.reminders, finalReminder: entry.reminders >= maxReminders });
  }
  if (changed) await save();
  return ready;
}

module.exports = { all, start, clear, due, machines, dataFile };
