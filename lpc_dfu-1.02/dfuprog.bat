@echo off
echo Downloading algo...
dfu-util -d 1fc9:000c -t 2048 -R -D iram_dfu_util_spiflash.bin.hdr
echo Programming %1
lpcdfu -d 3 -e -D %1 -U
