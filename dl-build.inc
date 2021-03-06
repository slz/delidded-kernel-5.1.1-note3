#!/bin/bash
# delidded kernel for Samsung Galaxy Note 3 build script.
# This build script is for AOSP/CyanogenMod only
#
# Originally written by jcadduono
# Updated by frequentc

################### BEFORE STARTING ################
#
# download a working toolchain and extract it somewhere and configure this file
# to point to the toolchain's root directory.
# I highly recommend Christopher83's Linaro GCC 4.9.x Cortex-A15 toolchain.
# Download it here: http://forum.xda-developers.com/showthread.php?t=2098133
#
# once you've set up the config section how you like it, you can simply run
# ./dl-build.sh
# while inside the /delidded-kernel-note3/ directory.
#
###################### CONFIG ######################

# root directory of delidded's git repo (default is this script's location)
RDIR=$(pwd)

# a short name for the kernel (will be used to locate ramdisk and zip folders)
KERNEL_SHORTNAME=dl

[ -z $VARIANT ] && \
# device variant/carrier, possible options:
#	can = N900W8	(Canadian, same as T-Mobile)
#	eur = N9005	(Snapdragon International / hltexx / Europe)
#	spr = N900P	(Sprint)
#	tmo = N900T	(T-Mobile, same as Canadian)
#	kor = N900K/L/S	(Unified Korean / KT Corporation, LG Telecom, South Korea Telecom)
# not currently possible options (missing cm12.1 support!):
#	att = N900A	(AT&T)
#	usc = N900R4	(US Cellular)
#	vzw = N900V	(Verizon)
VARIANT=tmo

[ -z $VER ] && \
# version number
VER=0.1

# kernel version string appended to 3.4.x-delidded-kernel-hlte-
# (shown in Settings -> About device)
KERNEL_VERSION=$VARIANT-$VER-cm12.1-5.1.1

[ -z $PERMISSIVE ] && \
# should we boot with SELinux mode set to permissive? (1 = permissive, 0 = enforcing)
PERMISSIVE=1

# output directory of flashable kernel
OUT_DIR=$RDIR

# output filename of flashable kernel
OUT_NAME=delidded-kernel-hlte-$KERNEL_VERSION

# should we make a TWRP flashable zip? (1 = yes, 0 = no)
MAKE_ZIP=1

# should we make an Odin flashable tar.md5? (1 = yes, 0 = no)
MAKE_TAR=0

# directory containing cross-compile arm-cortex_a15 toolchain
TOOLCHAIN=~/arm-cortex_a15-linux-gnueabihf-linaro_4.9.4-2015.06

# amount of cpu threads to use in kernel make process
THREADS=3

############## SCARY NO-TOUCHY STUFF ###############

export ARCH=arm
export CROSS_COMPILE=$TOOLCHAIN/bin/arm-eabi-
export LOCALVERSION=$KERNEL_VERSION

if ! [ -f $RDIR"/arch/arm/configs/dl_"$VARIANT"_defconfig" ] ; then
	echo "Device variant/carrier $VARIANT not found in arm configs!"
	exit -1
fi

if ! [ -d $RDIR/${KERNEL_SHORTNAME}.ramdisk/variant/$VARIANT/ ] ; then
	echo "Device variant/carrier $VARIANT not found in ${KERNEL_SHORTNAME}.ramdisk/variant!"
	exit -1
fi

[ $PERMISSIVE -eq 1 ] && SELINUX="permissive" || SELINUX="enforcing"

KDIR=$RDIR/build/arch/arm/boot

CLEAN_BUILD()
{
	echo "Cleaning build..."
	cd $RDIR
	rm -rf build
	echo "Removing old boot.img..."
	rm -f ${KERNEL_SHORTNAME}.zip/boot.img
	echo "Removing old zip/tar.md5 files..."
	rm -f $OUT_DIR/$OUT_NAME.zip
	rm -f $OUT_DIR/$OUT_NAME.tar.md5
}

BUILD_KERNEL_CONFIG()
{
	echo "Creating kernel config..."
	cd $RDIR
	mkdir -p build
	make -C $RDIR O=build dl_defconfig \
		VARIANT_DEFCONFIG=dl_${VARIANT}_defconfig
}

BUILD_KERNEL()
{
	echo "Starting build..."
	make -C $RDIR O=build -j"$THREADS"
}

BUILD_RAMDISK()
{
	echo "Building ramdisk structure..."
	cd $RDIR
	mkdir -p build/ramdisk
	cp -ar ${KERNEL_SHORTNAME}.ramdisk/common/* build/ramdisk
	cp -ar ${KERNEL_SHORTNAME}.ramdisk/variant/$VARIANT/* build/ramdisk
	echo "Building ramdisk.img..."
	cd $RDIR/build/ramdisk
	mkdir -pm 755 dev proc sys system
	mkdir -pm 771 data
	find | fakeroot cpio -o -H newc | gzip -9 > $KDIR/ramdisk.cpio.gz
	cd $RDIR
}

BUILD_BOOT_IMG()
{
	echo "Generating boot.img..."
	$RDIR/scripts/mkqcdtbootimg/mkqcdtbootimg --kernel $KDIR/zImage \
		--ramdisk $KDIR/ramdisk.cpio.gz \
		--dt_dir $KDIR \
		--cmdline "quiet console=null androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3F androidboot.bootdevice=msm_sdcc.1 androidboot.selinux=$SELINUX" \
		--base 0x00000000 \
		--pagesize 2048 \
		--ramdisk_offset 0x02900000 \
		--tags_offset 0x02700000 \
		--output $RDIR/${KERNEL_SHORTNAME}.zip/boot.img

#	$RDIR/tools/dtbTool -o $KDIR/boot.img-dtb --page-size 2048 --dtc-path $RDIR/scripts/dtc/ $KDIR/
#        mkbootimg --kernel $KDIR/zImage \
#                --ramdisk $KDIR/ramdisk.cpio.gz \
#                --dt $KDIR/boot.img-dtb \
#                --cmdline "quiet console=null androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3F androidboot.bootdevice=msm_sdcc.1 androidboot.selinux=$SELINUX" \
#                --base 0x00000000 \
#		--kernel_offset 00008000 \
#                --pagesize 2048 \
#                --ramdisk_offset 0x02900000 \
#                --tags_offset 0x02700000 \
#                --output $RDIR/${KERNEL_SHORTNAME}.zip/boot.img 

}

CREATE_ZIP()
{
	echo "Compressing to TWRP flashable zip file..."
	cd $RDIR/${KERNEL_SHORTNAME}.zip
	zip -r -9 - * > $OUT_DIR/$OUT_NAME.zip
	cd $RDIR
}

CREATE_TAR()
{
	echo "Compressing to Odin flashable tar.md5 file..."
	cd $RDIR/${KERNEL_SHORTNAME}.zip
	tar -H ustar -c boot.img > $OUT_DIR/$OUT_NAME.tar
	cd $OUT_DIR
	md5sum -t $OUT_NAME.tar >> $OUT_NAME.tar
	mv $OUT_NAME.tar $OUT_NAME.tar.md5
	cd $RDIR
}
