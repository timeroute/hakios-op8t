#!/bin/bash
# Make ../Mu-Silicium: a clone of Project-Silicium/Mu-Silicium at the commit our port was made on,
# with the redwood port (this directory) put into it. See README.md here for how to build it.
set -e
cd "$(dirname "$(readlink -f "$0")")"
MU=../Mu-Silicium
[ -e $MU ] && { echo "uefi/apply.sh: $MU already exists"; exit 1; }
git clone https://github.com/Project-Silicium/Mu-Silicium.git $MU
git -C $MU checkout -q "$(cat BASE)"
git -C $MU submodule update --init --recursive
git -C $MU apply --whitespace=nowarn "$PWD/mu-silicium.patch"
# The OnePlus 8 Pro uses the OnePlus 8's driver set unchanged (files/Binaries/instantnoodlep/README.md); its README is overlaid below.
mkdir -p $MU/Binaries/instantnoodlep
cp -r files/Binaries/instantnoodle/. $MU/Binaries/instantnoodlep/
# The OnePlus 8T is the phone those drivers came from in the first place (instantnoodle.md: the 8's set is mostly
# kebab's). Start with the same set, then replace it with the kebab DXEs extracted from THIS phone's xbl
# before trusting a boot; see files/Binaries/kebab/README.md.
mkdir -p $MU/Binaries/kebab
cp -r files/Binaries/instantnoodle/. $MU/Binaries/kebab/
cp -r files/. $MU/
[ -e $MU/Resources/DTBs/instantnoodlep.dtb ] || echo "uefi/apply.sh: for the OnePlus 8 Pro run: uefi/mk-instantnoodlep-dtb.py BOOT_B.img $MU/Resources/DTBs/instantnoodlep.dtb (docs/devices/instantnoodlep.md)"
[ -e $MU/Resources/DTBs/kebab.dtb ] || echo "uefi/apply.sh: for the OnePlus 8T run, put THIS phone's kebab.dtb in Mu-Silicium/Resources/DTBs/ (docs/devices/kebab.md)"
echo "uefi/apply.sh: done; next: uefi/README.md"
