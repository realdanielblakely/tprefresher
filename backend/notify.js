// Email alerts for finished laundry. Configured entirely by environment, and a
// no-op when unconfigured, so the app runs fine before credentials exist.
//
// MAIL_TO accepts several comma separated addresses. Most carriers run an
// email to SMS gateway, so one of them can be a phone number and arrive as a text.
const nodemailer = require('nodemailer');

const host = process.env.SMTP_HOST || '';
const port = Number(process.env.SMTP_PORT || 465);
const user = process.env.SMTP_USER || '';
const pass = process.env.SMTP_PASS || '';
const from = process.env.MAIL_FROM || user;
const to = (process.env.MAIL_TO || '').split(',').map((entry) => entry.trim()).filter(Boolean);

const configured = Boolean(host && user && pass && to.length);
let transport = null;

function ready() { return configured; }
function describe() {
  if (!configured) return 'email alerts disabled, set SMTP_HOST, SMTP_USER, SMTP_PASS and MAIL_TO';
  return `email alerts to ${to.join(', ')} via ${host}:${port}`;
}

async function send(subject, text) {
  if (!configured) { console.log(`notify: skipped, not configured. would have sent "${subject}"`); return false; }
  if (!transport) transport = nodemailer.createTransport({ host, port, secure: port === 465, auth: { user, pass } });
  try {
    await transport.sendMail({ from, to, subject, text });
    console.log(`notify: sent "${subject}"`);
    return true;
  } catch (error) {
    console.log(`notify: FAILED "${subject}":`, error.message);
    return false;
  }
}

// Kept short on purpose. These land on a phone lock screen and often as an SMS,
// where only the first few words are visible.
async function laundryDone(machine) {
  const nth = machine.reminder > 1 ? ` (reminder ${machine.reminder})` : '';
  const tail = machine.finalReminder ? '\n\nThis is the last reminder.' : '';
  return send(
    `${machine.name} is done${nth}`,
    `The ${machine.name.toLowerCase()} cycle finished. Go move the laundry.\n\nTap the machine on the closet display to clear this.${tail}`
  );
}

module.exports = { laundryDone, send, ready, describe };
