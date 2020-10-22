# system-controller

	Usage:

	sc_app -c <command> [-t <target> [-v <value>]]

	<command> - 
		version - version and compatibility information
		listbootmode - lists the supported boot mode targets
		setbootmode - set boot mode to <target>
		reset - apply power-on-reset
		eeprom - lists the content of EEPROM
		temperature - get the board temperature
		listclock - lists the supported clock targets
		getclock - get the frequency of <target>
		setclock - set <target> to <value> frequency
		setbootclock - set <target> to <value> frequency at boot time
		restoreclock - restore <target> to default value
		listvoltage - lists the supported voltage targets
		getvoltage - get the voltage of <target>
		listpower - lists the supported power targets
		getpower - get the voltage, current, and power of <target>
		listpowerdomain - lists the supported power domain targets
		powerdomain - get the power used by <target> power domain
		listworkaround - lists the applicable workaround targets
		workaround - apply <target> workaround (may requires <value>)
		listBIT - lists the supported Board Interface Test targets
		BIT - run BIT target
		ddr - get DDR DIMM information: <target> is either 'spd' or 'temp'
		listgpio - lists the supported gpio lines
		getgpio - get the state of <target> gpio
		getioexp - get IO expander <target> of either 'all', 'input', or 'output'
		setioexp - set IO expander <target> of either 'direction' or 'output' to <value>
		restoreioexp - restore IO expander to default values
		listSFP - lists the supported SFP connectors
		getSFP - get the connector information of <target> SFP
		getpwmSFP - get the power mode value of <target> SFP
		setpwmSFP - set the power mode value of <target> SFP to <value>
		listQSFP - lists the supported QSFP connectors
		getQSFP - get the connector information of <target> QSFP
		getpwmQSFP - get the power mode value of <target> QSFP
		setpwmQSFP - set the power mode value of <target> QSFP to <value>
		getpwmoQSFP - get the power mode override value of <target> QSFP
		setpwmoQSFP - set the power mode override value of <target> QSFP to <value>
		getEBM - get the content of EEPROM from EBM card from either <target>:
			 'all', 'common', 'board', or 'multirecord'
