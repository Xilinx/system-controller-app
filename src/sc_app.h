#ifndef SC_APP_H_
#define SC_APP_H_

#define STRLEN_MAX	32
#define	ITEMS_MAX	20
#define	SYSCMD_MAX	128
#define	MUX_MAX		4

/*
 * I2C Buses
 */
typedef struct {
	char	Name[STRLEN_MAX];
} I2C_Bus_t;

typedef struct I2C_Buses {
	int	Numbers;
	I2C_Bus_t	I2C_Bus[ITEMS_MAX];
} I2C_Buses_t;

typedef struct {
	int	Mux_Levels;
	int	Bus_Id;
	int	Mux_Route[MUX_MAX][2];
} I2C_Route_t;

/*
 * Boot Modes
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Value;
} BootMode_t;

typedef struct BootModes {
	int	Numbers;
	char	Mode_Lines[4][SYSCMD_MAX];
	BootMode_t	BootMode[ITEMS_MAX];
} BootModes_t;

/*
 * Clocks
 */
typedef enum {
	Oscillator,
	I2C,
} Clock_Type;

typedef struct {
	char	Name[STRLEN_MAX];
	unsigned int	Default_Freq;
	unsigned int	Upper_Freq;
	unsigned int	Lower_Freq;
	Clock_Type	Type;
	char	Sysfs_Path[SYSCMD_MAX];
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Clock_t;

typedef struct Clocks {
	int	Numbers;
	Clock_t	Clock[ITEMS_MAX];
} Clocks_t;

/*
 * INA226
 */
typedef struct {
	char	Name[STRLEN_MAX];
	char	Sysfs_Path[SYSCMD_MAX];
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Ina226_t;

typedef struct Ina226s {
	int	Numbers;
	Ina226_t	Ina226[ITEMS_MAX];
} Ina226s_t;

/*
 * Voltages
 */
typedef struct {
	char	Name[STRLEN_MAX];
	unsigned int	Default_Volt;
	unsigned int	Upper_Volt;
	unsigned int	Lower_Volt;
	int		Page_Select;
	I2C_Route_t	I2C_Route;
	int	I2C_Address;
} Voltage_t;

typedef struct Voltages {
	int	Numbers;
	Voltage_t	Voltage[ITEMS_MAX];
} Voltages_t;

/*
 * Workarounds
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	Arg_Needed;
	int	(*Plat_Workaround_Op)(void *);
} Workaround_t;

typedef struct Workarounds {
	int	Numbers;
	Workaround_t	Workaround[ITEMS_MAX];
} Workarounds_t;

/*
 * Board Interface Tests (BITs)
 */
typedef struct {
	char	Name[STRLEN_MAX];
	int	(*Plat_BIT_Op)(void);
} BIT_t;

typedef struct BITs {
	int	Numbers;
	BIT_t	BIT[ITEMS_MAX];
} BITs_t;

#endif	/* SC_APP_H_ */

