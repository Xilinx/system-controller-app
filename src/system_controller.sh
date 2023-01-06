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

/usr/bin/sc_appd &
