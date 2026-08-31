#!/bin/sh
set -eu

two_forty_user=retro
two_forty_offline=0
two_forty_start=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        --user) two_forty_user=$2; shift 2 ;;
        --offline) two_forty_offline=1; shift ;;
        --start) two_forty_start=1; shift ;;
        -h|--help)
            echo "Usage: sudo sh provision/install-two-forty.sh [--user NAME] [--offline] [--start]"
            exit 0 ;;
        *) echo "Unknown option: $1" >&2; exit 2 ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this script with sudo." >&2
    exit 1
fi
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if ! id "$two_forty_user" >/dev/null 2>&1; then
    echo "User does not exist: $two_forty_user" >&2
    exit 1
fi

packages="build-essential libdrm2 libgbm1 libegl1 libgles2 alsa-utils network-manager openssh-server"
if [ "$two_forty_offline" -eq 0 ]; then
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $packages
else
    for command_name in cc make aplay nmcli ssh; do
        command -v "$command_name" >/dev/null 2>&1 || {
            echo "Offline install is missing required command: $command_name" >&2
            exit 1
        }
    done
fi

for group in audio video render input; do
    getent group "$group" >/dev/null 2>&1 && usermod -aG "$group" "$two_forty_user"
done
chown -R "$two_forty_user:$two_forty_user" "$project_root"

runuser -u "$two_forty_user" -- make -C "$project_root" clean all
install -d -o "$two_forty_user" -g "$two_forty_user" "$project_root/run" "$project_root/snapshots"

service_temp=$(mktemp)
sed -e "s/^User=.*/User=$two_forty_user/" -e "s/^Group=.*/Group=$two_forty_user/" \
    -e "s|/home/retro/two-forty|$project_root|g" \
    "$project_root/deploy/two-forty.service" > "$service_temp"
install -m 0644 "$service_temp" /etc/systemd/system/two-forty.service
rm -f "$service_temp"
systemctl daemon-reload
systemctl enable two-forty.service >/dev/null
if [ "$two_forty_start" -eq 1 ]; then
    systemctl restart two-forty.service
fi

echo "Two Forty installed from $project_root."
echo "Service enabled: $(systemctl is-enabled two-forty.service)"
if [ "$two_forty_start" -eq 1 ]; then
    echo "Service state: $(systemctl is-active two-forty.service)"
else
    echo "The service will start on the next boot."
fi
