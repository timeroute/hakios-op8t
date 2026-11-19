# Bluetooth (OnePlus 8 Pro)

## OnePlus 8T (kebab), 2026-11-19 (kernel RAM tests through `bt=6`)

The 8T's Bluetooth is the **same chip and wiring**: QCA6390, the same UART (`qup_uart@998000`,
serial engine 6 of QUPv3 wrapper 0, GPIO 16-19), `BT_EN` GPIO 21 and sw-ctrl GPIO 124. Its rails are
the 8 Pro's except that the device tree takes **both the aon and the dig pin from `smpa6`** (as its
Wi-Fi does), so a vote for `smpf2` would be another board's rail; `smpa5` 1.9 V, `smpc8` 1.35 V and
`ldoa16` (pm8150 L16, 3.0 V, 10 mA) are the same names. `instantnoodle_bt.c` now takes `board=kebab`
and leaves `smpf2` out there. The phone's own firmware was read from the bluetooth partition in
TWRP and built into `instantnoodle_btfw.h`; do not use the 8 Pro's firmware.

**`bt=1` verified on the 8T (2026-11-19, read-only, no panic, `logs/170154-run.log`):** GCC wrapper 0's
master AHB clock running, engine 6's clock off, vote register `0x063c83c0` (the three bits 110); pins 16
(CTS), 17 (RFR), 18 (TX), 19 (RX), 20 (WLAN_EN), 21 (BT_EN) and 124 (sw-ctrl) all read as plain GPIO with
the engine off, and the look stops before touching the engine's registers, as it should. The phone's own
firmware was pulled from its bluetooth partition (`htbtfw20.tlv` 184,316 B, `htnv20.bin` 5,733 B — not the
8 Pro's sizes) and the kernel now carries it (`instantnoodle_btfw.h`, generated, 962 KB; the image with it
is 17,131,000 bytes).

**`bt=2` verified on the 8T (2026-11-19, no panic, `logs/170738-run.log`):** the rails went on as its device
tree says (`smpa6` for both aon and dig, `smpa5` 1.9 V, `smpc8` 1.35 V, `ldoa16` 3.3 V), `BT_EN` (pin 21)
was pulsed, the **sw-ctrl line read 1** (the controller is alive), the engine's clock and vote went to
`0x063d83c0` (111), pins 16-19 became the UART's, and the engine answered: revision `0x0a`, firmware
`0x00000203` (UART protocol 2, version 3); it was powered down cleanly again.

**`bt=3` verified on the 8T (2026-11-19, no panic, `logs/171245-run.log`):** HCI_Reset was sent at eight rates
and the controller answered at the driver's **"57600" entry, which is 115200 real** (the engine samples at 16x:
clock / divider / 16, the file's own note) — the same real rate as the 8 Pro. The engine's configuration read
back the same (revision `0x0a`, firmware `0x00000203`).

**`bt=4` verified on the 8T (2026-11-19, `logs/171903-run.log`, no panic):** the phone's own
184,316-byte patch downloaded in 759 segments and the 5,733-byte NVM in 24 segments, both with
zero anomalies. After the rate change to 3.2 Mbaud, HCI Reset, local version, and controller
address queries answered; patch version changed from `0x0d2b` to `0x250a` (not the 8 Pro's
`0x4509`). The address is not recorded. **Next:** `bt=5` transport and packet-device self-test;
this does not require the missing iOS Bluetooth userland.

**`bt=5` verified on the 8T (2026-11-19, `logs/173325-run.log`, no panic):** the
transport's 7/7 self-tests pass; 100/100 consecutive 71-byte events arrived whole,
106 HCI packets sent and received, 0 bad bytes, and `/dev/hk-bt` was created.
This proves kernel H4 packet transport, **not** iOS discovery/pairing. `bt=5` keeps the
controller and UART powered after the test thread exits; there is no `bt_stop` hook
in this driver. Reboot the phone before a new cold bring-up rather than assume the
prior run powered the chip down.

**`bt=6` verified on the 8T (2026-11-19, `logs/174332-run.log`, no panic):**
after a phone reboot and fresh resident-image upload, the transport again passed
7/7 tests and 100/100 consecutive long events, with 106 HCI packets each way and
zero bad bytes. `/dev/hk-bt` was created; kernel control
`com.apple.uart.bluetooth` registered successfully (status 0). The run returned
normally to the loader. **Not verified on this phone:** an iOS BlueTool connection,
discovery, pairing or Bluetooth audio. The current `userland/build.sh` has no
`BLUETOOTH=1` branch and the current root image has no BlueTool substitute.

**Status (owner-verified in RAM runs, 2026-10-27):** discovery (Classic and LE), pairing, and A2DP audio to a JBL Charge 2+ work with iOS's own `bluetoothd`.
Open: local name shows "(null)", a paired speaker's device type is blank, the Settings switch does not power the chip off, receive is polled, calls/sleep unverified
([../TODO.md](../TODO.md) T05, [../KNOWN_ISSUES.md](../KNOWN_ISSUES.md) K05). Background and the run-by-run record: `INSTANTNOODLEP-PLAN.md` addenda 36-44 (at the root of this repository).

## The hardware
One chip, the QCA6390: Wi-Fi on PCIe (`instantnoodle_wifi.c`), Bluetooth on a UART — serial engine 6 of QUPv3 wrapper 0 (`qcom,qup_uart@998000`, GPIO 16 CTS, 17 RFR, 18 TX,
19 RX, function 1), BT_EN GPIO 21, sw-ctrl GPIO 124 (an input the controller drives). Shared rails (RPMh): smpa6, smpf2, smpa5, smpc8, plus Bluetooth's own ldoa16. The two drivers vote
the same values and never turn the shared rails off.

## The driver: `xnu/files/bsd/dev/instantnoodle_bt.c` (a ladder of boot-args, see [../BOOT_ARGS.md](../BOOT_ARGS.md))
| `bt=` | What it does | Verified |
|---|---|---|
| 1 | reads the engine's clocks and pins | yes |
| 2 | powers the chip as Android does, reads the engine (its firmware is UART, FIFO mode) | yes |
| 3 | sets the engine up as a polled UART and sends HCI Reset and the ROME version request | yes: the controller answers |
| 4 | downloads the ROME patch (205836 bytes, 848 segments of 243) and NVM (5815 bytes, 24 segments) from the phone's own `htbtfw20.tlv`/`htnv20.bin` | yes: patch version 0x0d2b → 0x4509 |
| 5 | keeps the controller up as a packet device, `/dev/hk-bt` (H4 packets, one per read/write); a worker thread polls the UART | yes: 7/7 self-tests, 100 long events whole |
| 6 | registers the kernel control `com.apple.uart.bluetooth` (SOCK_STREAM) for bluetoothd; sysctl `kern.hackios.bt` (state), `/dev/btpoweroff` | yes |

Things learned the hard way (all in the file's header comment too):
- The engine's root generator is **not in DFS mode** on this boot chain: the driver programs GPLL0-even 300 MHz × M/N and the engine's divider itself, and puts the old values back. The engine **samples at 16x**: real baud = clock / divider / 16.
- The serial clock register must be enabled before any other engine write (a write with it at 0 took the phone to CrashDump).
- Firmware order (btqca.c): the patch, the NVM, **HCI_Reset at the download's rate (115200)**, only then the host to 3.2 Mbaud (tag 17 byte 1 = 0x11).
- After the firmware the controller uses Qualcomm in-band sleep: a 0xfd (wake-up) is answered with 0xfc; the host sends 0xfd before the first packet and never sends 0xfe.
- RX runs with the controller's flow control (no "RFR open"), TX honours CTS: a 1 ms poll cannot keep up with 3.2 Mbaud otherwise.
- The firmware is the phone's own and **never in a repository**: `docker/rebuild-bt.sh` turns `op8pro-data/bt-fw/*` into a generated header (`instantnoodle_btfw.h`, not committed) that `instantnoodle_bt.c` includes if present.

## iOS side (userland `BLUETOOTH=1`; the design is the upstream author's, commit c4379c2 of `iphont-xnu-userland`)
- **bluetoothd talks HCI itself.** `BlueTool` is only a boot helper: bluetoothd asks the Mach service `com.apple.BlueTool` to run `RunBuiltinScript` init/boot/deepsleep. `mkjobs.py` keeps
  `com.apple.BlueTool` denied and a job `com.hackios.bluetool` runs `src/btboot.c` under that service name (boot waits for `kern.hackios.bt` = "state up", up to 180 s).
- `mkjobs.py` sets bluetoothd's `BT_HCI_TRANSPORT=H4:POSIX:com.apple.uart.bluetooth,3000000`: bluetoothd connects `socket(PF_SYSTEM, SOCK_STREAM, SYSPROTO_CONTROL)` by name (the control **must** be registered with `CTL_FLAG_REG_SOCK_STREAM`).
- `src/inject_bt.c` (library inside bluetoothd) patches private function-pointer tables at fixed 20F66 addresses (generic vendor descriptor, no Broadcom VSCs, no multi-advertising, the unsupported init steps omitted) and logs bluetoothd's open/socket/connect/ioctl calls (`bt-watch:`).
- **Audio:** Apple's unmodified `BTAudioHALPlugin.driver` must be in the **system image's** `System/Library/Audio/Plug-Ins/HAL` (a RAM boot's audio server reads that one; `build.sh` puts it there for BLUETOOTH=1, and into the small image's `/hackios/hal` for the installed boot). Without it bluetoothd logs "unable to sendMsg due to no available xpc client connections" and the sound stays on the speaker.
- `init.c` sets the host name ("OnePlus 8 Pro"); it did **not** fix the "(null)" in Settings.

## Not possible / not done
- Receiving a photo from an Android phone's classic "Bluetooth share": iOS has no Object Push server. AirDrop needs the Wi-Fi side (untried).
- Other Apple audio plug-ins not shipped (AirPlay, USB audio, CarPlay, baseband voice, AOP): [../TODO.md](../TODO.md) notes; only Bluetooth was needed.

## Testing
`userland/rc.jobs.bt` + `src/bt_test.c` (opens `/dev/hk-bt` from iOS user space: Reset, version, an 8 s LE scan), `src/bt_scan.m` (CoreBluetooth discovery probe), the kernel's per-event counters (`bt: stats:` every 5 s) and L2CAP connection lines (`bt: acl`).
Run: `bt=6 bt_after=20` (or 45 with `wifi=1`), image built with `BLUETOOTH=1`, log snapshots every 20 s (the loader keeps only the first 256 KB and last 1 MB of the kernel log).
