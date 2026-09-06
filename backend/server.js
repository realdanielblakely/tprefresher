const express = require('express');
const path = require('path');
const { allStatuses, update, levels } = require('./store');
const app = express();
const port = Number(process.env.PORT || 3000);
const apiToken = process.env.API_TOKEN || '';
app.use(express.json({ limit: '16kb' }));
app.use(express.static(path.join(__dirname, '..', 'public')));
function tokenIsValid(req) {
  if (!apiToken) return true;
  const bearer = req.get('authorization') || '';
  return req.get('x-api-token') === apiToken || bearer === `Bearer ${apiToken}`;
}
function requireToken(req, res, next) {
  if (tokenIsValid(req)) return next();
  return res.status(401).json({ error: 'API token required' });
}
function knownId(req, res, next) {
  const id = String(req.params.id || '').toLowerCase();
  if (!/^[a-z0-9-]+$/.test(id)) return res.status(400).json({ error: 'Invalid bathroom id' });
  req.bathroomId = id;
  return next();
}
app.get('/api/status', async (_req, res, next) => {
  try { res.json({ bathrooms: await allStatuses() }); } catch (error) { next(error); }
});
function knownLevel(req, res, next) {
  const level = String(req.params.level || req.query.level || req.body?.level || 'low').toLowerCase();
  if (!levels.includes(level)) return res.status(400).json({ error: `Level must be one of ${levels.join(', ')}` });
  req.level = level;
  return next();
}
async function flag(req, res, next) {
  try {
    const result = await update(req.bathroomId, new Date().toISOString(), req.level);
    if (!result) return res.status(404).json({ error: 'Bathroom not found' });
    return res.json(result);
  } catch (error) { return next(error); }
}
app.post('/api/bathrooms/:id/flag', requireToken, knownId, knownLevel, flag);
app.get('/api/bathrooms/:id/flag', requireToken, knownId, knownLevel, flag);
// Level in the path so a bookmark or a QR code can report severity directly.
app.post('/api/bathrooms/:id/flag/:level', requireToken, knownId, knownLevel, flag);
app.get('/api/bathrooms/:id/flag/:level', requireToken, knownId, knownLevel, flag);
app.post('/api/bathrooms/:id/confirm', requireToken, knownId, async (req, res, next) => {
  try {
    const result = await update(req.bathroomId, null);
    if (!result) return res.status(404).json({ error: 'Bathroom not found' });
    return res.json(result);
  } catch (error) { next(error); }
});
// Alexa discovery runs beside the main server. It is optional: if the port is not
// available the app keeps serving normally and just logs why.
if (process.env.ALEXA !== 'off') {
  try { require('./alexa').start(); } catch (error) { console.log('alexa: not started,', error.message); }
}
app.use((error, _req, res, _next) => {
  console.error(error);
  res.status(500).json({ error: 'Internal server error' });
});
app.listen(port, () => {
  console.log(`TP Refresher listening on http://localhost:${port}`);
  if (apiToken) console.log('API token protection is enabled');
});
