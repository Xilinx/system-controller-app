#! /bin/sh

PNAME=`basename $0`
TRIGGERED=0

for pid in `pidof -x ${PNAME}`; do
	if [ $pid != $$ ]; then
		echo "$0: process ${PNAME} is already running!"
		exit -1
	fi
done

echo "$0: Starting poll for MIO37"
while [ 1 ]; do
	# MIO37="$(gpioget `gpiofind "DC_SYS_CTRL4"`)"
	MIO37=`gpioget gpiochip0 11`
	if [ ${MIO37} == 0 ]; then
		if [ ${TRIGGERED} == 1 ]; then
			echo "$0: falling edge is detecded"
			TRIGGERED=0
		fi
	elif [ ${MIO37} == 1 ]; then
		if [ ${TRIGGERED} == 0 ]; then
			echo "$0: rising edge is detected"
			TRIGGERED=1
			/usr/bin/sc_app -c workaround
		fi
	fi
done
