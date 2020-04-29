#! /bin/sh

PNAME=`basename $0`

for pid in `pidof -x ${PNAME}`; do
	if [ $pid != $$ ]; then
		echo "$0: process ${PNAME} is already running!"
		exit -1
	fi
done

while [ 1 ]; do
	# MIO37 == "DC_SYS_CTRL4"
	MIO37=`gpiomon --format="%e" --num-events=1 gpiochip0 11`

	# Falling edge (MIO37 == 0), Rising edge (MIO37 == 1)
	/usr/bin/sc_app -c workaround -t vccaux -v ${MIO37} > /dev/null

	# In case sc_app is busy, try until it succeeds
	while [ $? -ne 0 ]; do
		/usr/bin/sc_app -c workaround -t vccaux -v ${MIO37} > /dev/null
	done
done
