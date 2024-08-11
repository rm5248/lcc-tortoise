pipeline {
	agent any

	stages{
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
if [ ! -e .west.yml ]
then
	west init -l lcc-tortoise
	west update
fi

cd lcc-tortoise
west build -b lcc_tortoise lcc-tortoise
'''
			}
		} /* stage build */

		stage("Archive"){
			steps{
				archiveArtifacts artifacts:'build/zephyr/zephyr.signed.bin,build/zephyr/zephyr.signed.hex, lcc-tortoise/bootloader-prebuild/bootloader-rm.hex'
			}
		} /* stage archive */

	} /* end stages */
}
