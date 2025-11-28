S="./"	# source code dir
KEY_DIR="./keys"
KEYNAME="dev"
PREBUILT="./prebuilt"
FW="./fw"
UBOOT_CONFIG="rtd1619b_bleedingedge"

# prepare keys
prepare_keys() {
	echo "Preparing sign keys..."
	rm -rf ${KEY_DIR}
	cp -a ${PREBUILT}/keys ${KEY_DIR}
	dtc -I dtb -O dts -o ${S}/arch/arm/dts/sign.dtsi ${KEY_DIR}/dummy.dtb
	sed -i "/dts-v1/d" ${S}/arch/arm/dts/sign.dtsi
	sed -i "s/signature/signature: signature/" ${S}/arch/arm/dts/sign.dtsi
	sed -i "s/key-dev/key_dev: key-dev/" ${S}/arch/arm/dts/sign.dtsi
	sed -i "s/conf/invalid/" ${S}/arch/arm/dts/sign.dtsi
}

sign_spl() {
	if [ -d "${KEY_DIR}" ]; then
		echo "Signing SPL..."
		openssl dgst -sha256 -binary ./spl/u-boot-spl.bin_pad > ${PREBUILT}/u-boot-spl.sha
		openssl pkeyutl -inkey ${KEY_DIR}/dev.key -sign -in ${PREBUILT}/u-boot-spl.sha -out ${PREBUILT}/u-boot-spl.sig
		objcopy -I binary -O binary --reverse-bytes=256 ${PREBUILT}/u-boot-spl.sig ${PREBUILT}/u-boot-spl.sig
	fi
}

create_uboot_img() {
	echo "Create u-boot images..."
	cp ./spl/u-boot-spl.bin_pad ${PREBUILT}/
	cp ./u-boot.img ${PREBUILT}/
	cc -E -nostdinc -undef -D__DTS__ -x assembler-with-cpp \
		-o ${PREBUILT}/rtd1619b_emmc_4gb.pp ${PREBUILT}/rtd1619b_emmc_lpddr4_4gb.dts
	dtc -I dts -O dtb -o ${PREBUILT}/rtd1619b_emmc_4gb.dtb ${PREBUILT}/rtd1619b_emmc_4gb.pp
	tools/binman/binman build --update-fdt -I ./${PREBUILT} --dt ./${PREBUILT}/rtd1619b_emmc_4gb.dtb -O ./
}

BUILD_FLAGS=(
	CROSS_COMPILE=aarch64-linux-gnu-
	DEV_TREE_SEPARATE=1
	HOSTSTRIP=true
) 

if [ ! -d ${PREBUILT} ]; then
	echo "No fw or prebuilt artifacts found"
	exit 1
fi

prepare_keys

make "${BUILD_FLAGS[@]}" clean
make "${BUILD_FLAGS[@]}" "${UBOOT_CONFIG}_defconfig"
make "${BUILD_FLAGS[@]}" all -j$(nproc)

sign_spl
create_uboot_img

cp -vf u-boot.itb u-boot.bin-rtd1619b_emmc
cp -vf bind_4gb.bin rtd1619b_emmc_bind_4gb.bin
