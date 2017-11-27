#!/bin/bash

source ./setenv.sh

NUM_CPUS=$(cat /proc/cpuinfo | grep -c processor)

make mxs_imx280a_defconfig
make -j${NUM_CPUS} imx28-imx280a.dtb
make -j${NUM_CPUS} zImage
cat ./arch/arm/boot/dts/imx28-imx280a.dtb >> ./arch/arm/boot/zImage && sync
make -j${NUM_CPUS} LOADADDR=0x42000000 uImage
make -j${NUM_CPUS} modules
make -j${NUM_CPUS} firmware

rm -rf deploy/imx280a
mkdir -p deploy/imx280a/mod
mkdir -p deploy/imx280a/mod/usr/src/linux

cp -f arch/arm/boot/uImage /home/john/share/imx280/工具/烧写工具/cfimager/ && sync

cp arch/arm/boot/uImage deploy/imx280a
make modules_install INSTALL_MOD_PATH=deploy/imx280a/mod
make firmware_install INSTALL_MOD_PATH=deploy/imx280a/mod
make headers_install INSTALL_HDR_PATH=deploy/imx280a/mod/usr/src/linux
