const fs = require('fs/promises');
const path = require('path');
const { bathrooms } = require('./config');
const dataFile = process.env.DATA_FILE || path.join(__dirname, 'data.json');
const overdueMs = 24 * 60 * 60 * 1000;
let state = null;
let writeQueue = Promise.resolve();

async function load() {
  if (state) return state;
  try { state = JSON.parse(await fs.readFile(dataFile, 'utf8')); }
  catch (error) {
    if (error.code !== 'ENOENT') throw error;
    state = Object.fromEntries(bathrooms.map(({ id }) => [id, { flaggedAt: null }]));
    await persist();
  }
  for (const { id } of bathrooms) if (!state[id]) state[id] = { flaggedAt: null };
  return state;
}
async function persist() {
  await fs.mkdir(path.dirname(dataFile), { recursive: true });
  const temp = `${dataFile}.tmp`;
  await fs.writeFile(temp, JSON.stringify(state, null, 2) + '\n', 'utf8');
  await fs.rename(temp, dataFile);
}
function save() { writeQueue = writeQueue.then(persist); return writeQueue; }
function statusFor(bathroom) {
  const flaggedAt = state[bathroom.id]?.flaggedAt || null;
  if (!flaggedAt) return { ...bathroom, state: 'ok', flaggedAt: null };
  const age = Date.now() - new Date(flaggedAt).getTime();
  return { ...bathroom, state: age >= overdueMs ? 'overdue' : 'needs', flaggedAt };
}
async function allStatuses() { await load(); return bathrooms.map(statusFor); }
async function update(id, flaggedAt) {
  await load();
  const bathroom = bathrooms.find((entry) => entry.id === id);
  if (!bathroom) return null;
  state[id] = { flaggedAt };
  await save();
  return statusFor(bathroom);
}
module.exports = { allStatuses, update, dataFile };
