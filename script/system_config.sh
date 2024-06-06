#! /bin/bash

#
# Copyright (c) 2024 Advanced Micro Devices, Inc.  All rights reserved.
#
# SPDX-License-Identifier: MIT
#

DEV="eth0"
WIDTH=50
HEIGHT=15
IPCONFIG="DHCP"
DEF_IPADDR="192.168.10.10"
DEF_NETMASK="255.255.255.0"
DEF_DNS="1.1.1.1"
NETWORK_CONFIG="/etc/systemd/network/20-wired.network"
BACKTITLE="System Controller Configuration"

OCTET="(25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])"
VALID_IP4_ADDR="^$OCTET\\.$OCTET\\.$OCTET\\.$OCTET$"

#
# Convert an IP address in dotted decimal format into its corresponding
# integer representation.
#
ip_to_int() {
	echo $((($(echo "$1" | cut -d. -f1) << 24) + \
		($(echo "$1" | cut -d. -f2) << 16) + \
		($(echo "$1" | cut -d. -f3) << 8) + \
		$(echo "$1" | cut -d. -f4)))
}

#
# Convert a subnet mask provided as an integer to its corresponding
# dotted decimal format.
#
mask_to_dot_decimal() {
	MASK=0
	for I in {0..31}; do
		if [ $I -lt "$1" ]; then
			MASK=$(((MASK << 1) + 1))
		else
			MASK=$((MASK << 1))
		fi
	done

	printf "%d.%d.%d.%d" \
		 $(("$MASK" >> 24)) $(("$MASK" >> 16 & 0xff)) \
		 $(("$MASK" >> 8 & 0xff)) $(("$MASK" & 0xff))
}

#
# Calculate the CIDR notation (number of leading 1 bits) for a provided
# IP address in dotted decimal format.
#
dot_decimal_to_mask() {
	COUNT=0
	MASK=$(ip_to_int "$1")
	for I in {0..31}; do
		if [ $((MASK & 1)) -eq 1 ]; then
			COUNT=$((COUNT + 1))
		fi

		MASK=$((MASK >> 1))
	done
	echo $COUNT
}

#
# Determine network info needed for a static IP configuration
#
configure_static () {
	#
	# Determine the IP address
	#
	IPADDR=$(ip addr show dev $DEV | grep "inet " | awk '{print $2}' | cut -d/ -f1)
	if [ -z "$IPADDR" ]; then
		IPADDR="$DEF_IPADDR"
	else
		DEF_IPADDR="$IPADDR"
	fi

	if [ -f $NETWORK_CONFIG ]; then
		IPADDR=$(grep "^Address" $NETWORK_CONFIG | sed 's/Address=//' | sed 's/\/.*$//')
		if [ -z "$IPADDR" ]; then
			IPADDR="$DEF_IPADDR"
		fi
	fi

	VALID_ENTRY=""
	while [ -z $VALID_ENTRY ]; do
		IPADDR=$(whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
			--inputbox "IP Address" $HEIGHT $WIDTH $IPADDR 3>&1 1>&2 2>&3)
		if [ $? -eq 1 ]; then
			return
		fi

		if ! [[ $IPADDR =~ $VALID_IP4_ADDR ]]; then
			whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
				--msgbox "Invalid IP Address!" $HEIGHT $WIDTH
		else
			VALID_ENTRY="Yes"
		fi
	done

	#
	# Determine the subnet mask
	#
	NETMASK=$(ip addr show dev $DEV | grep "inet " | awk '{print $2}' | cut -d/ -f2)
	if [ -z "$NETMASK" ]; then
		NETMASK="$DEF_NETMASK"
	else
		NETMASK=$(mask_to_dot_decimal $NETMASK)
		DEF_NETMASK=$NETMASK
	fi

	if [ -f $NETWORK_CONFIG ]; then
		NETMASK=$(grep "^Address" $NETWORK_CONFIG | sed 's/Address=//' | sed 's/.*\///')
		if [ -n "$NETMASK" ]; then
			NETMASK=$(mask_to_dot_decimal "$NETMASK")
		else
			NETMASK="$DEF_NETMASK"
		fi
	fi

	VALID_ENTRY=""
	while [ -z $VALID_ENTRY ]; do
		NETMASK=$(whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
			--inputbox "Subnet Mask" $HEIGHT $WIDTH "$NETMASK" 3>&1 1>&2 2>&3)
		if [ $? -eq 1 ]; then
			return
		fi

		if ! [[ $NETMASK =~ $VALID_IP4_ADDR ]]; then
			whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
				--msgbox "Invalid Subnet Mask!" $HEIGHT $WIDTH
		else
			VALID_ENTRY="Yes"
		fi
	done

	#
	# Determine the gateway address
	#
	GATEWAY=$(ip route | grep default | awk '{print $3}')
	if [ -f $NETWORK_CONFIG ]; then
		GATEWAY=$(grep "^Gateway" $NETWORK_CONFIG | sed 's/Gateway=//')
	fi

	if [ -z "$GATEWAY" ]; then
		IP_INT=$(ip_to_int "$IPADDR")
		MASK_INT=$(ip_to_int "$NETMASK")
		GW_INT=$(((IP_INT & MASK_INT) + 1))
		GATEWAY=$(printf "%d.%d.%d.%d" $((GW_INT >> 24)) $((GW_INT >> 16 & 0xff)) \
			$((GW_INT >> 8 & 0xff)) $((GW_INT & 0xff)))
	fi

	VALID_ENTRY=""
	while [ -z $VALID_ENTRY ]; do
		GATEWAY=$(whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
			--inputbox "Gateway" $HEIGHT $WIDTH "$GATEWAY" 3>&1 1>&2 2>&3)
		if [ $? -eq 1 ]; then
			return
		fi

		if ! [[ $GATEWAY =~ $VALID_IP4_ADDR ]]; then
			whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
				--msgbox "Invalid Gateway!" $HEIGHT $WIDTH
		else
			VALID_ENTRY="Yes"
		fi
	done

	#
	# Determine the DNS address
	#
	DNS=$(grep nameserver /etc/resolv.conf | head -n 1 | awk '{print $2}')
	DEF_DNS=$DNS
	if [ -f $NETWORK_CONFIG ]; then
		DNS=$(grep "^DNS" $NETWORK_CONFIG | sed 's/DNS=//')
		if [ -z "$DNS" ]; then
			DNS=$DEF_DNS
		fi
	fi

	VALID_ENTRY=""
	while [ -z $VALID_ENTRY ]; do
		DNS=$(whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
			--inputbox "DNS" $HEIGHT $WIDTH "$DNS" 3>&1 1>&2 2>&3)
		if [ $? -eq 1 ]; then
			return
		fi

		if ! [[ $DNS =~ $VALID_IP4_ADDR ]]; then
			whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
				--msgbox "Invalid DNS!" $HEIGHT $WIDTH
		else
			VALID_ENTRY="Yes"
		fi
	done

	#
	# Get confirmation
	#
	whiptail --backtitle "$BACKTITLE" --title "Configure Network: Are the settings correct?" \
		--yesno "\n IP Address: $IPADDR \n Subnet Mask: $NETMASK \n Gateway: $GATEWAY \
		\n DNS: $DNS \n" $HEIGHT $WIDTH 3>&1 1>&2 2>&3
	if [ $? -eq 1 ]; then
		return
	fi

	NETMASK=$(dot_decimal_to_mask "$NETMASK")

	#
	# Create the network config file
	#
	cat > $NETWORK_CONFIG <<- EOF
[Match]
Name=$DEV

[Network]
Address=$IPADDR/$NETMASK
Gateway=$GATEWAY
DNS=$DNS
EOF
}

#
# Print the current settings for hostname and network configuration
#
print_current_settings () {
	HOSTNAME=$(hostname)
	IPADDR=$(ip addr show dev $DEV | grep "inet " | awk '{print $2}' | cut -d/ -f1)
	NETMASK=$(ip addr show dev $DEV | grep "inet " | awk '{print $2}' | cut -d/ -f2)
	if [ -n "$NETMASK" ]; then
		NETMASK=$(mask_to_dot_decimal "$NETMASK")
	fi

	GATEWAY=$(ip route | grep default | awk '{print $3}')
	DNS=$(grep nameserver /etc/resolv.conf | awk '{print $2}')
	if [ "$(echo "$DNS" | wc -l)" -gt 1 ]; then
		DNS=""
	fi

	if [ -f $NETWORK_CONFIG ]; then
		if ! grep -q "^DHCP=yes" $NETWORK_CONFIG; then
			IPCONFIG="Static"
		fi
	fi

	MSG="\nHostname: $HOSTNAME\nIP Config: $IPCONFIG\nIP Address: $IPADDR\nSubnet Mask: $NETMASK\nGateway: $GATEWAY\nDNS: $DNS"
	whiptail --backtitle "$BACKTITLE" --title "Current Settings" --msgbox "$MSG" $HEIGHT $WIDTH
}

#
# Change the hostname
#
change_hostname () {
	CUR_HOSTNAME=$(hostname)
	NEW_HOSTNAME=$(whiptail --backtitle "$BACKTITLE" --title "Change Hostname" \
		--inputbox "New Hostname" $HEIGHT $WIDTH "$CUR_HOSTNAME" 3>&1 1>&2 2>&3)
	if [ $? -eq 0 ]; then
		hostnamectl set-hostname "$NEW_HOSTNAME"
	fi
}

#
# Configure the network
#
configure_network () {
	DHCP_SETTING="on"
	STATIC_SETTING="off"
	if [ -f $NETWORK_CONFIG ]; then
		if ! grep -q "^DHCP=yes" $NETWORK_CONFIG; then
			DHCP_SETTING="off"
			STATIC_SETTING="on"
		fi
	fi

	IPCONFIG=$(whiptail --backtitle "$BACKTITLE" --title "Configure Network" \
		--radiolist "Static or DHCP Network configuration?" $HEIGHT $WIDTH 2 \
		DHCP "Dynamic Network config" $DHCP_SETTING \
		Static "Static Network config" $STATIC_SETTING 3>&1 1>&2 2>&3)
	if [ $? -eq 1 ]; then
		return
	fi

	#
	# If DHCP mode is selected, remove the network configuration file (if any)
	# since it is no longer needed.
	#
	if [ "$IPCONFIG" == "DHCP" ]; then
		rm -f $NETWORK_CONFIG
	else
		configure_static
	fi

	#
	# Make the new settings in-effect
	#
	systemctl restart systemd-networkd.service
}

#
# Super user access is required to run this script.
#
if [ $UID != 0 ]; then
	MSG="\n\n\n\n\n   Super user access is required.  Rerun the utility with sudo!"
	whiptail --backtitle "$BACKTITLE" --msgbox "$MSG" $HEIGHT ${#MSG}
	exit
fi

#
# Display the main menu
#
while true; do
	SELECT=$(whiptail --backtitle "$BACKTITLE" --title "$BACKTITLE" \
		--menu --notags "Make a selection:" $HEIGHT $WIDTH 4 \
		"1" "Show Current Settings" \
		"2" "Change Hostname" \
		"3" "Configure Network" \
		"4" "Exit" 3>&2 2>&1 1>&3)
	if [ $? -eq 1 ]; then
		SELECT="4"
	fi

	case $SELECT in
	"1")
		print_current_settings
		;;
	"2")
		change_hostname
		;;
	"3")
		configure_network
		;;
	"4")
		echo "Bye!"
		exit
		;;
	esac
done
