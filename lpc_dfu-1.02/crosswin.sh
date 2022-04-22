#!/bin/bash
LIBUSB_PATH="${HOME}/tmp/libusbx_win"
LIBUSB_URL=http://downloads.sourceforge.net/project/libusbx/releases/1.0.17/binaries
LIBUSB_NAME=libusbx-1.0.17-win.7z

set -o errexit

WHEREIAM=`pwd`

if [ ! -d "${LIBUSB_PATH}" ]; then
	echo "Libusb not found. Download and install"
	mkdir -p "${LIBUSB_PATH}"
	cd "${LIBUSB_PATH}"
	wget "${LIBUSB_URL}/${LIBUSB_NAME}"
	7z x ${LIBUSB_NAME}
	cd ${WHEREIAM}
fi

make TARGETOS=WIN32 clean
make TARGETOS=WIN32 USBLIB_CFLAGS="-I${LIBUSB_PATH}/include/libusbx-1.0" USBLIB_LDFLAGS="-L${LIBUSB_PATH}/MinGW32/dll -lusb-1.0"
