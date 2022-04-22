Flashing lpc4370 external spiflash with openocd in linux

first copy 4370.cfg to your openocd scripts/target folder
sudo cp 4370.cfg /usr/share/openocd/scripts/target/4370.cfg
(This will be used in conjunction with the existing lpc4370.cfg file)
then edit it for your interface IE
source [find interface/cmsis-dap.cfg]
or
source [find interface/ft2232-jtag.cfg]

start openocd
openocd -f /usr/share/openocd/scripts/interface/ft2232-jtag.cfg -f /usr/share/openocd/scripts/target/4370.cfg
(I know we already specified interface but doing it again helped for sum reason)

in another terminal
telnet 127.0.0.1 4444

halt

flash write_image erase firmware.bin 0x14000000


Flashing with dfu
copy everything in dfuprog to a place in path

sudo cp dfuprog/* /usr/local/bin
sudo chmod +x /usr/local/bin/dfuprog.sh
dfuprog.sh firmware.bin

UPDATE: added lpc-add-vectortbl-2-frmware which adds the crc required to the firmware (NOTE normaly auto added depending on upload method)
and lpc-dfu-header to add the required dfu header for ram only upload, after header firmware file is created all that is needed is "dfu-util -D firmwarefile"
and added my awesome pinout, im so good at making those I should get paid (note the sarcasm)

The scripts where taken from hackrf/greatfet source and the best version of dfu-util to use so far ive found is 0.9
