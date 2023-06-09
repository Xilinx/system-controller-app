#! /bin/sh

#
# Configure syslogd to log sc_app messages into a separate file.
#
/bin/grep -e '^local3' /etc/syslog.conf
if [ $? -ne 0 ]; then
    /bin/echo 'local3.*  /var/log/local3.log' >> /etc/syslog.conf
    /etc/init.d/syslog restart
fi

#
# If dfx-mgrd is used to install the board package, wait until that
# operation is complete before starting sc_appd.
#
if [ "`/usr/bin/pgrep dfx-mgrd`" != "" ]; then
    COUNT=5
    while [ $COUNT != 0 ] && [ ! -S "/tmp/dfx-mgrd.socket" ]; do
        /bin/sleep 1
        COUNT=`expr $COUNT - 1`
    done

    EEPROM=`/bin/ls /sys/bus/i2c/devices/1-0054/eeprom 2> /dev/null`
    BOARD=`/usr/sbin/ipmi-fru --fru-file=$EEPROM --interpret-oem-data | /usr/bin/awk -F": " '/FRU Board Product/ { print tolower ($2) }'`
    REVISION=`/usr/sbin/ipmi-fru --fru-file=$EEPROM --interpret-oem-data | /usr/bin/awk -F": " '/FRU Board Custom/ { print tolower ($2); exit }'`
    PACKAGE=`/usr/bin/dfx-mgr-client -listPackage | /bin/grep "$BOARD-$REVISION"`
    if [ "$PACKAGE" == "" ]; then
        /bin/echo "WARNING: dfx-mgr has no package for $BOARD-$REVISION board"
    fi
fi

#
# Make sure xsdb connection to Versal is available before starting sc_appd.
#
source /etc/profile.d/xsdb-variables.sh

# Configure JTAG mux for access from SC
gpioset `gpiofind SYSCTLR_JTAG_S0`=0
gpioset `gpiofind SYSCTLR_JTAG_S1`=0

COUNT=10
XSDB_OUT=`/usr/local/xilinx_vitis/xsdb -eval 'connect; targets -set -nocase -filter {name =~ "*Versal*"}' 2>&1`
while [ "$XSDB_OUT" != "" -a $COUNT -ne 0 ]; do
    echo -n "." > /dev/ttyPS0
    sleep 1
    COUNT=`expr $COUNT - 1`
    XSDB_OUT=`/usr/local/xilinx_vitis/xsdb -eval 'connect; targets -set -nocase -filter {name =~ "*Versal*"}' 2>&1`
done

if [ $COUNT -eq 0 ]; then
    echo "ERROR: xsdb access is not available!"
    exit -1
fi

# Reset the JTAG mux setting
gpioget `gpiofind SYSCTLR_JTAG_S0` > /dev/null
gpioget `gpiofind SYSCTLR_JTAG_S1` > /dev/null

/usr/bin/sc_appd &
