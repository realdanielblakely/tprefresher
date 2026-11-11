#!/bin/sh
# Copy TP Refresher (backend, phone page, Tidbyt app) to the M710q add-on.
#   ./ha-addon/deploy.sh           code only
#   ./ha-addon/deploy.sh restart   also restart the add-on (backend changes need it)
set -e
cd "$(dirname "$0")/.."
SSH="ssh -i $HOME/.ssh/homeassistant_ed25519 -o IdentitiesOnly=yes -p 22222"
HOST=root@192.168.1.101
$SSH $HOST 'mkdir -p /share/tprefresher/app /share/tprefresher/tidbyt/tprefresher /share/tprefresher/data /local_apps/tprefresher'
rsync -a --delete -e "$SSH" --exclude data.json --exclude laundry.json backend public node_modules package.json "$HOST:/share/tprefresher/app/"
TIDBYT="$HOME/Documents/ClaudeCode/Tidbyt/tprefresher"
rsync -a -e "$SSH" "$TIDBYT/tp_data.py" "$TIDBYT/tprefresher.star" "$TIDBYT/update.sh" "$HOST:/share/tprefresher/tidbyt/tprefresher/"
rsync -a -e "$SSH" ha-addon/config.yaml ha-addon/Dockerfile ha-addon/run.sh "$HOST:/local_apps/tprefresher/"
if [ "$1" = restart ]; then $SSH $HOST 'ha apps restart local_tprefresher'; fi
echo deployed
