#!/bin/bash
if [ "$#" -ne 2 ]; then
    echo "Usage: <pebble-firmware-utils path> <platform name>"; exit 1;
fi

cd Resources
# Extract and repack as described here
# https://github.com/pebble-dev/wiki/wiki/Working-with-resources
../Utilities/convert-any-to-pebble-image.py $2 $2_splash.png $2_splash.raw
$1/unpackFirmware.py $2_system_resources.pbpack
cd res
rm 474_*
cp ../$2_splash.raw ./474_splash
cat ./474_splash | python2 $1/calculateCrc.py - | 
grep -oP "(?<=Hex: 0x)[0-9a-fA-F]+" - | 
awk '{printf "mv ./474_splash ./474_%s\n", $1}' FS=/ | sh
cd ..
rm $2_system_resources.pbpack
$1/packResources.sh -o $2_system_resources.pbpack res/*
dd if=$2_system_resources.pbpack of=./$2_spi.bin bs=1 seek=3670016 conv=notrunc
rm -r res