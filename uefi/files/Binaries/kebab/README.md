## Firmware Infos — OnePlus 8T (kebab, KB2000), SM8250

The OnePlus 8's driver set in `../instantnoodle/` already came mostly from `kebab` (see that README);
`uefi/apply.sh` copies it here as a starting point. It is **not** this phone's own set: before a kebab
UEFI is trusted, replace these DXEs with the ones extracted from the attached KB2000's `xbl_a`/`xbl_b`
with [UEFIReader](https://github.com/WOA-Project/UEFIReader). Never commit the binaries.

- **Source to use:** this phone's `xbl` (bootloader unlocked, 2026-11-17), not the firmware of another 8T.
- **USB matters:** our loader talks to `xl` as a USB **device**. Device-Binaries' kebab `UsbConfigDxe`
  is patched to **host** mode; the OnePlus 8's set here has the USB drivers from its own OxygenOS 11
  `xbl_b` and an unpatched `UsbConfigDxe`. If `xl log` does not attach, this is the first thing to check.
- **Device tree:** the tree the loader hands XNU, and the source of the `board_<block>=` overrides, must
  be this phone's own (from its `vendor_boot`/`boot` image), placed in
  `Mu-Silicium/Resources/DTBs/kebab.dtb`. Mu-Silicium's upstream `kebab.dtb` is only a placeholder.
- **FD geometry:** `0x9FC00000`, size `0x300000` (Mu-Silicium `Resources/Configs/kebab.toml`, the same
  map as the OnePlus 8/8 Pro).
