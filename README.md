# system-controller

	Usage:

	sc_app -c <command> [-t <target> [-v <value>]]

	<command> - 
		version - version and compatibility information
		listbootmode - list the supported boot mode targets
		setbootmode - set boot mode to <target>
		reset - apply power-on-reset
		eeprom - list the selected content of on-board EEPROM
		geteeprom - get the content of on-board EEPROM from either <target>:
			    'all', 'common', 'board', or 'multirecord'
		temperature - get the board temperature
		listclock - list the supported clock targets
		getclock - get the frequency of <target>
		setclock - set <target> to <value> frequency
		setbootclock - set <target> to <value> frequency at boot time
		restoreclock - restore <target> to default value
		listvoltage - list the supported voltage targets
		getvoltage - get the voltage of <target>, with optional <value> of 'all'
		setvoltage - set <target> to <value> volts
		setbootvoltage - set <target> to <value> volts at boot time
		restorevoltage - restore <target> to default value
		listpower - list the supported power targets
		getpower - get the voltage, current, and power of <target>
		listpowerdomain - list the supported power domain targets
		powerdomain - get the power used by <target> power domain
		listworkaround - list the applicable workaround targets
		workaround - apply <target> workaround (may requires <value>)
		listBIT - list the supported Board Interface Test targets
		BIT - run BIT target
		ddr - get DDR DIMM information: <target> is either 'spd' or 'temp'
		listgpio - list the supported gpio lines
		getgpio - get the state of <target> gpio
		getioexp - get IO expander <target> of either 'all', 'input', or 'output'
		setioexp - set IO expander <target> of either 'direction' or 'output' to <value>
		restoreioexp - restore IO expander to default values
		listSFP - list the supported SFP connectors
		getSFP - get the connector information of <target> SFP
		getpwmSFP - get the power mode value of <target> SFP
		setpwmSFP - set the power mode value of <target> SFP to <value>
		listQSFP - list the supported QSFP connectors
		getQSFP - get the connector information of <target> QSFP
		getpwmQSFP - get the power mode value of <target> QSFP
		setpwmQSFP - set the power mode value of <target> QSFP to <value>
		getpwmoQSFP - get the power mode override value of <target> QSFP
		setpwmoQSFP - set the power mode override value of <target> QSFP to <value>
		getEBM - get the content of EEPROM from EBM card from either <target>:
			 'all', 'common', 'board', or 'multirecord'
		listFMC - list the plugged FMCs
		getFMC - get the content of EEPROM on FMC from a plugged <target>.  The <value>
			 should be either: 'all', 'common', 'board', or 'multirecord'
