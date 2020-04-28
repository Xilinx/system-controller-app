#! /bin/sh

PNAME=`basename $0`

for pid in `pidof -x ${PNAME}`; do
	if [ $pid != $$ ]; then
		echo "$0: process ${PNAME} is already running!"
		exit -1
	fi
done

while [ 1 ]; do
	COUNT=5

	# MIO37 == "DC_SYS_CTRL4"
	MIO37=`gpiomon --format="%e" --num-events=1 gpiochip0 11`

	# Falling edge (MIO37 == 0), Rising edge (MIO37 == 1)
	/usr/bin/sc_app -c workaround -t vccaux -v ${MIO37} > /dev/null

	# In case sc_app is busy, try few times before giving up
	while [ $? -ne 0 -a ${COUNT} -ne 0 ]; do
		/usr/bin/sc_app -c workaround -t vccaux -v ${MIO37} > /dev/null
		COUNT=`expr ${COUNT} - 1`
	done
done
