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
west build -b lcc_smtc8 --sysbuild lcc-smtc8
'''
			}
		} /* stage build */


		stage("Archive Switch Decoder"){
			steps{
				sh '''#!/bin/bash

mkdir -p artifacts
rm artifacts/* || true

cp lcc-tortoise/build/lcc-smtc8/zephyr/lcc-smtc8.signed.bin artifacts/lcc-smtc8.bin
cp lcc-tortoise/build/mcuboot/zephyr/zephyr.bin artifacts/lcc-smtc8-bootloader.bin
'''

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
west build --build-dir build-lcclink -b lcc_link lcc-link
'''
			}
		} /* stage build lcc link */

		stage("Archive LCC Link"){
			steps{
				sh '''#!/bin/bash

cp lcc-tortoise/build-lcclink/zephyr/zephyr.bin artifacts/lcc-link.bin
'''

				archiveArtifacts artifacts:'artifacts/lcc-link.bin'
			}
		} /* stage archive */

	} /* end stages */
}
