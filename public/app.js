const container = document.querySelector('#bathrooms');
const connection = document.querySelector('#connection');
const errorBox = document.querySelector('#error');
let latest = [];
function ageText(when) {
  if (!when) return 'No refresh needed';
  const hours = Math.floor((Date.now() - new Date(when).getTime()) / 3600000);
  if (hours < 1) return 'Flagged just now';
  if (hours === 1) return 'Flagged 1 hour ago';
  return `Flagged ${hours} hours ago`;
}
function render(items) {
  latest = items;
  container.replaceChildren(...items.map((item) => {
    const card = document.createElement('article'); card.className = `card ${item.state}`;
    const copy = document.createElement('div');
    const badge = document.createElement('span'); badge.className = 'badge';
    badge.textContent = item.state === 'ok' ? 'All clear' : item.state === 'overdue' ? 'High alert' : 'Needs a roll';
    const title = document.createElement('h2'); title.textContent = item.name;
    const detail = document.createElement('p'); detail.className = 'detail'; detail.textContent = ageText(item.flaggedAt);
    copy.append(badge, title, detail);
    const button = document.createElement('button'); button.className = 'action'; button.type = 'button';
    button.textContent = item.state === 'ok' ? 'Flag bathroom' : 'Confirm restock';
    button.addEventListener('click', () => changeState(item, button));
    card.append(copy, button); return card;
  }));
}
async function changeState(item, button) {
  button.disabled = true; errorBox.textContent = '';
  const action = item.state === 'ok' ? 'flag' : 'confirm';
  try {
    const response = await fetch(`/api/bathrooms/${encodeURIComponent(item.id)}/${action}`, { method: 'POST' });
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
