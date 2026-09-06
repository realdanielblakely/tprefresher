// Minimal Philips Hue bridge emulation so Alexa can find virtual switches on the
// LAN. No Amazon account, no internet exposure. Alexa Routines map a spoken phrase
// to one of these switches, and switching it on flags a bathroom.
//
// Echo devices expect a Hue bridge on port 80. See CLAUDE.md for the port note.
const dgram = require('dgram');
const http = require('http');
const os = require('os');
const { bathrooms } = require('./config');
const { allStatuses, update, levels } = require('./store');

const SSDP_ADDRESS = '239.255.255.250';
const SSDP_PORT = 1900;
const BRIDGE_ID = '001788FFFE0B0B0B';
const BRIDGE_UUID = '2f402f80-da50-11e1-9b23-0017880b0b0b';

function lanAddress() {
  for (const entries of Object.values(os.networkInterfaces())) {
    for (const entry of entries || []) {
      if (entry.family === 'IPv4' && !entry.internal) return entry.address;
    }
  }
  return '127.0.0.1';
}

// One virtual switch per bathroom per action. Turning one on performs the action.
function buildDevices() {
  const devices = [];
  for (const bathroom of bathrooms) {
    for (const level of levels) {
      devices.push({ id: String(devices.length + 1), name: `${bathroom.name} paper ${level}`, bathroom: bathroom.id, level });
    }
    devices.push({ id: String(devices.length + 1), name: `${bathroom.name} paper restocked`, bathroom: bathroom.id, level: null });
  }
  return devices;
}

function lightJson(device, on) {
  return {
    state: { on, bri: 254, alert: 'none', reachable: true },
    type: 'Dimmable light',
    name: device.name,
    modelid: 'LWB007',
    manufacturername: 'Philips',
    uniqueid: `00:17:88:5e:d3:00:00:${device.id.padStart(2, '0')}-0b`,
    swversion: '66009461'
  };
}

async function lightsPayload(devices) {
  const statuses = await allStatuses();
  const byId = Object.fromEntries(statuses.map((entry) => [entry.id, entry]));
  const payload = {};
  for (const device of devices) {
    const on = device.level ? byId[device.bathroom]?.state === device.level : false;
    payload[device.id] = lightJson(device, on);
  }
  return payload;
}

function start({ port = Number(process.env.ALEXA_PORT || 80), log = console.log } = {}) {
  const devices = buildDevices();
  const address = lanAddress();

  const server = http.createServer(async (req, res) => {
    const send = (body) => { res.writeHead(200, { 'Content-Type': 'application/json' }); res.end(JSON.stringify(body)); };
    try {
      const url = new URL(req.url, `http://${req.headers.host || address}`);
      const path = url.pathname;

      if (path === '/description.xml') {
        res.writeHead(200, { 'Content-Type': 'application/xml' });
        return res.end(`<?xml version="1.0" encoding="UTF-8" ?>
<root xmlns="urn:schemas-upnp-org:device-1-0">
<specVersion><major>1</major><minor>0</minor></specVersion>
<URLBase>http://${address}:${port}/</URLBase>
<device>
<deviceType>urn:schemas-upnp-org:device:Basic:1</deviceType>
<friendlyName>TP Refresher (${address}:${port})</friendlyName>
<manufacturer>Royal Philips Electronics</manufacturer>
<modelName>Philips hue bridge 2015</modelName>
<modelNumber>BSB002</modelNumber>
<serialNumber>${BRIDGE_ID}</serialNumber>
<UDN>uuid:${BRIDGE_UUID}</UDN>
</device>
</root>`);
      }

      // Alexa "pairs" by posting for a username. Any caller is accepted; this only
      // ever listens on the LAN and controls toilet paper flags.
      if (path === '/api' && req.method === 'POST') return send([{ success: { username: 'tprefresher' } }]);

      const lightMatch = path.match(/^\/api\/[^/]+\/lights\/(\d+)(\/state)?$/);
      if (lightMatch && req.method === 'PUT') {
        const device = devices.find((entry) => entry.id === lightMatch[1]);
        if (!device) return send([{ error: { type: 3, description: 'not found' } }]);
        const body = await new Promise((resolve) => {
          let raw = ''; req.on('data', (chunk) => { raw += chunk; });
          req.on('end', () => { try { resolve(JSON.parse(raw || '{}')); } catch (_e) { resolve({}); } });
        });
        if (typeof body.on === 'boolean') {
          if (!device.level) { if (body.on) await update(device.bathroom, null); }
          else if (body.on) await update(device.bathroom, new Date().toISOString(), device.level);
          else await update(device.bathroom, null);
          log(`alexa: ${device.name} -> ${body.on ? 'on' : 'off'}`);
        }
        return send([{ success: { [`/lights/${device.id}/state/on`]: !!body.on } }]);
      }
      if (lightMatch) {
        const device = devices.find((entry) => entry.id === lightMatch[1]);
        const payload = await lightsPayload(devices);
        return send(device ? payload[device.id] : {});
      }
      if (/^\/api\/[^/]+\/lights$/.test(path)) return send(await lightsPayload(devices));
      if (/^\/api\/[^/]+\/?$/.test(path)) {
        return send({ lights: await lightsPayload(devices), config: { name: 'TP Refresher', bridgeid: BRIDGE_ID, modelid: 'BSB002', mac: '00:17:88:0b:0b:0b' } });
      }
      res.writeHead(404); return res.end();
    } catch (error) { log('alexa error', error); res.writeHead(500); res.end(); }
  });

  server.on('error', (error) => {
    if (error.code === 'EACCES') log(`alexa: cannot bind port ${port}. Echo devices require port 80, which needs elevated privileges. See CLAUDE.md.`);
    else if (error.code === 'EADDRINUSE') log(`alexa: port ${port} already in use, emulation disabled.`);
    else log('alexa: emulation disabled,', error.message);
  });
  server.listen(port, () => log(`alexa: Hue emulation on http://${address}:${port} with ${devices.length} switches`));

  const ssdp = dgram.createSocket({ type: 'udp4', reuseAddr: true });
  ssdp.on('error', (error) => log('alexa: discovery disabled,', error.message));
  ssdp.on('message', (message, remote) => {
    const text = message.toString();
    if (!text.startsWith('M-SEARCH')) return;
    if (!/ST: *(ssdp:all|upnp:rootdevice|urn:schemas-upnp-org:device:basic:1)/i.test(text)) return;
    const reply = Buffer.from([
      'HTTP/1.1 200 OK', 'HOST: 239.255.255.250:1900', 'EXT:', 'CACHE-CONTROL: max-age=100',
      `LOCATION: http://${address}:${port}/description.xml`,
      'SERVER: Linux/3.14.0 UPnP/1.0 IpBridge/1.17.0', `hue-bridgeid: ${BRIDGE_ID}`,
      'ST: urn:schemas-upnp-org:device:basic:1', `USN: uuid:${BRIDGE_UUID}`, '', ''
    ].join('\r\n'));
    ssdp.send(reply, remote.port, remote.address);
  });
  ssdp.bind(SSDP_PORT, () => {
    try { ssdp.addMembership(SSDP_ADDRESS); log('alexa: listening for discovery on 1900'); }
    catch (error) { log('alexa: could not join discovery group,', error.message); }
  });

  return { server, ssdp, devices };
}

module.exports = { start, buildDevices };
