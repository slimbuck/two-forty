#!/bin/sh
set -eu

two_forty_user=retro
two_forty_hostname=twoforty
two_forty_address=192.168.137.2/24
two_forty_boot_game=launcher
two_forty_check_only=0
two_forty_skip_network=0

usage() {
    cat <<'EOF'
Usage: sudo sh provision/provision-pi.sh [options]

  --user NAME          Runtime/login user (default: retro)
  --hostname NAME      Pi hostname (default: twoforty)
  --address CIDR       Static Ethernet address (default: 192.168.137.2/24)
  --boot-game ID       Game loaded after boot (default: launcher)
  --skip-network       Do not alter NetworkManager settings
  --check              Validate the current machine without changing it
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --user) two_forty_user=$2; shift 2 ;;
        --hostname) two_forty_hostname=$2; shift 2 ;;
        --address) two_forty_address=$2; shift 2 ;;
        --boot-game) two_forty_boot_game=$2; shift 2 ;;
        --skip-network) two_forty_skip_network=1; shift ;;
        --check) two_forty_check_only=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "Run this script with sudo." >&2
    exit 1
fi
case "$two_forty_hostname" in
    ''|*[!A-Za-z0-9-]*|-*|*-)
        echo "Invalid hostname: $two_forty_hostname" >&2
        exit 2 ;;
esac

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
boot_config=/boot/firmware/config.txt
rgb_config="$project_root/provision/rgbberry-config.txt"

validate() {
    failures=0
    check_line() {
        if ! grep -Fqx "$1" "$boot_config"; then
            echo "MISSING: $1" >&2
            failures=$((failures + 1))
        fi
    }
    check_line "dtoverlay=audremap,pins_18_19"
    check_line "dtoverlay=vc4-kms-dpi-generic,hactive=320,hfp=16"
    check_line "dtparam=hsync=32,hbp=39"
    check_line "dtparam=vactive=240,vfp=3,vsync=3,vbp=16"
    check_line "dtparam=clock-frequency=6400000,rgb666-padhi"
    check_line "dtparam=hsync-invert,vsync-invert"
    if ! id "$two_forty_user" >/dev/null 2>&1; then
        echo "MISSING: user $two_forty_user" >&2
        failures=$((failures + 1))
    else
        for group in audio video render input; do
            if ! id -nG "$two_forty_user" | tr ' ' '\n' | grep -qx "$group"; then
                echo "MISSING: $two_forty_user membership in $group" >&2
                failures=$((failures + 1))
            fi
        done
    fi
    if ! systemctl is-enabled ssh >/dev/null 2>&1; then
        echo "MISSING: enabled SSH service" >&2
        failures=$((failures + 1))
    fi
    if [ "$(hostname)" != "$two_forty_hostname" ]; then
        echo "MISSING: hostname $two_forty_hostname" >&2
        failures=$((failures + 1))
    fi
    if ! grep -Eq "^[[:space:]]*127\\.0\\.1\\.1[[:space:]]+$two_forty_hostname([[:space:]]|$)" /etc/hosts; then
        echo "MISSING: /etc/hosts entry for $two_forty_hostname" >&2
        failures=$((failures + 1))
    fi
    if [ -f "$project_root/config/host.conf" ]; then
        if ! grep -Fqx "boot_game=$two_forty_boot_game" "$project_root/config/host.conf"; then
            echo "MISSING: boot_game=$two_forty_boot_game" >&2
            failures=$((failures + 1))
        fi
    fi
    if [ "$failures" -ne 0 ]; then
        echo "Two Forty provisioning check failed ($failures issue(s))." >&2
        return 1
    fi
    echo "Two Forty OS configuration is valid."
}

if [ "$two_forty_check_only" -eq 1 ]; then
    validate
    exit $?
fi

if [ ! -f "$boot_config" ] || [ ! -f "$rgb_config" ]; then
    echo "Expected Raspberry Pi boot config or RGBerry configuration is missing." >&2
    exit 1
fi
if [ ! -r /etc/os-release ] || ! grep -Eq '^VERSION_CODENAME=trixie$' /etc/os-release; then
    echo "This provisioner targets Raspberry Pi OS/Debian Trixie." >&2
    exit 1
fi
if ! id "$two_forty_user" >/dev/null 2>&1; then
    echo "User does not exist: $two_forty_user" >&2
    exit 1
fi

if [ ! -f "$boot_config.pre-two-forty" ]; then
    cp -p "$boot_config" "$boot_config.pre-two-forty"
    echo "Saved $boot_config.pre-two-forty"
fi

temporary=$(mktemp)
awk '
    /^# BEGIN TWO FORTY RGBERRY$/ { skip=1; next }
    /^# END TWO FORTY RGBERRY$/ { skip=0; next }
    !skip { print }
' "$boot_config" > "$temporary"
sed -i \
    -e '/^dtparam=audio=/d' \
    -e '/^camera_auto_detect=/d' \
    -e '/^display_auto_detect=/d' \
    -e '/^disable_fw_kms_setup=/d' \
    -e '/^disable_overscan=/d' \
    -e '/^dtoverlay=audremap,pins_18_19$/d' \
    -e '/^dtoverlay=vc4-kms-dpi-generic,hactive=320,hfp=16$/d' \
    -e '/^dtparam=hsync=32,hbp=39$/d' \
    -e '/^dtparam=vactive=240,vfp=3,vsync=3,vbp=16$/d' \
    -e '/^dtparam=clock-frequency=6400000,rgb666-padhi$/d' \
    -e '/^dtparam=hsync-invert,vsync-invert$/d' \
    "$temporary"
{
    cat "$temporary"
    printf '\n[all]\n'
    cat "$rgb_config"
} > "$boot_config"
rm -f "$temporary"

hostnamectl set-hostname "$two_forty_hostname"
if grep -Eq '^[[:space:]]*127\.0\.1\.1[[:space:]]' /etc/hosts; then
    sed -i "s/^[[:space:]]*127\\.0\\.1\\.1[[:space:]].*/127.0.1.1\t$two_forty_hostname/" /etc/hosts
else
    printf '127.0.1.1\t%s\n' "$two_forty_hostname" >> /etc/hosts
fi
for group in audio video render input; do
    getent group "$group" >/dev/null 2>&1 && usermod -aG "$group" "$two_forty_user"
done
systemctl enable ssh >/dev/null
systemctl set-default multi-user.target >/dev/null

mkdir -p "$project_root/config"
if [ -f "$project_root/config/host.conf" ]; then
    sed -i "s/^boot_game=.*/boot_game=$two_forty_boot_game/" "$project_root/config/host.conf"
else
    printf '# Loaded automatically when the Pi boots.\nboot_game=%s\n' "$two_forty_boot_game" > "$project_root/config/host.conf"
fi
chown -R "$two_forty_user:$two_forty_user" "$project_root/config"

if [ "$two_forty_skip_network" -eq 0 ]; then
    connection=$(nmcli -t -f NAME,TYPE connection show | awk -F: '$2=="802-3-ethernet" {print $1; exit}')
    if [ -z "$connection" ]; then
        connection=two-forty-ethernet
        nmcli connection add type ethernet ifname eth0 con-name "$connection" >/dev/null
    fi
    nmcli connection modify "$connection" \
        connection.autoconnect yes \
        ipv4.method manual ipv4.addresses "$two_forty_address" \
        ipv4.gateway "" ipv4.dns "" ipv6.method link-local
    echo "Ethernet will use $two_forty_address after reboot ($connection)."
fi

validate
echo "OS provisioning complete. Reboot is required for RGB/SCART timing changes."
