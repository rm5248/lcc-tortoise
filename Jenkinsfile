pipeline {
	agent any

	parameters{
		booleanParam(name: "CLEAN_ALL", defaultValue: false, description: "Clean the entire workspace")
	}

	stages{
		stage("Clean all"){
			when { expression { return params.CLEAN_ALL } }
			steps{
				cleanWs()
			}
		}

		stage("checkout"){
			steps{
				dir('lcc-tortoise'){
					checkout scm
				}
			}
		} /* stage checkout */

		stage("create venv"){
			steps{
				sh '''#!/bin/bash
if [ ! -e venv ]
then
	python3 -m venv venv
	source venv/bin/activate
	pip3 install west
fi
'''
			}
		} /* stage venv*/

		stage("Build SMTC8"){
			steps{
				sh '''#!/bin/bash
source venv/bin/activate
if [ ! -e .west ]
then
	west init -l lcc-tortoise
	west update
	west zephyr-export
	pip3 install -r zephyr/scripts/requirements.txt
fi

cd lcc-tortoise
west update
echo "SB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\\"/var/lib/signing-keys/lcc-smtc8.pem\\"" >> lcc-smtc8/sysbuild.conf
west build -b lcc_smtc8 --sysbuild lcc-smtc8
'''
			} /* steps */
		} /* stage build */


		stage("Archive Switch Decoder"){
			steps{
				sh '''#!/bin/bash

mkdir -p artifacts
rm artifacts/* || true

cp lcc-tortoise/build/lcc-smtc8/zephyr/lcc-smtc8.signed.bin artifacts/lcc-smtc8.bin
cp lcc-tortoise/build/mcuboot/zephyr/lcc-smtc8-bootloader.bin artifacts/lcc-smtc8-bootloader.bin
cp lcc-tortoise/flash-lcc-smtc8.sh artifacts
'''

				zip archive: true, defaultExcludes: false, dir: 'artifacts', exclude: '', glob: 'lcc-smtc8*,flash-lcc-smtc8.sh', overwrite: true, zipFile: 'lcc-smtc8.zip'
				archiveArtifacts artifacts:'artifacts/lcc-smtc8.bin,artifacts/lcc-smtc8-bootloader.bin'
			}
		} /* stage archive */

		stage("Build LCC-Link"){
			steps{
				sh '''#!/bin/bash
source venv/bin/activate
if [ ! -e .west ]
then
	west init -l lcc-tortoise
	west update
	west zephyr-export
	pip3 install -r zephyr/scripts/requirements.txt
fi

cd lcc-tortoise
west update
echo "SB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\\"/var/lib/signing-keys/lcc-link.pem\\"" >> lcc-link/sysbuild.conf
west build --build-dir build-lcclink -b lcc_link --sysbuild lcc-link
'''
			}
		} /* stage build lcc link */

		stage("Archive LCC Link"){
			steps{
				sh '''#!/bin/bash
mkdir -p artifacts
rm artifacts/* || true

cp lcc-tortoise/build-lcclink/lcc-link/zephyr/lcc-link.signed.bin artifacts/lcc-link.bin
cp lcc-tortoise/build-lcclink/mcuboot/zephyr/lcc-link-bootloader.bin artifacts/lcc-link-bootloader.bin
cp lcc-tortoise/flash-lcc-link.sh artifacts
'''

				zip archive: true, defaultExcludes: false, dir: 'artifacts', exclude: '', glob: 'lcc-link*,flash-lcc-link.sh', overwrite: true, zipFile: 'lcc-link.zip'
				archiveArtifacts artifacts:'artifacts/lcc-link.bin,artifacts/lcc-link-bootloader.bin'
			}
		} /* stage archive */

		stage("Build Servo16Plus"){
			steps{
				sh '''#!/bin/bash
source venv/bin/activate
if [ ! -e .west ]
then
	west init -l lcc-tortoise
	west update
	west zephyr-export
	pip3 install -r zephyr/scripts/requirements.txt
fi

cd lcc-tortoise
west update
echo "SB_CONFIG_BOOT_SIGNATURE_KEY_FILE=\\"/var/lib/signing-keys/lcc-servo16plus.pem\\"" >> lcc-servo16-plus/sysbuild.conf
west build --build-dir build-lccservo16 -b servo-tc16 --sysbuild lcc-servo16-plus
'''
			}
		} /* stage build lcc servo 16 plus */

		stage("Archive LCC servo 16 plus"){
			steps{
				sh '''#!/bin/bash
mkdir -p artifacts
rm artifacts/* || true

cp lcc-tortoise/build-lccservo16/lcc-servo16-plus/zephyr/lcc-servo16-plus.signed.bin artifacts/lcc-servo16-plus.bin
cp lcc-tortoise/build-lccservo16/mcuboot/zephyr/lcc-servo16-plus-bootloader.bin artifacts/lcc-servo16-plus-bootloader.bin
cp lcc-tortoise/flash-lcc-servo16-plus.sh artifacts
'''

				zip archive: true, defaultExcludes: false, dir: 'artifacts', exclude: '', glob: 'lcc-servo*,flash-lcc-servo16-plus.sh', overwrite: true, zipFile: 'lcc-servo16plus.zip'
				archiveArtifacts artifacts:'artifacts/lcc-servo16-plus.bin,artifacts/lcc-servo16-plus-bootloader.bin'
			}
		} /* stage archive */

	} /* end stages */
}
