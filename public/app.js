const container = document.querySelector('#bathrooms');
const connection = document.querySelector('#connection');
const errorBox = document.querySelector('#error');
const levels = [
  { id: 'low', label: 'Running low', button: 'Running low' },
  { id: 'urgent', label: 'Dangerously low', button: 'Dangerously low' },
  { id: 'out', label: 'Totally out', button: 'Totally out' }
];
let latest = [];
function labelFor(state) {
  const match = levels.find((level) => level.id === state);
  return match ? match.label : 'All clear';
}
function ageText(when) {
  if (!when) return 'No refresh needed';
  const hours = Math.floor((Date.now() - new Date(when).getTime()) / 3600000);
  if (hours < 1) return 'Flagged just now';
  if (hours === 1) return 'Flagged 1 hour ago';
  return `Flagged ${hours} hours ago`;
}
function detailText(item) {
  if (item.state === 'ok') return ageText(null);
  const base = ageText(item.flaggedAt);
  if (item.reported && item.reported !== item.state) return `${base}, escalated from ${labelFor(item.reported).toLowerCase()}`;
  return base;
}
function actionButton(text, className, handler) {
  const button = document.createElement('button');
  button.type = 'button'; button.className = className; button.textContent = text;
  button.addEventListener('click', () => handler(button));
  return button;
}
function render(items) {
  latest = items;
  container.replaceChildren(...items.map((item) => {
    const card = document.createElement('article'); card.className = `card ${item.state}`;
    const copy = document.createElement('div');
    const badge = document.createElement('span'); badge.className = 'badge'; badge.textContent = labelFor(item.state);
    const title = document.createElement('h2'); title.textContent = item.name;
    const detail = document.createElement('p'); detail.className = 'detail'; detail.textContent = detailText(item);
    copy.append(badge, title, detail);
    const actions = document.createElement('div'); actions.className = 'actions';
    if (item.state !== 'ok') actions.append(actionButton('Confirm restock', 'action confirm', (button) => send(item, 'confirm', button)));
    for (const level of levels) {
      if (level.id === item.reported) continue;
      actions.append(actionButton(level.button, `action level ${level.id}`, (button) => send(item, level.id, button)));
    }
    card.append(copy, actions); return card;
  }));
}
async function send(item, action, button) {
  button.disabled = true; errorBox.textContent = '';
  const path = action === 'confirm'
    ? `/api/bathrooms/${encodeURIComponent(item.id)}/confirm`
    : `/api/bathrooms/${encodeURIComponent(item.id)}/flag/${action}`;
  try {
    const response = await fetch(path, { method: 'POST' });
    if (!response.ok) throw new Error('Request failed');
    await refresh();
  } catch (_error) {
    errorBox.textContent = 'Could not save that change. Check your connection and try again.';
    button.disabled = false;
  }
}
async function refresh() {
  try {
    const response = await fetch('/api/status', { cache: 'no-store' });
    if (!response.ok) throw new Error('Status failed');
    const payload = await response.json(); render(payload.bathrooms);
    connection.textContent = 'Live'; connection.classList.remove('offline'); errorBox.textContent = '';
  } catch (_error) {
    connection.textContent = 'Offline'; connection.classList.add('offline');
    if (!latest.length) errorBox.textContent = 'The status board is not reachable yet.';
  }
}
refresh();
setInterval(refresh, 5000);
