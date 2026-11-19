# HANDOFF — start here when you begin a new session

Short, actionable, and dated. It does **not** replace [TODO.md](TODO.md) (roadmap), [KNOWN_ISSUES.md](KNOWN_ISSUES.md) (what is broken) or the subsystem pages;
it says where the work stands *today*. **Update this file after every substantial session** (rules in [../AGENTS.md](../AGENTS.md)); keep it under ~150 lines and move
anything that has aged into the right permanent page.

**Last updated: 2026-11-19** (section 0f, OnePlus 8T: GPU drawing, K25's text bars and Control Center black fixed, K26's keyboard fixed); before that 2026-11-17 (sections 0d, 0e: 8 Pro documentation sync; OnePlus 8T port scaffolding); before that 2026-11-13 (section 0c); before that 2026-11-07 (sections 0a, 0b); before that 2026-11-02 (section 0, OnePlus 8 Pro cellular); before that 2026-10-26 ~04:55 MSK by the scheduler session (sections 2, 3, 4, 7 for the scheduler; the rest as the documentation session left it at ~03:30). **Checked state** = what I ran/read on that date; **historical** = taken from earlier session notes and flagged as such.

## 0f. OnePlus 8T (kebab), 2026-11-18 — boots iOS 16.5, touch works, the GPU starts (RAM only)

- **iOS boots to SpringBoard** from RAM (`./xl enter`, `XL_RAMDISK=userland/root.apfs`, resident `userland/system.apfs` uploaded with `./xl ramdisk`, ~95 s). A `fastboot boot` or a trip through Android empties RAM: re-upload the resident image before the next run.
- **Touch works and the screen shows it.** New driver `xnu/files/bsd/dev/kebab_tcm.c` for the chip that actually answers, a **Synaptics S3908** (TouchComm/TCM) at I2C `0x4b`, reset/IRQ TLMM 38/39 — *not* the S6SY761 the live device tree names (that driver is now off unless `kebab_touch=1`). `logs/215439-run.log`: `1320 reports: 9 down, 1303 move, 8 up; 0 unreadable, 0 bad headers, 0 out of sync`, `tl: p` → `tl: i` in 50–70 µs, and `[TouchEvents] … <sceneID:com.apple.mobiletimer> … -> foreground` after the owner tapped Clock (Settings opened the same way). The owner's earlier "touch does nothing" was a run whose boot-args never reached the kernel (next line), not the chip.
- **Loader bug fixed (`xnuloader/loader.c`, uncommitted):** `with_defaults()` kept the merged command line for good, so the first parse — the USB payload that starts the loader, with no line at all — decided every later run's line and a run silently got the platform's `default_args` only. `rec_secs`, `touch_lat`, `loader_return_after`… were all lost. The cache is now keyed by the line it was made from; the kernel's `cmdline:` line shows the run's arguments again. Any measurement taken before this fix that relied on extra boot-args (screen recording, `touch_lat`) is void.
- **GPU on this phone: the kernel's whole bring-up works** (steps 1–21), with firmware pulled read-only from its `/vendor/firmware` (`a650_gmu.bin`, `a650_sqe.fw`, `a650_zap.mdt`+`.b00`–`.b02`, in `fw-gpu/`, untracked) into `root.apfs` as `fw/`. `adreno=3/9/12/17/21` pass — `CX is on`, `TRUSTZONE ACCEPTS THE ZAP SHADER'S SIGNATURE FROM THIS KERNEL`, `THE 3D CORE HAS POWER AND ANSWERS`, `THE 3D CORE WRITES OUR MEMORY ON COMMAND` — and `gpu=1` gives `THE GPU STAYS ON for the system to draw with: the service HKGPU` with SpringBoard up. `kebab.conf` now carries `gpu=1`.
- **GPU acceleration works (2026-11-19):** `tools/mesa/` came from shimoku74's port (Mesa 25.0.7 + our patches, same SHA256), `tools/mk-mesa.sh` built `libOSMesa.8.dylib`, `GL=1` put it and `libhakios.gl.dylib` in `root.apfs`, and with `gpu=1` the window server draws through it: `hakios-gl: 8 of 8 methods of EAGLContext are ours`, `OpenGL ES 3.2 Mesa 25.0.7; FD650`, `frame 8: 52 GL calls, 6 draws … 3374 us`, **50 frames a second** (10–24 with the software renderer). Also fixed: the kernel's service class is `HKGPU` while `inject.c`/`inject_sb.c`/`gl_test.c` and `tools/mesa/hakios-drm` looked for `HakiosGPU` only — all four now take either, without which no amount of Mesa would have loaded.
- **`tools/mesa/` is not in either repository**: it came from shimoku74/hakios-xnu-kernel (same Mesa 25.0.7, same SHA256, its
  `freedreno-darwin.patch`/`hakios-drm`/`air`), and the local `tools/mesa/hakios-drm/hakios.drm.c` carries the same two-name GPU
  lookup as `inject.c` (`HKGPU`, then `HakiosGPU`) -- **re-apply that one edit** whenever the directory is refreshed from elsewhere.
- **K25 fixed (2026-11-19, owner-verified): text bars *and* the black areas after Control Center.** The image table
  (`struct image images[256]`, `userland/src/eagl.c`) filled up, `image_new()` refused new surface-backed textures (225
  messages in one run), their `-texImageIOSurface:` was dropped and every draw of one sampled an *incomplete* texture --
  opaque, so a mask/text shader painted its own colour and a vibrant/blur layer painted black. Found by `gltrace_at` +
  `gltrace_place` + `gltrace_look` (the bars' draws named textures `0 x 0`). The table is 1024 now, with eviction when
  full (a texture that is gone first, else the one looked at longest ago); Settings > General > iPhone Storage and
  Settings after Control Center are both right. `glstill=0`, `glcopy=1`, the GPU (the software rasteriser showed the
  bars too) and the render size were all red herrings.
- **K26 fixed the same session: the search keyboard ran off the right edge.** iOS was told the panel's own
  1080 x 2400 = 360 x 800 pt, which is no iPhone's size, so iOS 16.5 had no keyboard layout and drew a wider one from
  the left edge. `platform_kebab.default_args` (and `kebab.conf`) now say **fbwidth=1290 fbheight=2796** (430 x 932 pt,
  the 8 Pro's since K43) and `fb_push=2` scales it to the panel; the home screen still draws 56-59 frames a second.
  **The path is now 200 bytes per row different from the panel's** -- anything that assumes iOS pixels == panel pixels
  on the 8T is wrong (the `xl screenshot` frames come out scaled by the loader, not cropped).
- **A rebuilt `xnuloader/loader.efi` does not reach a phone that is already in the loader**: `xl enter` returns at once
  when it finds one (`xl: loader is up`) and `xl run` sends only the kernel and its ramdisk — so a change to the
  platform's `default_args` is silently not there. Use **`./xl reload`** (it reboots to Android and enters with the new
  loader). **It goes through Android, so the resident `system.apfs` is gone afterwards** — `./xl ramdisk
  userland/system.apfs` (~95 s) before the next run, or the kernel panics at `Process 1 exec of /sbin/launchd failed,
  errno 2` (`logs/103651-run.log`). This cost a run on 2026-11-19, and it was lucky: with the old 1080 x 2400 defaults the window server was
  clean while the keyboard still ran off the edge, which separated K25 (the image table) from K26 (the display size) in
  one run (`logs/102356-run.log`). The kernel's `cmdline:` line is where a default that did not arrive shows.
- Panel note: after a run the panel keeps its last frame, so a loader-idle phone looks like a frozen iOS screen. That is not a hang.


## 0d. OnePlus 8 Pro, 2026-11-17 (documentation sync)

- **README.md updated (commit `2b188c9`):** the OnePlus 8 Pro working list now includes calls, mobile data in apps, sensors
  (rotation, compass, barometer), location, and front + back + telephoto cameras in the Camera app; "Not yet" narrowed to
  headphones/USB-C audio, fingerprint, fast charging (system sleep off by default, K45). The OnePlus 8 "Does not work yet"
  narrowed to sensors (compass, barometer), headphones/USB-C audio, fingerprint, App Store content and Apple ID sign-in, fast charging.
- **Propagated to:** [devices/instantnoodlep.md](devices/instantnoodlep.md), [devices/instantnoodle.md](devices/instantnoodle.md),
  [devices/README.md](devices/README.md), [KNOWN_ISSUES.md](KNOWN_ISSUES.md) (K01/K03/K06/K07/K12), [TODO.md](TODO.md) (T06/T14),
  [subsystems/gpu.md](subsystems/gpu.md) (Metal on by default since 2026-11-07).

## 0e. OnePlus 8T (kebab), 2026-11-17 — source scaffolding, nothing built or booted

- The attached phone is a **KB2000 / OnePlus 8T** (HydrogenOS 11 `KB2000_21_201110`, adb `b683318f`, slot `_a`), **unlocked** (`flash.locked=0`, `verifiedbootstate=orange`). Owner approved a **RAM-only audit**: `fastboot boot` of TWRP 3.7.0_11 (SHA-256 matched the published one), no partition written; phone was then returned to Android. From TWRP root we pulled the live `fdt`, `/proc` files, `getprop`, by-name map and ABL log to `../op8t-data/` (outside the repos). **Still nothing built or booted of HackiOS, no flash.**
- Added (uncommitted): `common/devices/{kebab.conf,kebab.platform.c}`, `xnuloader/loader.c` (`platform_kebab`, `PLAT_IS_KONA()`, payload `platform=` override), `xnuloader/mkpayload.py --platform`, `common/build.sh` + `xl` pass it, board=kebab in `instantnoodle.h`, `instantnoodle_i2c.c`, `MiniIOMobileFramebuffer.cpp` (no 8-style 90 Hz switch), `instantnoodle_touch.c` (off unless `kebab_touch=1`), `hk_{buttons,power,cpufreq}.c`, `uefi/apply.sh` + `uefi/files/Binaries/kebab/README.md`, docs (`devices/kebab.md`, device index, BOOT_ARGS, TODO T50).
- The phone's own DTB/ABL (evidence in `docs/devices/kebab.md`): Samsung **amb655x** 1080x2400 120 Hz command/DSC panel with Iris properties (so "no Iris" was wrong), **S6SY761/SY771** touch at 0x48 on TLMM 38/39 (the 8's, so touch is on by default), GPU ZAP `0x8c51a000` and gauge bq27541 (the 8's), all `board_<block>=` addresses equal the kernel defaults (BOARD_ARGS empty). fb base/height equal the 8's, hence the explicit payload platform entry. Still unverified: firmware/DXEs, push path, 120 Hz switch, every peripheral after CPU+framebuffer+touch; T50. Next: supervised RAM-only first boot; do not flash.
- **Built and RAM-booted (same session):** kernel 16 MB (`xnu-build.sh -j8 -k BUILD_DSYM=0`; Ubuntu awk needed a `makesyscalls.sh` fix, now done in `xnu/apply.sh`, plus the +55→+57 hunk-header fix in the patch), loader 104 KB, kebab UEFI FD 3 MB (Mu-Silicium cloned/built; needed system nasm/lld/llvm symlinks), `out/kebab-boot.img` 1.9 MB. First `fastboot boot` (nothing flashed): loader prints `platform kebab`, 1080x2400 at 0x9c000000, USB started; kernel reaches `MiniIOMobileFramebuffer: display 1080x2400 ...` with 7 cores up, then panics only on `Failed to mount root device` because the test used `XL_RAMDISK=none`. Phone returned to Android 11. No flash.
- Host build recipe and fixes: [devices/kebab.md](devices/kebab.md). The userland repo's upstream `main` is empty (moved into the main HakiOS repo), so building full iOS images needs those scripts brought over; an iOS 16.5 IPSW is in `~/Downloads`. No commit made; do not reset other sessions' work.

## 0c. OnePlus 8 Pro, 2026-11-13 (RAM images 374-396, kernels `data103`-`data113`; nothing flashed)

- **Charging (verified on the phone, c20a282):** it charges (3 A in, ~2 A into the cell) under hard limits: a 95% cap (resumes at 90),
  4.40 V, 5-45 C, 10 h, the gauge silent = off, fv 4.35 V / fcc 2 A, and the PMIC's own bark-bite watchdog, so a hung kernel stops
  charging. Do not weaken these (owner's order). `charge_limit=N`, `charge_wd_test=1/2` (tests). [subsystems/power.md](subsystems/power.md).
- **Weather (verified, userland 216a810):** the app and the widget show Open-Meteo's weather, answered on the phone
  (`userland/src/weather_standin.m`; Apple's service needs FairPlay). [research/ios-userland.md](research/ios-userland.md) §4a.
- **Compass: not working.** SEE registers no magnetometer; the MEMSIC/AKM drivers' WHO-AM-I read on the DSP's I2C bus 1 fails, and
  the DSP's log is not reachable (no diag over GLINK/QRTR, no adspmsgd in the sensors process). Everything tried:
  [subsystems/input-peripherals.md](subsystems/input-peripherals.md).
- **App Store / Apple ID:** the same FairPlay wall (T15/T48); not attempted further. **AirDrop:** needs AWDL (TODO), not started.
- The scratch boot script's default kernel is `data113` (all of the above).

## 0b. OnePlus 8 Pro, 2026-11-07 later (RAM images 175-185, kernel `data75`, loader 15; nothing flashed, boot_a still image 145)

- **Verified:** Camera.app opens, switches back/front six times and closes with the sensor DSP on (two bugs fixed: mediaserverd's filter
  thumbnails, the camera's io_map leak that panicked the kernel); the front camera's colour tuning; orientation (face up) from the
  accelerometer; `slpi=1` by default (loader 15). Loader 14+ gives the whole returned log and the panic text ("== PANIC LOG ==").
- **Off by default, no longer crashing (RAM images 187, 188):** `als=1` (CoreBrightness's True Tone check guarded; lux arrives; not yet
  seen following the light) and `prox=1` ("Placement" = 1 for backboardd's sensor controller; SEE has sent no near/far event yet).
- **Not seen (needs a hand):** the screen turning, tap-to-focus (back camera), proximity. Compass: SEE never registers a mag; the DSP's own
  log (`slpi_dsplog=1`) does not come yet (only the root process got init2; no lines).
- **After a kernel return:** `fastboot getvar result` then `fastboot get_staged` before anything else (never `xl screenshot`/`oem log` first).
- **Flash candidate:** `out/instantnoodlep-standalone189.img` (kernel data75, loader 15, `out/tree/r42` of 10:00; sha256 2d929353810cfd21...),
  RAM-booted as is and checked: Camera back/front (natural colours), Maps, Photos, orientation. Flashing boot_a needs the owner's OK (rule 1).

## 0a. OnePlus 8 Pro, 2026-11-06/07: Metal for the whole system, GPS fields, the sensor DSP started on

- **boot_a holds image 145** (flashed by the owner; `metal=2 metal_profile=1 applog=1` on its line). Everything below was RAM runs (images 134-160), nothing else flashed.
- **Metal on by default** (loader `out/loader-8pro-uefimenu12.efi`: `metal=2 metal_profile=1` in `default_args`; `metal=0` opts out). Verified on the phone: Maps (whole map,
  right colours, no flicker), Photos (opens, thumbnails, full photo), every app on both home pages without a crash (not Phone, not Camera), Freeform's PencilKit.
  The fixes and how they were found: [subsystems/gpu.md](subsystems/gpu.md) "2026-11-07"; KNOWN_ISSUES K01. Kernel `data53`+ (kHKSurface per plane, `kern.hackios.fb_layers`).
- **The Mesa library** (`libhackiosair.dylib`) is built in the `hk-ws` volume with `CPATH` pointing at `tools/mesa/ios-include` (an `endian.h` the iOS SDK lacks): copy
  `tools/mesa/air/*` into the volume, `AIR=1 bash tools/mk-mesa.sh`, then into `out/tree/r42/lib/` and `codesign -f -s -`. The host frontend `air2nir` is in the volume at
  `build/mesa-host/build/src/compiler/air/air2nir` (prints any dumped shader, `metal_dump=NAME`).
- **GPS (hk_location.m):** QMI LOC TLVs fixed (speed 0x18, vertical accuracy 0x1c; 0x19/0x1d were the speed's uncertainty and the vertical confidence), the test file
  `hk_location_on` removed. Built and in `out/tree/r42/bin`; not yet seen against a fix (the phone was indoors).
- **Sensors (T47), working on the DSP side (verified, RAM image 174, kernel `data69`):** `hk_slpi.c` + `hk_fastrpc.c` (both in `xnu/hackios.patch`'s
  `conf/files` hunk), `slpi=1`. SEE reports accel, gyro, light, proximity and the derived sensors; the magnetometer now registers too. Firmware from the modem
  partition at boot (ADSP too: audio verified that way, `adsp.*` no longer in `out/tree/r42/fw`). Sensor registry/configs under
  `out/tree/r42/sensors/` (copied from the phone's persist/vendor/odm, untracked `fw-sensors/`). How it works: subsystems/input-peripherals.md
  "Sensors". Next: feed iOS (HID services), the magnetometer, `slpi=1` default. TODO T47.
- **Uncommitted:** all of the above (both repositories). The phone is on RAM images; a reboot gives back image 145.

## 0. OnePlus 8 Pro, 2026-10-28 (this session; the rest of the file is older)

- **Flashed on boot_a: `out/instantnoodlep-standalone30.img`** (owner-approved; kernel `ufs40`, root `userland/root-sb12.apfs` = image 29's root with the new `libhackios-ui`; display **1290 x 2796**, 120 Hz). It fixed the "123" key (K43: iOS 16.5's keyboard has no digits plane at 440 pt; the size was the cause) and moved the status bar. Android is on slot B and boots; the "Reboot to Android" menu entry is gone (see the A/B incident in memory).
- **Repacking a root image:** extracting with `7z` turns the absolute symlinks (`hal/HKAudio.driver`, `ext/HKMetal.bundle`) into links to the scratch folder, and then iOS has no sound or haptics (found the hard way). Re-link after extracting and compare the listing with the original (K43 entry).
- **Cellular (T06), 2026-10-29, RAM runs only (nothing in flashed image 30):** the modem works from our kernel up to registration and an accepted SMS. Test line: `modem=5 modem_quiet=3 modem_chans=139 modem_bringup=1 modem_tftp=1 modem_blk=1024 modem_efs=1 modem_service=1 modem_qrtr=SECS [modem_sms=1: one test SMS to the owner]` (scratchpad `mrun.sh`; kernel tags `modem83..86`; userland `libhackios-sb.dylib` with the ported status-bar hook in `userland/root-sb13.apfs`). Observed: QMI, PIN1 0000, LTE registration ~17 s after power-up, network name and bars; `kern.hackios.cellular_*` / `modem_*` sysctls (state, bars, SMS send by writing `NUMBER TEXT` to `modem_sms_send`, last received SMS in `modem_sms_last`) exist but the userland side of SMS (a tool/app) and the SMS receive path are untested; SpringBoard logged `3 bars shown` and the network name, but `iphont screenshot` only returns the boot logo so the bar was NOT seen. The recipient of the test SMS has not confirmed it. **FLASHED on boot_a (2026-10-29, LATEST: `out/instantnoodlep-standalone37.img` (sha256 71148ff9...): kernel `modem105` (modem service, RTC clock, COUNTRY_CODE/383/365/419 answered, live Apple80211 and scan logging, 40 ms active-scan dwell in the Wi-Fi driver, sleep off by default), root `userland/root-sb41.apfs` = sb27 + `hk_sysctl`, `hk_buddy`, `hk_logcfg`; booted from RAM and after the flash: registered on LTE, bars, date restored, no SpringBoard crash. Before it: `standalone35` (kernel modem101, root-sb27); image 32 crash-looped SpringBoard because of the status-bar hook and was replaced by 33, 34, 35).** What 35 adds: the date kept across restarts (`hk_time` daemon + `kern.hackios.rtc_seconds`; the PMIC's RTC counts but does not hold the date), the signal bars lit (mode 1), the modem started again after a link loss, COUNTRY_CODE answered. Not fixed: Wi-Fi auto-join (K44), sleep (K45), icon alpha (K46), calls/VoLTE/VoWiFi, mobile data, tethering, AirDrop, GPS and the other sensors. Earlier line: **image 32** (kernel modem87, root `userland/root-sb15.apfs` = sb13 + `bin/hk_cell`, the modem args above with modem_service=1): after the flash the phone registered on its own (log: `THE MODEM IS REGISTERED`, `3 bars shown`). Tested from RAM with a job list (`hk_cell`, root-sb14): state sysctls right, **an SMS written to `kern.hackios.modem_sms_send` from iOS user space was accepted by the modem**; the owner confirmed the first (kernel-sent) SMS arrived. System sleep does not happen in this configuration with or without the modem (`power_test sleep`: NOT ASLEEP both times, a separate issue), so sleep with the modem running is untested. Use `hk_cell status|watch N|sms NUMBER TEXT` (needs a way to run a command). Open: calls, data (IPA/MHI hw channels), writing the modem's EFS back, sleep with the modem running (untested), flashing. Everything that mattered is in [research/modem-sdx55.md](research/modem-sdx55.md) (last section) and TODO T06.
- **2026-10-29 (later): MOBILE DATA AND SMS RECEIVE WORK; flashed `out/instantnoodlep-standalone42.img` on boot_a** (sha256 a48b6f61...; kernel `data14`, root `userland/root-cell20.apfs` = + the SMS notification in `libhackios-sb`, see research/modem-sdx55.md "SMS into iOS"; before it image 40: kernel `data11`, root `userland/root-cell13.apfs` = sb41 + `net_test cell` step in com.hackios.wifi + `hk_smsd` daemon; args as image 37 plus `modem_chans=2187 modem_data=1`; earlier 38/39 had the SMS-receive bug). Verified: en1 appears about 30 s after boot, IPConfiguration DHCPs it, DNS/TCP/HTTPS work over LTE (RAM runs, `net_test fetch`), a 5-minute fetch loop ran through the modem's EFS sync, the phone texted its own number (`modem_sms_self=1 modem_sms_num=...`, RAM run), received it, acknowledged it and `hk_smsd` stored it in sms.db with the right text. Details: research/modem-sdx55.md ("Mobile data over IP_HW0", "SMS receive"), TODO T06, K47. New code: `instantnoodle_modem.c` (data section, en1 link, SMS route/ack), `hk_net.c` multi-unit (en0 + en1), `userland/src/hk_smsd.c`, `net_test.m` `cell`, `com.hackios.smsd.plist`. **Seen on the phone via `iphont` + screenshots (you can drive the phone yourself now: BUILD.md, research page):** an SMS arrives, a banner appears, tapping it opens the conversation, Messages' list is current. **Not seen:** the session after a modem restart; calls and sending SMS from Messages are not implemented. The docker `theos` volume had to be recreated (BUILD.md).
- **2026-10-29 (latest): flashed `out/instantnoodlep-standalone43.img` on boot_a** (sha256 b99b0441...; kernel `data19`, root `userland/root-cell28.apfs`): mobile data over native IPv6 (iOS's CLAT46 for IPv4), SMS **send from Messages is native** (the `libhackios-im` library in imagent, `/etc/inject.conf`), `hk_smsd` only resends failed rows and handles incoming. How I drove the phone and found things: BUILD.md, research/modem-sdx55.md. Not done: tethering, voice calls, native incoming SMS (would call `smsMessageReceived:msgID:` in imagent instead of the sms.db + bulletin path), MMS. Probe: boot-arg `im_probe=1` with a control image logs imagent's telephony calls.
- **2026-11-02: boot_a holds `out/instantnoodlep-standalone63.img`** (kernel `data22`, root `userland/root-cell63.apfs` made from the tree `out/tree/r42` by `out/tree/flash.sh N`, loader `out/loader-8pro-uefimenu9.efi` with the `baseband` node; arguments: the daily modem set + `modem_diag=0`, `exctrace=1 iphont=1 iphont_control=1` for the device agent, **no `applog`**). Verified on the phone: status bar LTE/bars/carrier, Settings > Cellular (data switch acts on en1 both ways, options, SIM PIN), About (Network, IMEI, ICCID, MEID from the modem), SMS sent and received through Messages, mobile data, idle CPU low (the earlier heat was hk_commcenter's: research/modem-sdx55.md "The older CoreTelephony API, the heat"). The owner's number is set on the phone only (`set_settings`). Tools: `out/tree/{flash.sh,cpu.py,tap.py,crop.py,snap.sh,reqs.py}`. Not done: Voice & Data row, usage list, Personal Hotspot, calls (no audio path; signalling not wired), live incoming SMS in an open conversation. Uncommitted.
- **2026-11-02 (later): boot_a holds `out/instantnoodlep-standalone80.img`** (kernel `data24`, root `userland/root-cell80.apfs` from `out/tree/r42`, the same loader, `mic=4` added). New: the signal bars' mode fixed (idle home screen 120 -> 30 frames a second, `backboardd` 62% -> 20%), log noise cut, microphone on, Phone keypad layout, CallKit provider, status bar icons 5 pt in. Upstream's surf_wrap, glstill and fb_pri are ported but off: no difference measured here. `hakios-metal` is **not** ported: it targets another chip (A635 family, chip id 0x06030500), and upstream's own default is still OpenGL ES. **Next: the owner tests real calls** (Do Not Disturb off); then the voice audio path. Dual boot: nothing written; the plan is in research/boot-chain-and-recovery.md. Everything uncommitted.
- **2026-11-02 (latest): boot_a holds `out/instantnoodlep-standalone92.img`** (kernel `data33`, no `voice=`, no bridge, `mic=4`). Real calls work both ways, answer and hang up from either side, status bar follows the network (LTE/3G). **A call has no sound**: state and next step in research/modem-sdx55.md, "The bridge". VoLTE not done. `hk_commcenter` must be re-signed after every build (`codesign -f -s - --entitlements userland/src/hk_commcenter.entitlements`).
- **2026-11-02 (end): boot_a holds `out/instantnoodlep-standalone98.img`** (kernel `data38`, `mic=4 voice=3 voice_sat=1`). **Calls have sound** (real 3G call, owner heard it; loudspeaker only). Earpiece, volume, mute, tuning and VoLTE not done: research/modem-sdx55.md "THE SOUND OF A CALL WORKS". Everything uncommitted.
- **2026-11-02 (afternoon; RAM runs only, boot_a still holds image 98):** `out/instantnoodlep-standalone102.img` (sha256 ac33519e2967f0c7..., kernel `data40`, root `userland/root-cell102.apfs`, `mic=4 voice=3 voice_sat=1`) is NOT flashed. New: a call starts at the earpiece (0x34 alone, profile `receiver`); iOS's speaker button works through `libhackios-call.dylib` in callservicesd (fake routes for a call, `kern.hackios.voice_speaker`); mute reaches the DSP (`kern.hackios.voice_mute`). Verified on the phone with a fake call and a voice session without a network call: the button lights up, the amplifiers switch both ways, no alarm, no crash. **Not heard on a real call yet.** `kern.exec_inject`'s kernel buffer is now 1024 bytes: the list had outgrown 512 and init's "error 22" silently took every hook away. Also new, untried on the network: hold, swap, call waiting (research/modem-sdx55.md). Next: flash on the owner's OK, a real call, judge the level (`kern.hackios.voice_earpiece_gain`, `hk_sysctl NAME=VALUE`).
- **2026-11-02 (late; boot_a holds image 110, flashed with the owner's OK; slot a marked successful in the GPT):** `out/instantnoodlep-standalone110.img` (sha256 38b9b64baff3a786..., kernel `data47`, UEFI `out/uefi-8pro-menu4.fd`, loader `out/loader-8pro-uefimenu10.efi`, `mic=4 voice=3 voice_sat=1 hk_slot_ok=1`). Verified on the phone, first by RAM boots, without a call: modem recovery (40 s, twice, data and voice session after it), 22 route changes clean. Not run: the menu's Android, any of it on a real call. `kernel_task` reads about 38% with the screen asleep on `data40` and `data47` alike (not a regression; about 9-11% was measured with the screen on): unexplained, worth a look for battery and heat. The UEFI's Docker volume `hk-uefi` was remade from `Mu-Silicium/` + `uefi/files`.
- **2026-11-02 (latest; boot_a holds image 113, flashed with the owner's OK):** image 110 had a bug of mine: `hk_slot_ok=1` took the boot line past 608 bytes, which cut off `var_data=hfs` and `hk_fast_halt=1` (no persistent volume, no Android from the menu). `out/instantnoodlep-standalone113.img` (sha256 d1525c9a8664cd8c..., kernel `data48`, loader `out/loader-8pro-uefimenu11.efi`, UEFI `out/uefi-8pro-menu4.fd`) fixes it. **Boot args are now opt-out:** `default_args` in the loader, `BOOT_LINE_LENGTH` 1024 in kernel and loader, the image's line holds only its own (BOOT_ARGS.md). RAM-boot checked: the whole line arrives (695 bytes), the data volume mounts kept, modem, data, silent voice session. Also in it, unproven until a real call: an outgoing call's sound starts at alerting, not at the dial; a CVS session the DSP will not destroy is retried and kept. **Owner-verified on image 113, 2026-11-02: the menu's Android boots Android; call sound "worked perfectly"; persistence works.**
- **Committed 2026-10-29 (the first public state of this repository):** everything of this session (kernel: `xnu/files/bsd/dev/{instantnoodle_modem,hk_mhi,instantnoodle_ufs,hk_input,hk_adreno,hk_cpufreq,...}.c`, `bsd/sys/{hk_mhi,hk_input}.h`, `xnu/hackios.patch`, `tools/macdev/kbuild`; userland: `src/inject_ui.c`, `inject_sb.c`; profile `common/devices/instantnoodlep.conf`; docs). 

## 1. State of the project (checked 2026-10-26)

- Active device: **OnePlus 8 (instantnoodle)**, no Android on it, iOS 16.5 user space running from its own flash. Installed boot verified 2026-10-24 (historical, owner-confirmed); iOS boots to SpringBoard, touch, 90 Hz display,
  GPU (OpenGL ES), speakers, main mic, haptics, flashlight, battery, Wi-Fi (RAM runs), sleep/wake, HFS+ `/private/var` work per [devices/instantnoodle.md](devices/instantnoodle.md).
- Redwood (POCO X5 Pro): not touched recently, owner asked not to run POCO checks.
- Everything is **uncommitted**: git HEAD of the kernel repo is `de24a37` (2026-10-25), of the userland repo `d2c1e87`. Work since 2026-10-25 ~15:00 (GPU collapse, brightness,
  `ca_ambient`, vmd DENY, Wi-Fi station work, rendering fixes, `tools/t`, wakeperf/look) lives in the working tree only.
- Docs: this overhaul created `docs/{README,architecture,BUILD,BOOT_ARGS,TODO,KNOWN_ISSUES,HANDOFF}.md`, `docs/{devices,subsystems,research}/`, `AGENTS.md`, `CLAUDE.md` and moved images to
  `docs/images/` — **all uncommitted**; run `git add -A docs AGENTS.md CLAUDE.md README.md` (+ `xnu/README.md`, `uefi/README.md`, `prebuilt/NOTICES.md`) in the **kernel** repo. `docs/air*.md` belong to the **userland** repo.

## 2. Uncommitted changes a new session must preserve

Check with `git status --short` **and** `common/ugit status --short` (many paths appear in both; see [architecture.md](architecture.md#two-repositories-one-tree)). 2026-10-26 03:25: 68 modified/deleted entries in the kernel
repo, 39 modified in the userland repo. Do not `git checkout`/`reset`/`stash` them. Groups:

| Group | Paths (representative) | Owner/meaning |
|---|---|---|
| Wi-Fi station and QCA6390 (verified in RAM runs, 2026-10-25) | `wifi/lx/src/*`, `wifi/lx/include/lx_more.h`, `xnu/files/bsd/dev/hk_wifi.c`, `hk_lx.c`, `hk_sta.c`, `hk_apple80211.c`, `hk_net.c`, `sys/hk_{lx,net,sta}.h`, `tools/mk-ath11k.sh`, `tools/wpa-test/run.sh` | WPA3 H2E, TKIP, private MAC etc. |
| Power / GPU / display (GPU collapse and brightness verified at command level 2026-10-26) | `hk_power.c`, `hk_adreno.c`, `HKGPU.cpp`, `hk_cpufreq.c`, `MiniIOMobileFramebuffer.cpp`, `loader_return.c`, `GenericARMPE.cpp`, `MiniDrivers.h`, `instantnoodle.h`, `hk_input.c/.h`, `instantnoodle_{i2c,touch}.c` | `xnu/export.sh` was run for round 3 addenda; **re-check that `xnu/` matches `xnu-src/`** before exporting again |
| Tools | `xl`, `xnu-build.sh`, `tools/frames.py`, `tools/t`, `tools/look/`, `tools/wakeperf/`, `tools/sleep-test/` | test harness (short tests) |
| Profile | `common/devices/instantnoodle.conf` | sound/sb_clock args |
| Userland repo | `userland/build.sh`, `mkjobs.py`, `src/eagl.c`, `inject.c`, `inject_sb.c`, `rwtouch.c`, `power_test.c`, `hkprof.h`, ... | rendering fixes, vmd DENY, ca_ambient, profilers; new files need `common/ugit add -f` |
| Docs | all of `docs/`, `AGENTS.md`, `CLAUDE.md`, `README.md` | this overhaul |

**Scheduler work (2026-10-26 02:20-04:55, done for now, uncommitted):** Edge scheduler + performance controller ([subsystems/scheduler.md](subsystems/scheduler.md)).
`xnu/export.sh` was run at 04:48 after checking `xnu/` was not ahead of `xnu-src/` (only `hk_cpufreq.c` and `GenericARMPE.cpp` differed, both newer in `xnu-src`); the export
also picked up other sessions' `xnu-src` edits then present (`bsd_init.c`, `kern_exit.c`, `IOService.cpp`, `exception.c`, `osfmk/conf/files.arm64`, `iokit/conf/files`, `bsd/sys/Makefile`).
New files to add when committing: kernel repo `xnu/files/bsd/dev/hk_perfctl.c`, `hk_sched_check.c`, `tools/sched-test/{qemu.sh,amp.py}`, `docs/subsystems/scheduler.md`;
userland repo `userland/src/sched_test.c` (`common/ugit add -f`). Baseline kernel binary from before the work: `out/sched-baseline/mach.development.bcm2837` (`XL_KERNEL=`).
A second session is building a **developer toolkit** (`tools/hkdev/`, `docs/TOOLKIT.md`, edits to TESTING/README/INSTALL command references). Ask before editing a file that is modified and not yours.

## 3. Active priorities (see TODO.md for all)

1. **T01/T02** verify power and Wi-Fi-through-sleep on the phone (safety paths unverified): [TODO.md](TODO.md#t01-verify-the-unverified-power-work-on-the-phone--power--built-unverified--p0).
2. **T11** scheduler — working (Edge default, `sched=clutch` baseline); open points in [TODO.md](TODO.md#t11-heterogeneous-cpu-scheduling--cpu--partial-working-uncommitted--p1).
3. **T03** install the current build (needs the owner's explicit approval; writes flash).
4. **T04/T05/T07/T08** missing hardware: sensors, Bluetooth, OnePlus cameras, headphones.
5. **T10** Metal usability; research notes in `docs/air.md` (userland repo).

## 4. Recent changes worth knowing (2026-10-24 .. 26, historical summary, details in the pages)

- 10-24: speakers, alert slider, main mic, HFS+ `/private/var`, installed boot on the OnePlus 8 ([audio.md](subsystems/audio.md), [storage.md](subsystems/storage.md)).
- 10-25: sleep/wake rounds 1-4 and the GIC shared-interrupt fix ([power.md](subsystems/power.md)); Wi-Fi QCA6390 ([wifi.md](subsystems/wifi.md)); App Library vibrancy fix; custom boot logo; `tools/t` ([TESTING.md](TESTING.md)).
- 10-26: **Edge scheduler on three clusters with our CLPC replacement** (`hk_perfctl.c`; boot-arg `sched=`; `config_pervasive_energy` on; `kern.hackios.amp`);
  membench threads bound and self-starting (a caller-starvation hang under `sched=clutch`).
- 10-26: GPU block collapse in sleep (133 ms wake), hwcg, touch boost, brightness service, `ca_ambient`, vmd denied (post-wake CPU), kernel probes (`top`, `cpuspeed`, `memlat`, `membench`, `kprof`, `hkprof`). The installed bundle predates these (T03).

## 5. Blockers
- Owner approval for anything that writes flash (`userdata`, `boot_b`), swaps slots/ABL/XBL/TZ (forbidden) or runs audibly.
- SoC low-power states are architecturally blocked ([power.md](subsystems/power.md)). Sensors need the DSP's SEE client; App Store/FairPlay need Secure Enclave behaviour.
- No Android on the OnePlus 8: its vendor/persist/modem firmware only exists in backups outside the repo (`../ios-instantnoodle/`).

## 6. Verified findings to rely on (details in pages)
CrashDump triggers ([research/hardware-qualcomm.md](research/hardware-qualcomm.md)); never DCS 0x10; never `touchscreen=1` on the OnePlus; Android's QCA6390 firmware asserts under ath11k (use linux-firmware);
GIC per-core shutdown must not quiesce shared SPIs; `vmd` was the post-wake CPU hog; sleep timers use wall time; tests: `tools/t host` 2 s, `quick` ~70 s, `power` ~2 min.

## 7. Tests performed and results (checked unless marked)
- 2026-10-26 02:40-04:50 (scheduler session): `tools/sched-test/qemu.sh` 4/4 modes PASS; phone `tools/t boot sched` Edge 8/8 kernel checks, A/B workloads (scheduler.md);
  `tools/t full` Edge 16/17 and Clutch 16/17 (both fail only "frames after sleep" phase 2 after 5 rapid sleeps, ~66 fps: pre-existing); one false `sleep` FAIL (K24).
- 2026-10-26 (historical, from session notes): `tools/t power` 10 sleeps with GPU CX/GX off, wake 133 ms; `tools/t brightness`: 20/90/5/100% reach the panel as 0x52/0x67a/0x6/0x7ff; `tools/t full` passed except a noisy frames-after-wake phase (verdict loosened).
- 2026-10-25 (historical): `common/flash/qemu-test.sh` 14/14; `tools/sleep-test/run.sh` 4/4; phone: 3/3 sleeps, power-key wake, WPA2/TKIP-mixed/WPA3 joins.
- This documentation session ran **no hardware tests** and made no functional changes. It checked links and referenced paths mechanically with a throw-away script (not kept in the repository).

## 8. Where things are
Index [README.md](README.md) · architecture [architecture.md](architecture.md) · build [BUILD.md](BUILD.md) · boot-args [BOOT_ARGS.md](BOOT_ARGS.md) · tests [TESTING.md](TESTING.md) ·
research [research/README.md](research/README.md) · agent rules [../AGENTS.md](../AGENTS.md) · phone logs `logs/` (`t-<time>.txt`, `<time>-run.log`, `.live` copies) · build log `out/kernel-build.log`.

## 9. Recommended immediate next actions
1. Read `git status` of both trees; commit the docs (kernel repo) without touching the others' files.
2. Ask the owner for a short supervised phone session: `tools/t power`, then T01 charger/off-mode checks.
3. When the scheduler session finishes, export (`xnu/export.sh` after syncing `xnu-src` to the patch), update [subsystems/cpu.md](subsystems/cpu.md), TODO T11.
4. Pick one missing-hardware item (T04 sensors is the most valuable for iOS behaviour) and start with its first read-only probe.

## iphont continuation — 2026-10-26

- New agent CLI is **`./iphont`**, Python standard library, not `tools/hkdev`/Rust. Start with `./iphont describe` and [TOOLKIT.md](TOOLKIT.md). Host modules, `xnuloader/iphont.c`, `xnu/files/{bsd/dev/iphont.c,bsd/sys/iphont.h,iokit/Kernel/IPhont.cpp,osfmk/arm64/iphont_threads.c}`, kernel hooks, and the userland daemon/job are uncommitted. New userland files need explicit `common/ugit add -f` when a commit is requested.
- Preserved scheduler/Wi-Fi work. Added only a narrow `hk_touch_inject` API to the scheduler-owned input files: same queue/boost, no physical recovery-chord counting. Apple-file patch sections were regenerated selectively; **no whole-tree export from a stale private tree**. `xl` log/capture staging and fetches now hold one USB lock. No legacy script was deleted; [TOOLKIT-MIGRATION.md](TOOLKIT-MIGRATION.md) inventories 170 owned scripts/modules and the parity gates.
- Verified: 32 host checks including the real loader C protocol fixture; existing `tools/t host` 4/4; native loader/daemon builds; clean private XNU compile/link (known export checks still fail); APFS verified by both readers; seven real XNU QEMU probes and a timed return. Physical phone: state/log snapshots/screenshots and five PNG captures (~1.6 fps), without changing the run. This is **not** mailbox, daemon, touch, or keyboard hardware validation.
- **Owner deferral:** “Continue host/build checks; leave phone running.” No reboot, slot change, partition write, or new physical boot was performed. The next physical test needs that restriction lifted and coordination with the running phone session, including the known cable replug requirement.
- Ignored reviewable artifacts are in `out/iphont/`: `loader-ram.img`, new `loader.efi`, `iphontd`, diagnostic `root.apfs`, private `xnu-clean/.../mach.development.bcm2837`, host/QEMU/build/image reports, and capture files. `prepare` packages a RAM-only instantnoodle image and never touches the phone. The new root is md1 diagnostic content; preserve the ordinary resident md0 flashboot workflow.
- **Build trap found:** copied `BUILD/` embeds absolute `SOURCE_DIR` paths into the original checkout. A copy can otherwise compile concurrent scheduler edits against stale private headers. The clean private build discarded only its own generated `BUILD/`; shared builds/source were not reset. Use `out/iphont/xnu-clean`, not the older mixed-path `xnu-integrated` artifact, for the next RAM test.
- Remaining scope is T36/K22: supervised USB/agent/control tests, framework/entitlement behavior, deeper GPU/service tests, recovery/deployment parity, function symbolication, and legacy removal after evidence. No claim of comprehensive completion.

Validation note: source whitespace checks pass. Full `git diff --check` flags unified-diff context whitespace inside the stored `xnu/hackios.patch`; raw source checks are separate. The original concurrent scheduler/Wi-Fi/docs changes remain uncommitted and were not reset.
