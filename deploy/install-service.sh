#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sudo install -m 0644 "$project_root/deploy/two-forty.service" /etc/systemd/system/two-forty.service
sudo systemctl daemon-reload
sudo systemctl enable --now two-forty.service
sudo systemctl --no-pager --full status two-forty.service
