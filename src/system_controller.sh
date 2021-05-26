#! /bin/sh

#
# Configure syslogd to log sc_app messages into a separate file.
#
/bin/grep -e '^local3' /etc/syslog.conf
if [ $? -ne 0 ]; then
    /bin/echo 'local3.*  /var/log/local3.log' >> /etc/syslog.conf
    /etc/init.d/syslog restart
fi

/usr/bin/sc_appd &
