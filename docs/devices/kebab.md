# OnePlus 8T (kebab, KB2000) — Snapdragon 865 (SM8250, "kona")

**Status 2026-11-18, verified on the phone: iOS 16.5 boots to SpringBoard from RAM, touch works (Synaptics S3908, TCM), and the Adreno 650 starts (`gpu=1`, steps 1–21).** Profile `common/devices/kebab.conf`, UEFI target `kebab` (Mu-Silicium carries `Platforms/OnePlus/kebabPkg` upstream), loader platform `platform_kebab`, kernel `board=kebab`. The phones with working iOS are [instantnoodle.md](instantnoodle.md) (8) and [instantnoodlep.md](instantnoodlep.md) (8 Pro). The rest of this page keeps the 2026-11-17 reading of the phone (device tree, ABL log) and the first boot, then what the 2026-11-18 runs established.

## What is known about this phone (observed, read-only)

On 2026-11-17 the attached KB2000 was booted into **TWRP 3.7.0_11 from RAM** (`fastboot boot`, no partition written) to get root. The following were pulled to `../op8t-data/` (outside both repos, never committed): `/sys/firmware/fdt` → `dtb/kebab-live.dtb` (912910 bytes), `/proc/{cpuinfo,iomem,interrupts,meminfo,cmdline}`, `getprop`, the by-name map and `/proc/bootloader_log`.

| Fact | Evidence |
|---|---|
| Android 11 HydrogenOS `KB2000_21_201110`, slot `_a`, bootloader **unlocked** (`flash.locked=0`, `verifiedbootstate=orange`) | `getprop`; no adb root in the stock ROM, so the RAM recovery |
| Same SM8250 SoC: GIC `17a00000`/`17a60000`, UFS `1d84000`, PM8150, Adreno 650, QCA6390 + external SDX55M | live DTB; `tools/board-from-dt.py` returns the kernel's existing defaults, so `BOARD_ARGS=""` |
| Panel: Samsung **amb655x**, 1080x2400, DSI command mode + DSC, 60 Hz `timing@0` and 120 Hz `timing@1` (`timing-default`); `qcom,mdss-dsi-timing-switch-command` for 120 Hz is `f0 5a 5a / 60 10 / f0 a5 a5` | active panel in `/proc/cmdline`: `msm_drm.dsi_display0=qcom,mdss_dsi_samsung_amb655x_dsc_cmd`; ABL: `DisplayDxe: Resolution 1080x2400`, panel id `00A50501`; live DTB |
| Pixelworks Iris properties are present (`pxlw,iris-chip-enable`, `pixelworks,iris`), as on the Pro | live DTB; which mode ABL left the chip in is unknown |
| Touch: the live DTB's `sec-s6sy761@48` node (Samsung S6SY761/SY771 at 0x48) is **not** the chip that answers: a **Synaptics S3908** does, at I2C **0x4b**, reset/IRQ TLMM 38/39, panel 1079x2399 | 2026-11-18, TCM `CMD_IDENTIFY`: `part "s3908-15.0.0..."`, `application 1079 x 2399, 10 objects`; the vendor source in `../op8t-data/vendor-src/synaptics_tcm_oncell.c` (`compatible = "synaptics-s3908"`) is the reference the driver was written from |
| GPU ZAP region `0x8c51a000` (the 8's, not the Pro's `0x8e21a000`); gauge bq27541 at 0x55; haptics AW8697 at 0x5a | live DTB reserved-memory and I2C nodes |
| Continuous splash `0x9c000000`, size `0x2300000` — the same address/height as the OnePlus 8 | live DTB and ABL |

Charging is 65 W Warp with no wireless charging: **do not copy the 8 Pro's 95 % / 4.40 V charging limits**.

## What differs from the 8 / 8 Pro, and what the port does

| | OnePlus 8 | OnePlus 8 Pro | OnePlus 8T (this port) |
|---|---|---|---|
| Panel | ANA6705, 1080x2400, 90 Hz, curved | ANA6706, 1440x3168 (run at 1080x2376), 120 Hz, curved | amb655x, 1080x2400, 120 Hz, flat |
| Touch | S6SY761 | S6SY791 | **Synaptics S3908 (TouchComm/TCM) at 0x4b**, `bsd/dev/kebab_tcm.c`; the 8's S6SY761 driver stays gated off (`kebab_touch=1` runs it) |
| Cameras | IMX586 main | IMX689 + telephoto + AK7377 lens motor | no Pro telephoto; `cam=` off until the sensors are checked |
| GPU ZAP | `0x8c51a000` | `0x8e21a000` | `0x8c51a000` — the non-Pro branch of `hk_adreno.c` |
| iOS geometry | 1080x2400 | 1290x2796 (GPU-scaled) | **1290x2796** (430x932 pt, scaled to the 1080x2400 panel with `fb_push=2`; at the panel's own size iOS has no keyboard layout, K26) |
| Loader platform | `instantnoodle` | `instantnoodlep` | `kebab`; same fb as the 8, so `mkpayload.py --platform` disambiguates |

Source work (uncommitted): `common/devices/{kebab.conf,kebab.platform.c}`; `xnuloader/loader.c` (`platform_kebab`, `PLAT_IS_KONA()`, payload `platform=` override), `xnuloader/mkpayload.py --platform`, `common/build.sh` + `xl` send it; `board=kebab` joins `instantnoodle_board()` while `instantnoodlep_board()` stays false; the 8's 90 Hz switch is excluded; `uefi/apply.sh` copies a starting DXE set into `uefi/files/Binaries/kebab/`; `tools/board-from-dt.py` now forces output for live trees with mismatched phandles.

## Build and first RAM boot (2026-11-17, verified)

Built here with the system toolchain (Theos + iPhoneOS16.5.sdk in `~/theos`), no Nix:

```sh
xnu-deps/setup.sh --stage1 && ./xnu/apply.sh && xnu-deps/setup.sh --stage2
./xnu-build.sh -j8 -k BUILD_DSYM=0      # dsymutil is macOS-only; the binary is linked before that step
xnuloader/build.sh                     # clang + lld-link
# Mu-Silicium: uefi/apply.sh (manual: its patch is CRLF/LF-sensitive), copy kebab-live.dtb into Resources/DTBs/kebab.dtb
source venv/bin/activate && CLANG_BIN=$PWD/../uefi-toolchain/bin/ python3 build_uefi.py -d kebab -r RELEASE
PATH=../uefi-toolchain/bin:$PATH SKIP="kernel images" common/build.sh kebab
```

Host fixes needed for Ubuntu: `xnu/hackios.patch`'s `bsd/conf/files` hunk header had +55 where 57 lines were added (fixed in the patch); `makesyscalls.sh`'s awk `\(` escapes are removed by `xnu/apply.sh` on Ubuntu; `BUILD_DSYM=0` skips the macOS `dsymutil`; the EDK2 build needs `llvm-*` tools without the `-21` suffix next to `CLANG_BIN` (`uefi-toolchain/bin/`).

First RAM-only boot (`fastboot boot out/kebab-boot.img`, nothing flashed):

- Loader: `display 1080 x 2400 at 9c000000`, **`platform kebab`**, `usb: started`.
- Kernel with `XL_RAMDISK=none`: 7 cores online (prime + performance + efficiency clusters), GICv3, scheduler edge, perfctl, `hk_hfs` registered, and `MiniIOMobileFramebuffer: display 1080x2400 on a panel of 1080x2400 (4320 bytes per row)`. The only panic is `Failed to mount root device` — expected with no userland image in RAM.
- Phone returned to Android 11 afterwards; `fastboot boot` works on this HydrogenOS 11 ABL.

## 2026-11-18, verified on the phone (RAM only, nothing flashed)

All runs over `xl` (`. common/lib.sh; load_device kebab`), `XL_RAMDISK=userland/root.apfs`, the
resident image uploaded with `./xl ramdisk userland/system.apfs` (2.3 GB, ~95 s; a `fastboot boot`
or a trip through Android empties RAM and it must be uploaded again).

- **iOS boots to SpringBoard** (`launchd`, `backboardd`, `SpringBoard` alive) and keeps repainting.
- **Touch works end to end**, and the screen shows it. Evidence, run `logs/215439-run.log`:
  `tl: tcm: 1320 reports: 9 down, 1303 move, 8 up; 0 unreadable, 0 bad headers, 0 out of sync`;
  the kernel's sample reaches backboardd in 50–70 µs (`tl: p` → `tl: i`); `[TouchEvents] new scene
  host settings: … <sceneID:com.apple.mobiletimer> … -> foreground` after the tap on Clock, and
  Settings before it (run `204310`). The owner saw both open. An earlier complaint of "touch does
  nothing" came from a run whose boot-args never reached the kernel (below) — not from the chip.
- **Display is software-rendered**: `MiniIOMobileFramebuffer: … (10 a second) … 0 frames scaled by
  the GPU`, `0 by the GPU`. The panel's last frame stays on the glass after the kernel returns, so a
  loader-idle phone looks "frozen" — that is not a hang.
- **GPU: the kernel's whole bring-up works.** `adreno=3` (reads only), `adreno=9` (CX power switch
  and clocks on, `CX is on`, `smmu cb0 tcr = 0x18b77597`), `adreno=12` (`TRUSTZONE ACCEPTS THE ZAP
  SHADER'S SIGNATURE FROM THIS KERNEL`), `adreno=17` (`THE 3D CORE HAS POWER AND ANSWERS`),
  `adreno=21` (`THE 3D CORE WRITES OUR MEMORY ON COMMAND`, page `0x600dc0de`), and `gpu=1`
  (`THE GPU STAYS ON for the system to draw with: the service HKGPU`) with SpringBoard up. Speed bin
  fuse `0x10` → Adreno 650; ZAP region `0x8c51a000` as the non-Pro branch expects.
  **Accelerated (2026-11-19):** with `GL=1` in `USERLAND_VARS` the image carries Mesa
  `lib/libOSMesa.8.dylib` + `lib/libhakios.gl.dylib`, and CoreAnimation draws with the GPU:
  `hakios-gl: 8 of 8 methods of EAGLContext are ours`, `an OpenGL ES 3 context … OpenGL ES 3.2 Mesa
  25.0.7; FD650`, `frame 8: 52 GL calls, 6 draws … 3374 us`, and the display goes from 10–24 to
  **50 frames a second**. (The kernel's GPU service class is `HKGPU` here; `inject.c`, `inject_sb.c`,
  `gl_test.c` and `tools/mesa/hakios-drm` now match either that or upstream's `HakiosGPU` — before
  that they found nothing and the window server silently kept its software renderer.)
- **GL defects (K25): fixed 2026-11-19.** Text came out as solid bars in its own colour and a visit to Control Center
  left large black areas, because the image table (`256` textures with IOSurfaces) filled up and new surface-backed
  textures got no storage at all -- an incomplete texture samples as opaque. Now 1024 slots with eviction when full.
  Owner-verified: Settings > General > iPhone Storage draws its text, and Settings after Control Center is clean.
- **Keyboard (K26): fixed 2026-11-19.** It ran off the right edge of the screen because iOS was told the panel's own
  1080 x 2400 = 360 x 800 pt, which is no iPhone's size and has no keyboard layout; iOS 16.5 has layouts up to
  430 x 932 pt (K43). iOS now renders at **1290 x 2796** and `fb_push=2` scales to the panel: the keyboard is right,
  and the home screen stays at 56-59 frames a second. `gpu=1` plus `GL=1` throughout.
- **Vendor firmware for the GPU** was pulled read-only from the phone's `/vendor/firmware`
  (`a650_gmu.bin`, `a650_sqe.fw`, `a650_zap.mdt`, `.b00`–`.b02`) with TWRP 3.7.0_11 booted from RAM
  (`fastboot boot`, root over adb; the stock Android denies shell access to `/vendor/firmware` and
  its fastbootd has no `fetch`). They live in `fw-gpu/` (untracked, `.gitignore` covers it) and are
  packed into `root.apfs` as `fw/`, which is where `hk_image_file("fw", …)` finds them.
- **`rec_secs`/`rec_step`/`rec_ring`/`touch_lat` and any other extra boot-arg now reach the kernel**
  — they did not before 2026-11-18: `xnuloader/loader.c`'s `with_defaults()` cached the merged
  command line for good, so the first parse (the USB payload that starts the loader, with no line at
  all) decided every later run's line and a run got the platform's `default_args` and nothing it
  asked for. The cache is now keyed by the line it was made from.


## What is still unverified / next

(History: the first supervised RAM-only boot was on 2026-11-17, while ColorOS 14 on this model was
said to refuse `fastboot boot` and never booted slot a; this phone is HydrogenOS 11, where
`fastboot boot` works — `xl enter` has been the delivery route for every run since.)

1. **Mesa/OpenGL ES (`GL=1`) and the profile's `USERLAND_VARS`**, so CoreAnimation draws with the GPU the kernel now keeps on: `tools/mk-mesa.sh` builds `libOSMesa.8.dylib` + `libhakios.gl.dylib`, which `userland/build.sh GL=1` puts in `root.apfs` (`root/lib/`), so no system image is rebuilt. Until then `gpu=1` changes nothing on the glass.
2. **Audio, haptics, torch, Wi-Fi, camera, modem** — one boot-arg group at a time, each after a run, each only added to `kebab.conf` once the phone has shown it.
3. **The DXEs from this phone's `xbl`** (UEFIReader): the OnePlus 8 set in `uefi/files/Binaries/kebab/` is a placeholder; never commit vendor binaries.
4. Only then an installed image and a flash — every partition write needs fresh, per-write approval.

## Safety for this phone

Never `touchscreen=1` (redwood's Goodix driver on a OnePlus pin), never `wifi_again=1`/`adsp_again=1`, never a DCS sleep-in (0x10) to the panel. Wrong register or pin writes drop a OnePlus into Qualcomm CrashDump (Power+VolUp to leave). Tell the owner before and after every run; one hardware run at a time (`./xl wait`). See [../research/hardware-qualcomm.md](../research/hardware-qualcomm.md).
