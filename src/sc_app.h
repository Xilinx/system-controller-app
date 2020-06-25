#ifndef SC_APP_H_
#define SC_APP_H_

#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define STRLEN_MAX	32
#define ITEMS_MAX	20
#define SYSCMD_MAX	1024
#define MUX_MAX		4

#define APPDIR          "/home/root/.sc_app"
#define SILICONFILE     APPDIR"/silicon"

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
	char	Part_Name[STRLEN_MAX];
	unsigned int	Default_Volt;
	unsigned int	Upper_Volt;
	unsigned int	Lower_Volt;
	int		Page_Select;
	char	I2C_Bus[STRLEN_MAX];
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
	char	TCL_File[SYSCMD_MAX];
	int	(*Plat_BIT_Op)(void *);
} BIT_t;

typedef struct BITs {
	int	Numbers;
	BIT_t	BIT[ITEMS_MAX];
} BITs_t;

/*
 * DDR serial presence detect (SPD)
 */
struct i2c_info {
	__u16 Bus_addr;
	__u8  Reg_addr;
	__u16 Read_len;
};

struct ddr_dimm1 {
	char I2C_Bus[STRLEN_MAX];
	struct i2c_info Spd;
	struct i2c_info Therm;
};

/*
 * GPIO line-number, sc_app-display-name pairs
 */
struct Gpio_line_name {
	const char *Name;
	int Line;
};

#define I2C_READ(FD, Address, Len, Out, In) \
{ \
	struct i2c_msg Msgs[2]; \
	struct i2c_rdwr_ioctl_data Msgset[1]; \
	Msgs[0].addr = (Address); \
	Msgs[0].flags = 0; \
	Msgs[0].len = 1; \
	Msgs[0].buf = (Out); \
	Msgs[1].addr = (Address); \
	Msgs[1].flags = (I2C_M_RD | I2C_M_NOSTART); \
	Msgs[1].len = (Len); \
	Msgs[1].buf = (In); \
	Msgset[0].msgs = Msgs; \
	Msgset[0].nmsgs = 2; \
	if (ioctl((FD), I2C_RDWR, &Msgset) < 0) { \
		printf("ERROR: unable to read from I2C device 0x%x\n", (Address)); \
		return -1; \
	} \
}

#define I2C_WRITE(FD, Address, Len, Out) \
{ \
	if (ioctl((FD), I2C_SLAVE_FORCE, (Address)) < 0) { \
		printf("ERROR: unable to access I2C device 0x%x\n", (Address)); \
		return -1; \
	} \
	if (write((FD), (Out), (Len)) != (Len)) { \
		printf("ERROR: unable to write to I2C device 0x%x\n", (Address)); \
		return -1; \
	} \
}

#define PMBUS_VOUT_MODE		0x20
#define PMBUS_READ_VOUT		0x8B

#endif	/* SC_APP_H_ */

