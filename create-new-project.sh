#!/bin/bash

echo "Enter board name:"
read board_name
echo "Enter project name:"
read project_name

mkdir -p boards/$board_name/support
cp boards/lcc_smtc8/lcc_smtc8.dts boards/$board_name/$board_name.dts
cp boards/lcc_smtc8/board.yml boards/$board_name/
cp boards/lcc_smtc8/lcc_smtc8_defconfig boards/$board_name/${board_name}_defconfig
cp boards/lcc_smtc8/lcc_smtc8.yaml boards/$board_name/$board_name.yaml
cp boards/lcc_smtc8/board.cmake boards/$board_name/board.cmake
cp boards/lcc_smtc8/support/openocd.cfg boards/$board_name/support
cp boards/lcc_smtc8/Kconfig.lcc_smtc8 boards/$board_name/Kconfig.$board_name

sed -i "s/model = .*/model = \"${board_name}\";/g" boards/$board_name/$board_name.dts
sed -i "s/name: lcc_smtc8/name: ${board_name}/g" boards/$board_name/board.yml
sed -i "s/identifier: lcc_smtc8/identifier: ${board_name}/g" boards/$board_name/$board_name.yaml
sed -i "s/config BOARD_LCC_SMTC8/config BOARD_${board_name^^}/g" boards/$board_name/Kconfig.*

mkdir -p $project_name

openssl genrsa -out $project_name/$project_name.pem 2048
cp lcc-smtc8/prj.conf $project_name/prj.conf
cp lcc-smtc8/sysbuild.conf $project_name/sysbuild.conf
sed -i "s/lcc-smtc8/${project_name}/g" $project_name/prj.conf
sed -i "s/lcc-smtc8/${project_name}/g" $project_name/sysbuild.conf

mkdir -p ${project_name}/sysbuild
cp lcc-smtc8/sysbuild/lcc-smtc8.conf ${project_name}/sysbuild/${project_name}.conf
cp lcc-smtc8/sysbuild/mcuboot.conf ${project_name}/sysbuild/mcuboot.conf
sed -i "s/lcc-smtc8/${project_name}/g" $project_name/sysbuild/${project_name}.conf
sed -i "s/CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=.*/CONFIG_MCUBOOT_IMGTOOL_SIGN_VERSION=\"1.0.0\"/g" $project_name/sysbuild/${project_name}.conf
sed -i "s/lcc-smtc8-bootloader/${project_name}-bootloader/g" $project_name/sysbuild/mcuboot.conf

cp flash-lcc-smtc8.sh flash-$project_name.sh
sed -i "s/lcc-smtc8/$project_name/g" flash-$project_name.sh

mkdir -p $project_name/src
cp template-files/CMakeLists.txt $project_name
cp template-files/src/* $project_name/src
