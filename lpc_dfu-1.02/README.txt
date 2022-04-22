Build instructions:

Linux
-----
- It needs libusb-1.0-dev and pkg-config installed
- untar file with 'tar xfz lpc_dfu-X.XX.tar.gz'
- run 'make' from the lpcdfu dir

To cross-build for Win32 on Linux (it needs wget and 7z)
run crosswin.sh

To run on Linux use dfuprog.sh shell script, you need dfu-util 0.7 installed.
You also need proper udev rules, try
# sudo cp 45-dfuutil.rules /etc/udev/rules.d
and reboot.


Windows and Mingw32
-------------------
- Download and extract libusbx-1.0.17-win.7z (http://downloads.sourceforge.net/project/libusbx/releases/1.0.17/binaries)
- Copy libusb.h, from include/libusbx-1.0/ to $HOME/include (if it does not exist, create it)
- Copy the MinGW32/dll/ content to $HOME/dll (if it does not exist, create it)
- Open mingw/msys shell
- unzip file 'lpc_dfu-X.XX.zip'
- run 'make' from the lpcdfu dir

To run on Windows use dfuprog.sh from the mingw/msys shell, you need dfu-util 0.7 installed and of course the msys shell.
You also need winusb drivers for DFU (http://www.lpcware.com/content/nxpfile/lpc18xx43xx-winusb-drivers-dfu-boot)
already installed.
