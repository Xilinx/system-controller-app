# system-controller-app

	Usage:

	sc_app -c <command> [-t <target> [-v <value>]]

	<command> - 
		version - version and build information
		board - name of the board
		reset - apply power-on-reset

		listfeature - list the supported features for this board

		listeeprom - list the supported EEPROM targets
		geteeprom - get the content of <target> EEPROM for either <value>:
			    'summary', 'all', 'common', 'board', or 'multirecord'

		listtemp - list the supported temperature sensor targets
		gettemp - get the reading of <target> temperature sensor

		listbootmode - list the supported boot mode targets
		getbootmode - get boot mode, with optional <value> of 'alternate'
		setbootmode - set boot mode to <target>, with optional <value> of 'alternate'

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
		getcalpower - get the voltage, current, and power of custom calibrated <target>
		getINA226 - get the content of <target> registers
		setINA226 - set the 'Configuration', 'Calibration', 'Mask/Enable', and
			    'Alert Limit' registers of <target> to <value>

		listpowerdomain - list the supported power domain targets
		powerdomain - get the power used by <target> power domain

		listworkaround - list the applicable workaround targets
		workaround - apply <target> workaround (may requires <value>)

		listBIT - list the supported Board Interface Test targets
		BIT - run BIT target

		listddr - list the supported DDR DIMM targets
		getddr - get <target> DDR DIMM information for either <value>:
			 'spd' or 'temp'

		listgpio - list the supported gpio line targets
		getgpio - get the state of <target> gpio

		listioexp - list the supported IO expander targets
		getioexp - get the state of <target> IO expander for either <value>:
			   'all', 'input', or 'output'
		setdirioexp - set direction of <target> IO expander to <value>
		setoutioexp - set output of <target> IO expander to <value>
		restoreioexp - restore IO expander to default values

		listSFP - list the plugged SFP transceiver targets
		getSFP - get the transceiver information of <target> SFP

		listQSFP - list the plugged QSFP transceiver targets
		getQSFP - get the transceiver information of <target> QSFP

		listEBM - list the plugged EBM daughter card targets
		getEBM - get the content of EEPROM on <target> EBM card for either <value>:
			 'all', 'common', 'board', or 'multirecord'

		listFMC - list the plugged FMCs
		getFMC - get the content of EEPROM on <target> FMC for either <value>:
			 'all', 'common', 'board', or 'multirecord'
