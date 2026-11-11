#!/bin/sh
# Code: /share/tprefresher/app (backend, public, node_modules) and /share/tprefresher/tidbyt,
# copied from the Mac by ha-addon/deploy.sh. State: /share/tprefresher/data.
node -e '
const o = require("/data/options.json"), fs = require("fs");
const q = (v) => "\x27" + String(v).replace(/\x27/g, "\x27\\\x27\x27") + "\x27";
fs.writeFileSync("/data/env", [
  `export SMTP_HOST=${q(o.smtp_host)} SMTP_PORT=${q(o.smtp_port)} SMTP_USER=${q(o.smtp_user)} SMTP_PASS=${q(o.smtp_pass)}`,
  `export MAIL_FROM=${q(o.mail_from)} MAIL_TO=${q(o.mail_to)}`, ""].join("\n"), { mode: 0o600 });
fs.writeFileSync("/data/tidbyt.env", `TIDBYT_API_KEY=${q(o.tidbyt_api_key)}\nTIDBYT_DEVICE_ID=${q(o.tidbyt_device_id)}\n`, { mode: 0o600 });
'
. /data/env
mkdir -p /share/tprefresher/data
export DATA_FILE=/share/tprefresher/data/data.json LAUNDRY_FILE=/share/tprefresher/data/laundry.json
# The Alexa (fake Hue) piece needs port 80, which Home Assistant uses here; it stays on Hermes.
export ALEXA=off PORT=3080

# Tidbyt: every 5 minutes, like the old launchd job on Hermes
(
  sleep 20
  while true; do
    if grep -q "TIDBYT_API_KEY='.\+'" /data/tidbyt.env; then
      TIDBYT_ENV=/data/tidbyt.env TP_BASE_URL=http://127.0.0.1:3080 \
        /bin/bash /share/tprefresher/tidbyt/tprefresher/update.sh 2>&1 | sed 's/^/tidbyt: /'
    fi
    sleep 300
  done
) &

cd /share/tprefresher/app || exit 1
while true; do
  node backend/server.js
  echo "server exited ($?), restarting in 5 s"
  sleep 5
done
