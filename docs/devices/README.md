# Devices

| Device | Doc | Role | State |
|---|---|---|---|
| OnePlus 8 Pro (instantnoodlep, SM8250) | [instantnoodlep.md](instantnoodlep.md) | **active development** (this repository's focus); iOS in a `darwin` partition next to Android | SpringBoard, GPU, Wi-Fi, Bluetooth, back + front + telephoto cameras, calls, mobile data in apps, LTE + SMS, sensors, location, installed boot (see doc) |
| OnePlus 8 (instantnoodle, SM8250) | [instantnoodle.md](instantnoodle.md) | worked example for the Pro; iOS root on its own flash, no Android | SpringBoard, Wi-Fi, audio, sleep, installed boot work (see doc) |
| OnePlus 8T (kebab, SM8250) | [kebab.md](kebab.md) | port in progress; flat 1080x2400 120 Hz amb655x panel | boots iOS 16.5 from RAM to SpringBoard, touch (Synaptics S3908/TCM) works, Adreno 650 starts; software rendering until Mesa |
| POCO X5 Pro 5G (redwood, SM7325) | [redwood.md](redwood.md) | where the port was born; owner asked not to run redwood checks | works per the project; not re-verified recently |
| QEMU `virt` | [qemu.md](qemu.md) | no phone needed; CI-like scenarios | works |

Profiles: `common/devices/{instantnoodlep,instantnoodle,kebab,redwood,qemu-virt}.conf` (+ `_template.conf`, documented). `common/hackios devices` lists them.
A kernel binary serves every phone with the same SoC blocks: addresses default to redwood's and `board=` / `board_<block>=` select others
([../architecture.md](../architecture.md#device-specific-code)). Porting a new phone: [../PORTING.md](../PORTING.md).
Cross-device feature matrix:

| Feature | redwood | instantnoodle | instantnoodlep | kebab | QEMU |
|---|---|---|---|---|---|
| CPU cores / governor | 8 / yes | 8 / yes | 8 / yes (`cpufreq=1`) | 8 / yes | up to 8 / no |
| display | 120 Hz video panel | 90 Hz command panel, MDP push | 120 Hz command panel, DSC, MDP push | 1080x2400 120 Hz amb655x command/DSC, Iris properties in DT | ramfb |
| GPU | Adreno 642L, GL only | Adreno 650, GL + experimental Metal | Adreno 650, GL | Adreno 650, kernel bring-up verified (`gpu=1`); GL needs Mesa | none |
| touch | Goodix/FocalTech, polled | S6SY761, interrupt | S6SY791 | Synaptics S3908 (TCM, `kebab_tcm.c`) | virtio |
| Wi-Fi | WCN6750 | QCA6390 PCIe | QCA6390 PCIe | QCA6390 PCIe (expected) | virtio-net |
| speakers / mic | AW88261 / analog | TFA9874 / digital | yes (`sound=4`) | expected (same SoC) | virtio-sound |
| haptics / torch | yes / yes | yes / yes | yes / yes | expected (same SoC) | no |
| cameras | yes | no | back + front + telephoto | expected IMX586, off for now | no |
| system sleep | no | yes | off by default (K45) | off by default | test only |
| installed boot | dual boot (Android kept) | standalone | dual boot (Android on slot B) | not attempted | scenario tests |
| battery | DSP battery manager | bq27541 gauge | see [power.md](../subsystems/power.md) | gauge expected, unverified | none |
