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

		stage("Build"){
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
west build -b lcc_tortoise --sysbuild lcc-tortoise
'''
			}
		} /* stage build */

		stage("Archive"){
			steps{
				sh '''#!/bin/bash

mkdir -p artifacts
rm artifacts/* || true

cp lcc-tortoise/build/lcc-tortoise/zephyr/zephyr.signed.bin artifacts/lcc-tortoise-8.bin
cp lcc-tortoise/build/mcuboot/zephyr/zephyr.bin artifacts/lcc-tortoise-8-bootloader.bin
'''

				archiveArtifacts artifacts:'artifacts/lcc-tortoise-8.bin,artifacts/lcc-tortoise-8-bootloader.bin'
			}
		} /* stage archive */

	} /* end stages */
}
