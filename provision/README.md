# Rebuilding a Two Forty Pi

The laptop repository is the source of truth. Nothing configured interactively
on the Pi is required to recreate it.

## 1. Flash and prepare the card

Flash **Raspberry Pi OS Lite (64-bit), Trixie** with Raspberry Pi Imager. Apply
OS customisation so the boot partition contains `user-data` and
`network-config`, then run from the repository root:

```powershell
py -3 provision/configure_card.py --drive E: --ssh-key $env:USERPROFILE\.ssh\id_ed25519.pub
```

This writes only first-boot concerns:

- hostname `twoforty`
- user `retro`, required hardware groups, passwordless administrative access
- key-based SSH
- static Ethernet `192.168.137.2/24`
- the tested RGBerry 320×240p60 DPI timings and GPIO18/19 audio mapping

It does not copy or compile the game software. Eject the card and boot the Pi.

## 2. Install the software

Once SSH responds:

```powershell
.\provision\install.ps1 -IdentityFile $env:USERPROFILE\.ssh\id_ed25519
```

The installer copies the reproducible source/assets, validates Trixie, applies
the same OS configuration idempotently, installs required Debian packages,
builds the host and every game module, enables `two-forty.service`, and reboots.

On a Pi that already has all packages and no internet route:

```powershell
.\provision\install.ps1 -IdentityFile $env:USERPROFILE\.ssh\id_ed25519 -Offline
```

Use `-NoReboot` while diagnosing an install, `-SkipNetwork` to preserve an
existing network profile, and `-BootGame phosphor-run` to boot directly into
Phosphor Run instead of the default launcher.

## 3. Autonomous operation

No laptop or dashboard is needed after installation. Systemd starts
`two-forty-host` during normal multi-user boot and restarts it after a failure.
`config/host.conf` chooses the startup destination:

```ini
boot_game=launcher
```

The dashboard's **Boot directly into** selector edits this same file. F1 or
Escape returns from a running game to the launcher. The launcher can start
another game or power the Pi down safely.

## Updating later

The dashboard's **Deploy + build** action remains the fastest development
update. Without the dashboard, rerun `provision/install.ps1`; provisioning is
idempotent, and existing boot and network settings are reconciled rather than
stacked repeatedly.

## Validation and recovery

On the Pi:

```sh
cd /home/retro/two-forty
sudo sh provision/provision-pi.sh --check --boot-game launcher
systemctl status two-forty.service
```

The first provisioning run preserves the original firmware configuration as
`/boot/firmware/config.txt.pre-two-forty`.
