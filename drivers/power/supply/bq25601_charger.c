/*
 * Driver for the TI BQ25601 charger.
 * Author: Mark A. Greer <mgreer@animalcreek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/usb/phy.h>
#include <uapi/linux/usb/charger.h>

#define BQ25601_BATTERY_NAME				"sc27xx-fgu"

/* Register 00h */
#define BQ2560X_REG_00			0x00
#define REG00_ENHIZ_MASK		GENMASK(7, 7)
#define REG00_ENHIZ_SHIFT		7
#define	REG00_HIZ_ENABLE		1
#define	REG00_HIZ_DISABLE		0

#define	REG00_STAT_CTRL_MASK		GENMASK(6, 5)
#define REG00_STAT_CTRL_SHIFT		5
#define	REG00_STAT_CTRL_STAT		0
#define	REG00_STAT_CTRL_ICHG		1
#define	REG00_STAT_CTRL_IINDPM		2
#define	REG00_STAT_CTRL_DISABLE		3

#define REG00_IINLIM_MASK		GENMASK(4, 0)
#define REG00_IINLIM_SHIFT		0
#define	REG00_IINLIM_LSB		100
#define	REG00_IINLIM_BASE		100

/* Register 01h */
#define BQ2560X_REG_01			0x01
#define REG01_PFM_DIS_MASK		GENMASK(7, 7)
#define	REG01_PFM_DIS_SHIFT		7
#define	REG01_PFM_ENABLE		0
#define	REG01_PFM_DISABLE		1

#define REG01_WDT_RESET_MASK		GENMASK(6, 6)
#define REG01_WDT_RESET_SHIFT		6
#define REG01_WDT_RESET			1

#define	REG01_OTG_CONFIG_MASK		GENMASK(5, 5)
#define	REG01_OTG_CONFIG_SHIFT		5
#define	REG01_OTG_ENABLE		1
#define	REG01_OTG_DISABLE		0

#define REG01_CHG_CONFIG_MASK		GENMASK(4, 4)
#define REG01_CHG_CONFIG_SHIFT		4
#define REG01_CHG_DISABLE		0
#define REG01_CHG_ENABLE		1

#define REG01_SYS_MINV_MASK		GENMASK(3, 1)
#define REG01_SYS_MINV_SHIFT		1

#define	REG01_MIN_VBAT_SEL_MASK		GENMASK(0, 0)
#define	REG01_MIN_VBAT_SEL_SHIFT	0
#define	REG01_MIN_VBAT_2P8V		0
#define	REG01_MIN_VBAT_2P5V		1

/* Register 0x02*/
#define BQ2560X_REG_02			0x02
#define	REG02_BOOST_LIM_MASK		GENMASK(7, 7)
#define	REG02_BOOST_LIM_SHIFT		7
#define	REG02_BOOST_LIM_0P5A		0
#define	REG02_BOOST_LIM_1P2A		1

#define	REG02_Q1_FULLON_MASK		GENMASK(6, 6)
#define	REG02_Q1_FULLON_SHIFT		6
#define	REG02_Q1_FULLON_ENABLE		1
#define	REG02_Q1_FULLON_DISABLE		0

#define REG02_ICHG_MASK			GENMASK(5, 0)
#define REG02_ICHG_SHIFT		0
#define REG02_ICHG_BASE			0
#define REG02_ICHG_MAX			3000
#define REG02_ICHG_LSB			60

/* Register 0x03*/
#define BQ2560X_REG_03			0x03
#define REG03_IPRECHG_MASK		GENMASK(7, 4)
#define REG03_IPRECHG_SHIFT		4
#define REG03_IPRECHG_BASE		60
#define REG03_IPRECHG_LSB		60

#define REG03_ITERM_MASK		GENMASK(3, 0)
#define REG03_ITERM_SHIFT		0
#define REG03_ITERM_BASE		60
#define REG03_ITERM_LSB			60
#define REG03_ITERM_DEFAULT		120

/* Register 0x04*/
#define BQ2560X_REG_04			0x04
#define REG04_VREG_MASK			GENMASK(7, 3)
#define REG04_VREG_SHIFT		3
#define REG04_VREG_BASE			3856
#define REG04_VREG_MAX			4624
#define REG04_VREG_LSB			32
#define REG04_VREG_EXCE_VOLTAGE	4352
#define REG04_VREG_EXCE_CODE	0X0F



#define	REG04_TOPOFF_TIMER_MASK		GENMASK(2, 1)
#define	REG04_TOPOFF_TIMER_SHIFT	1
#define	REG04_TOPOFF_TIMER_DISABLE	0
#define	REG04_TOPOFF_TIMER_15M		1
#define	REG04_TOPOFF_TIMER_30M		2
#define	REG04_TOPOFF_TIMER_45M		3

#define REG04_VRECHG_MASK		GENMASK(0, 0)
#define REG04_VRECHG_SHIFT		0
#define REG04_VRECHG_100MV		0
#define REG04_VRECHG_200MV		1

/* Register 0x05*/
#define BQ2560X_REG_05			0x05
#define REG05_EN_TERM_MASK		GENMASK(7, 7)
#define REG05_EN_TERM_SHIFT		7
#define REG05_TERM_ENABLE		1
#define REG05_TERM_DISABLE		0

#define REG05_WDT_MASK			GENMASK(5, 4)
#define REG05_WDT_SHIFT			4
#define REG05_WDT_DISABLE		0
#define REG05_WDT_40S			1
#define REG05_WDT_80S			2
#define REG05_WDT_160S			3
#define REG05_WDT_BASE			0
#define REG05_WDT_LSB			40

#define REG05_EN_TIMER_MASK		GENMASK(3, 3)
#define REG05_EN_TIMER_SHIFT		3
#define REG05_CHG_TIMER_ENABLE		1
#define REG05_CHG_TIMER_DISABLE		0

#define REG05_CHG_TIMER_MASK		GENMASK(2, 2)
#define REG05_CHG_TIMER_SHIFT		2
#define REG05_CHG_TIMER_5HOURS		0
#define REG05_CHG_TIMER_10HOURS		1

#define	REG05_TREG_MASK			GENMASK(1, 1)
#define	REG05_TREG_SHIFT		1
#define	REG05_TREG_90C			0
#define	REG05_TREG_110C			1

#define REG05_JEITA_ISET_MASK		GENMASK(0, 0)
#define REG05_JEITA_ISET_SHIFT		0
#define REG05_JEITA_ISET_50PCT		0
#define REG05_JEITA_ISET_20PCT		1

/* Register 0x06*/
#define BQ2560X_REG_06			0x06
#define	REG06_OVP_MASK			GENMASK(7, 6)
#define	REG06_OVP_SHIFT			0x6
#define	REG06_OVP_5P5V			0
#define	REG06_OVP_6P2V			1
#define	REG06_OVP_10P5V			2
#define	REG06_OVP_14P3V			3

#define	REG06_BOOSTV_MASK		GENMASK(5, 4)
#define	REG06_BOOSTV_SHIFT		4
#define	REG06_BOOSTV_4P85V		0
#define	REG06_BOOSTV_5V			1
#define	REG06_BOOSTV_5P15V		2
#define	REG06_BOOSTV_5P3V		3

#define	REG06_VINDPM_MASK		GENMASK(3, 0)
#define	REG06_VINDPM_SHIFT		0
#define	REG06_VINDPM_BASE		3900
#define	REG06_VINDPM_LSB		100
#define	REG06_VINDPM_TRY	4400
#define	REG06_VINDPM_NORMAL		4600
#define	REG06_VINDPM_MAX		5400

/* Register 0x07*/
#define BQ2560X_REG_07			0x07
#define REG07_FORCE_DPDM_MASK		GENMASK(7, 7)
#define REG07_FORCE_DPDM_SHIFT		7
#define REG07_FORCE_DPDM		1

#define REG07_TMR2X_EN_MASK		GENMASK(6, 6)
#define REG07_TMR2X_EN_SHIFT		6
#define REG07_TMR2X_ENABLE		1
#define REG07_TMR2X_DISABLE		0

#define REG07_BATFET_DIS_MASK		GENMASK(5, 5)
#define REG07_BATFET_DIS_SHIFT		5
#define REG07_BATFET_OFF		1
#define REG07_BATFET_ON			0

#define REG07_JEITA_VSET_MASK		GENMASK(4, 4)
#define REG07_JEITA_VSET_SHIFT		4
#define REG07_JEITA_VSET_4100		0
#define REG07_JEITA_VSET_VREG		1

#define	REG07_BATFET_DLY_MASK		GENMASK(3, 3)
#define	REG07_BATFET_DLY_SHIFT		3
#define	REG07_BATFET_DLY_0S		0
#define	REG07_BATFET_DLY_10S		1

#define	REG07_BATFET_RST_EN_MASK	GENMASK(2, 2)
#define	REG07_BATFET_RST_EN_SHIFT	2
#define	REG07_BATFET_RST_DISABLE	0
#define	REG07_BATFET_RST_ENABLE		1

#define	REG07_VDPM_BAT_TRACK_MASK	GENMASK(1, 0)
#define	REG07_VDPM_BAT_TRACK_SHIFT	0
#define	REG07_VDPM_BAT_TRACK_DISABLE	0
#define	REG07_VDPM_BAT_TRACK_200MV	1
#define	REG07_VDPM_BAT_TRACK_250MV	2
#define	REG07_VDPM_BAT_TRACK_300MV	3

/* Register 0x08*/
#define BQ2560X_REG_08			0x08
#define REG08_VBUS_STAT_MASK		GENMASK(7, 5)
#define REG08_VBUS_STAT_SHIFT		5
#define REG08_VBUS_TYPE_NONE		0
#define REG08_VBUS_TYPE_USB		1
#define REG08_VBUS_TYPE_ADAPTER		3
#define REG08_VBUS_TYPE_OTG		7

#define REG08_CHRG_STAT_MASK		GENMASK(4, 3)
#define REG08_CHRG_STAT_SHIFT		3
#define REG08_CHRG_STAT_IDLE		0
#define REG08_CHRG_STAT_PRECHG		1
#define REG08_CHRG_STAT_FASTCHG		2
#define REG08_CHRG_STAT_CHGDONE		3

#define REG08_PG_STAT_MASK		GENMASK(2, 2)
#define REG08_PG_STAT_SHIFT		2
#define REG08_POWER_GOOD		1

#define REG08_THERM_STAT_MASK		GENMASK(1, 1)
#define REG08_THERM_STAT_SHIFT		1

#define REG08_VSYS_STAT_MASK		GENMASK(0, 0)
#define REG08_VSYS_STAT_SHIFT		0
#define REG08_IN_VSYS_STAT		1

/* Register 0x09*/
#define BQ2560X_REG_09			0x09
#define REG09_FAULT_WDT_MASK		GENMASK(7, 7)
#define REG09_FAULT_WDT_SHIFT		7
#define REG09_FAULT_WDT			1

#define REG09_FAULT_BOOST_MASK		GENMASK(6, 6)
#define REG09_FAULT_BOOST_SHIFT		6
#define REG09_FAULT_BOOT		1

#define REG09_FAULT_CHRG_MASK		GENMASK(5, 4)
#define REG09_FAULT_CHRG_SHIFT		4
#define REG09_FAULT_CHRG_NORMAL		0
#define REG09_FAULT_CHRG_INPUT		1
#define REG09_FAULT_CHRG_THERMAL	2
#define REG09_FAULT_CHRG_TIMER		3

#define REG09_FAULT_BAT_MASK		GENMASK(3, 3)
#define REG09_FAULT_BAT_SHIFT		3
#define	REG09_FAULT_BAT_OVP		1

#define REG09_FAULT_NTC_MASK		GENMASK(2, 0)
#define REG09_FAULT_NTC_SHIFT		0
#define	REG09_FAULT_NTC_NORMAL		0
#define REG09_FAULT_NTC_WARM		2
#define REG09_FAULT_NTC_COOL		3
#define REG09_FAULT_NTC_COLD		5
#define REG09_FAULT_NTC_HOT		6

/* Register 0x0A */
#define BQ2560X_REG_0A			0x0a
#define	REG0A_VBUS_GD_MASK		GENMASK(7, 7)
#define	REG0A_VBUS_GD_SHIFT		7
#define	REG0A_VBUS_GD			1

#define	REG0A_VINDPM_STAT_MASK		GENMASK(6, 6)
#define	REG0A_VINDPM_STAT_SHIFT		6
#define	REG0A_VINDPM_ACTIVE		1

#define	REG0A_IINDPM_STAT_MASK		GENMASK(5, 5)
#define	REG0A_IINDPM_STAT_SHIFT		5
#define	REG0A_IINDPM_ACTIVE		1

#define	REG0A_TOPOFF_ACTIVE_MASK	GENMASK(3, 3)
#define	REG0A_TOPOFF_ACTIVE_SHIFT	3
#define	REG0A_TOPOFF_ACTIVE		1

#define	REG0A_ACOV_STAT_MASK		GENMASK(2, 2)
#define	REG0A_ACOV_STAT_SHIFT		2
#define	REG0A_ACOV_ACTIVE		1

#define	REG0A_VINDPM_INT_MASK		GENMASK(1, 1)
#define	REG0A_VINDPM_INT_SHIFT		1
#define	REG0A_VINDPM_INT_ENABLE		0
#define	REG0A_VINDPM_INT_DISABLE	1

#define	REG0A_IINDPM_INT_MASK		GENMASK(0, 0)
#define	REG0A_IINDPM_INT_SHIFT		0
#define	REG0A_IINDPM_INT_ENABLE		0
#define	REG0A_IINDPM_INT_DISABLE	1

#define	REG0A_INT_MASK_MASK		GENMASK(1, 0)
#define	REG0A_INT_MASK_SHIFT		0

/* Register 0x0B */
#define	BQ2560X_REG_0B			0x0b
#define	REG0B_RESET_MASK		GENMASK(7, 7)
#define	REG0B_RESET_SHIFT		7
#define	REG0B_RESET			1

#define REG0B_PN_MASK			GENMASK(6, 3)
#define REG0B_PN_SHIFT			3

#define REG0B_DEV_REV_MASK		GENMASK(1, 0)
#define REG0B_DEV_REV_SHIFT		0

#define USB_SDP_CURRENT 500000
#define USB_DCP_CURRENT 1500000

struct bq25601_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct usb_phy *usb_phy;
	struct notifier_block usb_notify;
	struct power_supply *psy_usb;
	struct power_supply_charge_current cur;
	struct work_struct work;
	struct mutex lock;
	bool charging;
	u32 limit;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work vindpm_work;
	struct delayed_work reg_debug_work;
	struct regmap *pmic;
	u32 charger_detect;
	struct gpio_desc *gpiod;
	u32 disable_pin_reg;
};

struct reg_debug_info {
	bool set_en;
	u8 addr;
	u8 value;
};

static struct reg_debug_info reg_debug_array[13];
static struct bq25601_charger_info *charger_info;

static bool bq25601_charger_is_bat_present(struct bq25601_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(BQ25601_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int bq25601_read(struct bq25601_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int bq25601_write(struct bq25601_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int bq25601_update_bits(struct bq25601_charger_info *info, u8 reg,
		u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = bq25601_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return bq25601_write(info, reg, v);
}

static void bq25601_print_regs(struct bq25601_charger_info *info)
{
	int i;
	u8 value[12];

	for (i = 0; i < ARRAY_SIZE(value); i++) {
		bq25601_read(info, i, &(value[i]));
		if (i == ARRAY_SIZE(value) - 1) {
			pr_info("####### bq25601: 0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
				value[0], value[1], value[2], value[3], value[4], value[5],
				value[6], value[7], value[8], value[9], value[10], value[11]);
		}
	}
}

static int
bq25601_charger_set_termina_vol(struct bq25601_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol < REG04_VREG_BASE)
		reg_val = 0x0;
	else if (vol >= REG04_VREG_MAX)
		reg_val = REG04_VREG_MASK >> REG04_VREG_SHIFT;
	else if (vol == REG04_VREG_EXCE_VOLTAGE)
		reg_val = REG04_VREG_EXCE_CODE;
	else
		reg_val = (vol - REG04_VREG_BASE) / REG04_VREG_LSB;

	return bq25601_update_bits(info, BQ2560X_REG_04,
				    REG04_VREG_MASK,
				    reg_val << REG04_VREG_SHIFT);
}

static int bq25601_charger_get_termina_vol(struct bq25601_charger_info *info,
					u32 *vol)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_04, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG04_VREG_MASK;
	reg_val = reg_val >> REG04_VREG_SHIFT;

	if (reg_val == REG04_VREG_EXCE_CODE)
		*vol = REG04_VREG_EXCE_VOLTAGE * 1000;
	else
		*vol = (reg_val * REG04_VREG_LSB + REG04_VREG_BASE) * 1000;

	return 0;
}


static int
bq25601_charger_set_topoff(struct bq25601_charger_info *info, u32 topoff_ma)
{
	u8 reg_val = 0;
	int ret = 0;

	reg_val = (topoff_ma - REG03_ITERM_BASE) / REG03_ITERM_LSB;

	if (topoff_ma % REG03_ITERM_LSB)
		reg_val = reg_val + 1;

	ret = bq25601_update_bits(info, BQ2560X_REG_03,
				   REG03_ITERM_MASK,
				   reg_val << REG03_ITERM_SHIFT);
	if (ret) {
		dev_err(info->dev, "set bq25601 terminal cur failed\n");
		return ret;
	}


	return 0;
}

static int bq25601_charger_get_topoff(struct bq25601_charger_info *info,
					u32 *topoff_ua)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_03, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG03_ITERM_MASK;
	reg_val = reg_val >> REG03_ITERM_SHIFT;

	*topoff_ua = (reg_val * REG03_ITERM_LSB + REG03_ITERM_BASE) * 1000;
	return 0;
}

static int
bq25601_charger_set_recharge_voltage(struct bq25601_charger_info *info, u32 recharge_voltage_uv)
{
	u8 reg_val = 0;
	int ret = 0;

	reg_val = (recharge_voltage_uv > 100000) ? REG04_VRECHG_200MV : REG04_VRECHG_100MV;
	ret = bq25601_update_bits(info, BQ2560X_REG_04,
				   REG04_VRECHG_MASK,
				   reg_val << REG04_VRECHG_SHIFT);
	if (ret) {
		dev_err(info->dev, "set bq25601 recharge_voltage failed\n");
		return ret;
	}


	return 0;
}

static int bq25601_charger_get_recharge_voltage(struct bq25601_charger_info *info,
					u32 *recharge_voltage_uv)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_04, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG04_VRECHG_MASK;
	reg_val = reg_val >> REG04_VRECHG_SHIFT;

	*recharge_voltage_uv = reg_val ? 200000 : 100000;

	return 0;
}

static int bq25601_charger_start_charge(struct bq25601_charger_info *info)
{
	int ret;

	pr_info("bq25601: start charge #####\n");

	ret = bq25601_update_bits(info, BQ2560X_REG_01,
				   REG01_CHG_CONFIG_MASK,
				   REG01_CHG_ENABLE << REG01_CHG_CONFIG_SHIFT);

	if (ret)
		dev_err(info->dev, "enable bq25601 charge failed\n");

	return ret;
}

static void bq25601_charger_stop_charge(struct bq25601_charger_info *info)
{
	int ret;

	pr_info("bq25601: stop charge #####\n");

	ret = bq25601_update_bits(info, BQ2560X_REG_01,
				   REG01_CHG_CONFIG_MASK,
				   REG01_CHG_DISABLE << REG01_CHG_CONFIG_SHIFT);
	if (ret)
		dev_err(info->dev, "disable bq25601 charge failed\n");
}

static int bq25601_charger_set_current(struct bq25601_charger_info *info,
					u32 cur)
{
	u8 reg_val;

	if (cur > REG02_ICHG_MAX * 1000)
		cur = REG02_ICHG_MAX * 1000;

	reg_val = (cur/1000 - REG02_ICHG_BASE) / REG02_ICHG_LSB;


	return bq25601_update_bits(info, BQ2560X_REG_02,
				    REG02_ICHG_MASK,
				    reg_val << REG02_ICHG_SHIFT);
}

static int bq25601_charger_get_current(struct bq25601_charger_info *info,
					u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_02, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG02_ICHG_MASK;
	reg_val = reg_val >> REG02_ICHG_SHIFT;

	*cur = (reg_val * REG02_ICHG_LSB + REG02_ICHG_BASE) * 1000;
	return 0;
}

static int
bq25601_charger_set_limit_current(struct bq25601_charger_info *info,
				   u32 limit_cur)
{
	u8 reg_val;
	int ret;

	if (limit_cur <= 100000)
		reg_val = 0x0;
	else if (limit_cur > 3200000)
		reg_val = REG00_IINLIM_MASK >> REG00_IINLIM_SHIFT;
	else
		reg_val = (limit_cur/1000 - REG00_IINLIM_BASE)/REG00_IINLIM_LSB;

	ret = bq25601_update_bits(info, BQ2560X_REG_00,
				   REG00_IINLIM_MASK,
				   reg_val << REG00_IINLIM_SHIFT);

	if (ret)
		dev_err(info->dev, "set bq25601 limit cur failed\n");

	return ret;
}

static u32
bq25601_charger_get_limit_current(struct bq25601_charger_info *info,
				   u32 *limit_cur)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_00, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG00_IINLIM_MASK;
	reg_val = reg_val >> REG00_IINLIM_SHIFT;

	*limit_cur = 1000 * (reg_val * REG00_IINLIM_LSB + REG00_IINLIM_BASE);
	return 0;
}

static int bq25601_charger_get_health(struct bq25601_charger_info *info,
				     u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int bq25601_charger_get_online(struct bq25601_charger_info *info,
				     u32 *online)
{
	if (info->limit)
		*online = true;
	else
		*online = false;

	return 0;
}

static int bq25601_charger_feed_watchdog(struct bq25601_charger_info *info,
					  u32 val)
{
	int ret;

	ret = bq25601_update_bits(info, BQ2560X_REG_01,
				   REG01_WDT_RESET_MASK, REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);

	if (ret)
		dev_err(info->dev, "reset bq25601 failed\n");

	return ret;
}

static int bq25601_charger_set_shipmode(struct bq25601_charger_info *info,
					  u32 val)
{
	int ret;

	ret = bq25601_update_bits(info, BQ2560X_REG_05, REG05_WDT_MASK,
			REG05_WDT_DISABLE << REG05_WDT_SHIFT);
	if (ret)
		dev_err(info->dev, "disable bq2560x wdt failed\n");

	ret = bq25601_update_bits(info, BQ2560X_REG_07, REG07_BATFET_DIS_MASK,
			REG07_BATFET_OFF << REG07_BATFET_DIS_SHIFT);

	if (ret)
		dev_err(info->dev, "set_shipmode failed\n");
	pr_info("bq25601: set shipmode #####\n");

	return ret;
}

static int bq25601_charger_get_shipmode(struct bq25601_charger_info *info, u32 *val)
{
	u8 data = 0;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_07, &data);
	if (ret < 0)
		return ret;
	data = (data & REG07_BATFET_DIS_MASK) >> REG07_BATFET_DIS_SHIFT;
	*val = !data;

	return 0;
}

static int bq25601_charger_set_vindpm(struct bq25601_charger_info *info,
					u32 vol_mv)
{
	u8 reg_val;

	if (vol_mv > REG06_VINDPM_MAX)
		vol_mv = REG06_VINDPM_MAX;

	reg_val = (vol_mv - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;

	dev_info(info->dev, "set bq25601 vindpm reg_val 0x%x\n", reg_val);

	return bq25601_update_bits(info, BQ2560X_REG_06,
				    REG06_VINDPM_MASK,
				    reg_val << REG06_VINDPM_SHIFT);
}

static int bq25601_charger_get_vindpm(struct bq25601_charger_info *info,
					u32 *vol)
{
	u8 reg_val;
	int ret;

	ret = bq25601_read(info, BQ2560X_REG_06, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG06_VINDPM_MASK;
	reg_val = reg_val >> REG06_VINDPM_SHIFT;

	*vol = reg_val * REG06_VINDPM_LSB + REG06_VINDPM_BASE;
	return 0;
}

static int bq25601_charger_set_safety_timer(struct bq25601_charger_info *info,
					u8 enable)
{
	return bq25601_update_bits(info, BQ2560X_REG_05,
				    REG05_EN_TIMER_MASK,
				    enable << REG05_EN_TIMER_SHIFT);
}

static int bq25601_charger_set_watchdog_timer(struct bq25601_charger_info *info,
					u32 timer)
{
	u8 reg_val = 0;

	if (timer >= 160)
		reg_val = REG05_WDT_160S;
	else if (timer <= 0)
		reg_val = REG05_WDT_DISABLE;
	else
		reg_val = (timer - REG05_WDT_BASE) / REG05_WDT_LSB;
	return bq25601_update_bits(info, BQ2560X_REG_05,
					REG05_WDT_MASK,
					reg_val << REG05_WDT_SHIFT);
}

static int bq25601_charger_get_reg_status(struct bq25601_charger_info *info,
					u32 *status)
{
	u8 reg_val;
	int ret;

	if (!info || !status) {
		pr_err("%s: Null ptr\n", __func__);
		return -EFAULT;
	}

	ret = bq25601_read(info, BQ2560X_REG_08, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= REG08_CHRG_STAT_MASK;
	reg_val = reg_val >> REG08_CHRG_STAT_SHIFT;
	*status = reg_val;

	return 0;
}

static int bq25601_charger_get_status(struct bq25601_charger_info *info)
{
	u32 charger_status = 0;
	int ret = 0;

	ret = bq25601_charger_get_reg_status(info, &charger_status);
	if (ret == 0) {
		if (charger_status == REG08_CHRG_STAT_IDLE) {
			return POWER_SUPPLY_STATUS_NOT_CHARGING;
		} else if ((charger_status == REG08_CHRG_STAT_PRECHG) ||
					(charger_status == REG08_CHRG_STAT_FASTCHG)) {
			return POWER_SUPPLY_STATUS_CHARGING;
		} else if (charger_status == REG08_CHRG_STAT_CHGDONE) {
			return POWER_SUPPLY_STATUS_FULL;
		} else {
			return POWER_SUPPLY_STATUS_CHARGING;
		}
	}
	pr_info("bq25601: read status failed, ret = %d\n", ret);
	return POWER_SUPPLY_STATUS_UNKNOWN;
}

static int bq25601_charger_set_status(struct bq25601_charger_info *info,
				       int val)
{
	int ret = 0;

	if (!val && info->charging) {
		bq25601_charger_stop_charge(info);
		info->charging = false;
	} else if (val && !info->charging) {
		ret = bq25601_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static int bq25601_charger_dumper_reg(struct bq25601_charger_info *info)
{
	int usb_icl = 0, fcc = 0, fcv = 0, topoff = 0, recharge_voltage = 0;

	bq25601_charger_get_termina_vol(info, &fcv);

	bq25601_charger_get_limit_current(info, &usb_icl);

	bq25601_charger_get_current(info, &fcc);

	bq25601_charger_get_topoff(info, &topoff);

	bq25601_charger_get_recharge_voltage(info, &recharge_voltage);

	pr_info("bq25601: charging[%d], fcv[%d], usb_icl[%d], fcc[%d], topoff[%d], rechg_volt[%d]",
				info->charging, fcv / 1000, usb_icl / 1000, fcc / 1000,
				topoff / 1000, recharge_voltage / 1000);
	bq25601_print_regs(info);
	return 0;
}


static void bq25601_charger_work(struct work_struct *data)
{
	struct bq25601_charger_info *info =
		container_of(data, struct bq25601_charger_info, work);
	int limit_cur, cur, ret;
	bool present = bq25601_charger_is_bat_present(info);

	mutex_lock(&info->lock);

	if (info->limit > 0 && !info->charging && present) {
		/* set current limitation and start to charge */
		switch (info->usb_phy->chg_type) {
		case SDP_TYPE:
			limit_cur = info->cur.sdp_limit;
			cur = info->cur.sdp_cur;
			break;
		case DCP_TYPE:
			limit_cur = info->cur.dcp_limit;
			cur = info->cur.dcp_cur;
			break;
		case CDP_TYPE:
			limit_cur = info->cur.cdp_limit;
			cur = info->cur.cdp_cur;
			break;
		default:
			limit_cur = info->cur.unknown_limit;
			cur = info->cur.unknown_cur;
		}

		ret = bq25601_charger_set_limit_current(info, limit_cur);
		if (ret)
			goto out;

		ret = bq25601_charger_set_current(info, cur);
		if (ret)
			goto out;

		ret = bq25601_charger_start_charge(info);
		if (ret)
			goto out;

		info->charging = true;
	} else if ((!info->limit && info->charging) || !present) {
		/* Stop charging */
		info->charging = false;
		bq25601_charger_stop_charge(info);
	}

out:
	mutex_unlock(&info->lock);
	dev_info(info->dev, "battery present = %d, charger type = %d\n",
		 present, info->usb_phy->chg_type);
	cm_notify_event(info->psy_usb, CM_EVENT_CHG_START_STOP, NULL);
}


static int bq25601_charger_usb_change(struct notifier_block *nb,
				       unsigned long limit, void *data)
{
	struct bq25601_charger_info *info =
		container_of(nb, struct bq25601_charger_info, usb_notify);

	info->limit = limit;

	pr_info("bq25601: bq25601_charger_usb_change: %d\n", limit);

#ifndef CONFIG_VENDOR_POWER_VOTER
	schedule_work(&info->work);
#endif

	return NOTIFY_OK;
}

#define VCHG_CTRL_THRESHOLD_MV_072 4370
#define VINDPM_LOW_THRESHOLD_UV 4150000
static int bq25601_charger_tuning_vindpm(struct bq25601_charger_info *info)
{
	union power_supply_propval val;
	struct power_supply *fuel_gauge;
	int ret = 0, vchg = 0, vindpm = 0;

	fuel_gauge = power_supply_get_by_name(BQ25601_BATTERY_NAME);
	if (!fuel_gauge)
		return -ENODEV;

	ret = power_supply_get_property(fuel_gauge,
				POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE, &val);
	power_supply_put(fuel_gauge);
	if (ret)
		return ret;

	vchg = val.intval / 1000;

	bq25601_charger_get_vindpm(info, &vindpm);
	pr_info("%s: get CHARGE_VOLTAGE %d, vindpm %d\n", __func__, vchg, vindpm);

	if (vchg > VCHG_CTRL_THRESHOLD_MV_072) {
		bq25601_charger_set_vindpm(info, REG06_VINDPM_BASE);
		pr_info("%s: vchg %d, now vindpm %d, set vindpm to %d\n", __func__, vchg, vindpm, REG06_VINDPM_BASE);
	} else {
		bq25601_charger_set_vindpm(info, REG06_VINDPM_NORMAL);
		pr_info("%s: vchg %d, now vindpm %d, set vindpm to %d\n", __func__, vchg, vindpm, REG06_VINDPM_NORMAL);
	}

	return 0;
}

static int bq25601_charger_usb_get_property(struct power_supply *psy,
					     enum power_supply_property psp,
					     union power_supply_propval *val)
{
	struct bq25601_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur, online, health;
	enum usb_charger_type type;
	int ret = 0;

	if (!info)
		return -EAGAIN;

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_CHARGING_ENABLED:
		if (info->limit)
			val->intval = bq25601_charger_get_status(info);
		else
			val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
		break;

	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25601_charger_get_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_CHARGE_CONTROL_LIMIT:
		val->intval = info->limit;
		break;

	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = bq25601_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;

			val->intval = cur;
		}
		break;

	case POWER_SUPPLY_PROP_ONLINE:
		ret = bq25601_charger_get_online(info, &online);
		if (ret)
			goto out;

		val->intval = online;

		break;

	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = bq25601_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;

	case POWER_SUPPLY_PROP_USB_TYPE:
		type = info->usb_phy->charger_detect(info->usb_phy);

		switch (type) {
		case SDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_SDP;
			break;

		case DCP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_DCP;
			break;

		case CDP_TYPE:
			val->intval = POWER_SUPPLY_USB_TYPE_CDP;
			break;

		default:
			val->intval = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		}

		break;
	case POWER_SUPPLY_PROP_VOLTAGE_MAX:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		bq25601_charger_get_termina_vol(info, &val->intval);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		ret = bq25601_charger_get_shipmode(info, &val->intval);
		if (ret < 0)
			dev_err(info->dev, "get shipmode failed\n");
		pr_info("bq25601[REG]: get shipmode: %d!", val->intval);
		break;
	case POWER_SUPPLY_PROP_TUNING_VINDPM:
		ret = bq25601_charger_get_vindpm(info, &val->intval);
		if (ret < 0)
			dev_err(info->dev, "get vindpm failed\n");
		pr_info("bq25601[REG]: get vindpm: %d!", val->intval);
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		ret = bq25601_charger_get_topoff(info, &val->intval);
		if (ret < 0)
			dev_err(info->dev, "get topoff failed\n");
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		ret = bq25601_charger_get_recharge_voltage(info, &val->intval);
		if (ret < 0)
			dev_err(info->dev, "get charge recharge_voltage failed\n");
		break;
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		val->intval = 60;
		break;

	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int bq25601_charger_usb_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	struct bq25601_charger_info *info = power_supply_get_drvdata(psy);
	int ret;
	int cur;

	if (!info)
		return -EAGAIN;

	mutex_lock(&info->lock);

#ifndef CONFIG_VENDOR_POWER_VOTER
	if (!info->charging && psp != POWER_SUPPLY_PROP_STATUS) {
		mutex_unlock(&info->lock);
		return -ENODEV;
	}
#endif

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		pr_info("bq25601[REG]: set fcc %d", val->intval);
		ret = bq25601_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		bq25601_charger_get_current(info, &cur);
		pr_info("bq25601[REG]: get fcc %d", cur);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
		pr_info("bq25601[REG]: set fcv %d", val->intval);
		ret = bq25601_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "set charge voltage failed\n");
		break;
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
		pr_info("bq25601[REG]: set topoff %d", val->intval);
		ret = bq25601_charger_set_topoff(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge voltage failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		pr_info("bq25601[REG]: set usb icl %d", val->intval);
		ret = bq25601_charger_set_limit_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		bq25601_charger_get_limit_current(info, &cur);
		pr_info("bq25601[REG]: get usb icl %d", cur);
		break;
	case POWER_SUPPLY_PROP_STATUS:
		pr_info("bq25601[REG]: set enable %d", val->intval);

		ret = bq25601_charger_set_status(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
		pr_info("bq25601[REG]: set recharge_voltage %d", val->intval);

		ret = bq25601_charger_set_recharge_voltage(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge recharge_voltage failed\n");
		break;
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		pr_info("bq25601[REG]: feed watchdog\n");
		ret = bq25601_charger_feed_watchdog(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "feed charger watchdog failed\n");
		bq25601_charger_dumper_reg(info);
		break;
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
		if (val->intval == 0) {
			pr_info("bq25601[REG]: set shipmode %d", val->intval);
			ret = bq25601_charger_set_shipmode(info, val->intval);
			if (ret < 0)
				dev_err(info->dev, "set shipmode failed\n");
		} else
			pr_info("bq25601[REG]: set shipmode invalid val %d!", val->intval);
		break;
	case POWER_SUPPLY_PROP_TUNING_VINDPM:
		pr_info("bq25601[REG]: tuning vindpm %d", val->intval);
		bq25601_charger_set_vindpm(info, REG06_VINDPM_TRY);
		schedule_delayed_work(&info->vindpm_work, HZ * 3);
		break;
	case POWER_SUPPLY_PROP_SET_WATCHDOG_TIMER:
		pr_info("bq25601[REG]: set dog %d", val->intval);
		ret = bq25601_charger_set_watchdog_timer(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "failed to set watchdog timer\n");
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int bq25601_charger_property_is_writeable(struct power_supply *psy,
						enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_SET_SHIP_MODE:
	case POWER_SUPPLY_PROP_TUNING_VINDPM:
	case POWER_SUPPLY_PROP_RECHARGE_SOC:
	case POWER_SUPPLY_PROP_FEED_WATCHDOG:
		ret = 1;
		break;

	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_usb_type bq25601_charger_usb_types[] = {
	POWER_SUPPLY_USB_TYPE_UNKNOWN,
	POWER_SUPPLY_USB_TYPE_SDP,
	POWER_SUPPLY_USB_TYPE_DCP,
	POWER_SUPPLY_USB_TYPE_CDP,
	POWER_SUPPLY_USB_TYPE_C,
	POWER_SUPPLY_USB_TYPE_PD,
	POWER_SUPPLY_USB_TYPE_PD_DRP,
	POWER_SUPPLY_USB_TYPE_APPLE_BRICK_ID
};

static enum power_supply_property bq25601_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGING_ENABLED,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_CHARGE_TERM_CURRENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_USB_TYPE,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_SET_SHIP_MODE,
	POWER_SUPPLY_PROP_TUNING_VINDPM,
	POWER_SUPPLY_PROP_FEED_WATCHDOG,
	POWER_SUPPLY_PROP_RECHARGE_SOC,
};

static const struct power_supply_desc bq25601_charger_desc = {
	.name			= "bq25601_charger",
#ifdef CONFIG_VENDOR_POWER_VOTER
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
#else
	.type			= POWER_SUPPLY_TYPE_USB,
#endif
	.properties		= bq25601_usb_props,
	.num_properties		= ARRAY_SIZE(bq25601_usb_props),
	.get_property		= bq25601_charger_usb_get_property,
	.set_property		= bq25601_charger_usb_set_property,
	.property_is_writeable	= bq25601_charger_property_is_writeable,
	.usb_types		= bq25601_charger_usb_types,
	.num_usb_types		= ARRAY_SIZE(bq25601_charger_usb_types),
};

static void bq25601_charger_detect_status(struct bq25601_charger_info *info)
{
	int min, max;

	/*
	 * If the USB charger status has been USB_CHARGER_PRESENT before
	 * registering the notifier, we should start to charge with getting
	 * the charge current.
	 */
	pr_info("bq25601: charger_detect_status %d", info->usb_phy->chg_state);
	if (info->usb_phy->chg_state != USB_CHARGER_PRESENT)
		return;

	usb_phy_get_charger_current(info->usb_phy, &min, &max);
	info->limit = min;
	pr_info("bq25601: limit %d", min);

#ifndef CONFIG_VENDOR_POWER_VOTER
	schedule_work(&info->work);
#endif
}

static void
bq25601_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25601_charger_info *info = container_of(dwork,
							  struct bq25601_charger_info,
							  wdt_work);
	int ret;

	ret = bq25601_update_bits(info, BQ2560X_REG_01,
				   REG01_WDT_RESET_MASK, REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
	if (ret) {
		dev_err(info->dev, "reset bq25601 failed\n");
		return;
	}

	schedule_delayed_work(&info->wdt_work, HZ * 30);
}

#ifdef CONFIG_REGULATOR
static void bq25601_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25601_charger_info *info = container_of(dwork,
			struct bq25601_charger_info, otg_work);
	u32 vbus_gpio_value;
	int ret;

	if (!info->gpiod)
		return;

	vbus_gpio_value = gpiod_get_value_cansleep(info->gpiod);
	if (!vbus_gpio_value) {
		ret = bq25601_update_bits(info, BQ2560X_REG_01,
					  REG01_OTG_CONFIG_MASK,
					   REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
		if (ret)
			dev_err(info->dev, "restart bq25601 charger otg failed\n");
	}

	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

#define BQ25601_OTG_VALID_MS				500
#define BQ25601_FEED_WATCHDOG_VALID_MS		50
#define BIT_DP_DM_BC_ENB					BIT(0)

static int bq25601_charger_enable_otg(struct regulator_dev *dev)
{
	struct bq25601_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	ret = regmap_update_bits(info->pmic, info->charger_detect,
				BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
	if (ret) {
		dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
		return ret;
	}

	ret = bq25601_update_bits(info, BQ2560X_REG_01,
				REG01_OTG_CONFIG_MASK,
				REG01_OTG_ENABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable bq25601 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect,
				BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(BQ25601_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(BQ25601_OTG_VALID_MS));

	return 0;
}

static int bq25601_charger_disable_otg(struct regulator_dev *dev)
{
	struct bq25601_charger_info *info = rdev_get_drvdata(dev);
	int ret;

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = bq25601_update_bits(info, BQ2560X_REG_01,
					  REG01_OTG_CONFIG_MASK,
					   REG01_OTG_DISABLE << REG01_OTG_CONFIG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable bq25601 otg failed\n");
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	return regmap_update_bits(info->pmic, info->charger_detect,
				  BIT_DP_DM_BC_ENB, 0);
}

static int bq25601_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct bq25601_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	ret = bq25601_read(info, BQ2560X_REG_01, &val);
	if (ret) {
		dev_err(info->dev, "failed to get bq25601 otg status\n");
		return ret;
	}

	val &= REG01_OTG_CONFIG_MASK;

	return val;
}

static const struct regulator_ops bq25601_charger_vbus_ops = {
	.enable = bq25601_charger_enable_otg,
	.disable = bq25601_charger_disable_otg,
	.is_enabled = bq25601_charger_vbus_is_enabled,
};

static const struct regulator_desc bq25601_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &bq25601_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static int
bq25601_charger_register_vbus_regulator(struct bq25601_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev,
				      &bq25601_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

#else
static int
bq25601_charger_register_vbus_regulator(struct bq25601_charger_info *info)
{
	return 0;
}
#endif

static void
tuning_vindpm_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct bq25601_charger_info *info = container_of(dwork,
							  struct bq25601_charger_info,
							  vindpm_work);

	pr_info("%s: enter", __func__);

	bq25601_charger_tuning_vindpm(info);
}

static int bq25601_charger_hw_init(struct bq25601_charger_info *info)
{
	struct power_supply_battery_info bat_info = { };
	int voltage_max_microvolt, current_max_ua;
	int ret, value;

	ret = power_supply_get_battery_info(info->psy_usb, &bat_info);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");

		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 100 mA, and default
		 * charge termination voltage to 4.2V.
		 */
		info->cur.sdp_limit = USB_SDP_CURRENT;
		info->cur.sdp_cur = USB_SDP_CURRENT;
		info->cur.dcp_limit = USB_SDP_CURRENT;
		info->cur.dcp_cur = USB_SDP_CURRENT;
		info->cur.cdp_limit = USB_SDP_CURRENT;
		info->cur.cdp_cur = USB_DCP_CURRENT;
		info->cur.unknown_limit = USB_SDP_CURRENT;
		info->cur.unknown_cur = USB_SDP_CURRENT;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;

		voltage_max_microvolt =
			bat_info.constant_charge_voltage_max_uv / 1000;
		current_max_ua = bat_info.constant_charge_current_max_ua / 1000;
		power_supply_put_battery_info(info->psy_usb, &bat_info);

		ret = bq25601_update_bits(info, BQ2560X_REG_0B,
					   REG0B_RESET_MASK,
					   REG0B_RESET << REG0B_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "reset bq25601 failed\n");
			return ret;
		}

		value = (REG06_VINDPM_NORMAL - REG06_VINDPM_BASE) / REG06_VINDPM_LSB;
		ret = bq25601_update_bits(info, BQ2560X_REG_06,
					   REG06_VINDPM_MASK,
					   value << REG06_VINDPM_SHIFT);
		if (ret) {
			dev_err(info->dev, "set bq25601 vindpm failed\n");
			return ret;
		}

		value = (REG03_ITERM_DEFAULT - REG03_ITERM_BASE) / REG03_ITERM_LSB;
		ret = bq25601_update_bits(info, BQ2560X_REG_03,
					   REG03_ITERM_MASK,
					   value << REG03_ITERM_SHIFT);
		if (ret) {
			dev_err(info->dev, "set bq25601 terminal cur failed\n");
			return ret;
		}

		ret = bq25601_update_bits(info, BQ2560X_REG_01,
				   REG01_WDT_RESET_MASK, REG01_WDT_RESET << REG01_WDT_RESET_SHIFT);
		if (ret) {
			dev_err(info->dev, "feed bq25601 watchdog failed\n");
			return ret;
		}
		ret = bq25601_charger_set_termina_vol(info, voltage_max_microvolt);
		if (ret)
			dev_err(info->dev, "set bq25601 terminal vol failed\n");

		ret = bq25601_charger_set_limit_current(info, USB_SDP_CURRENT);
		if (ret)
			dev_err(info->dev, "set bq25601 set limit current failed\n");

		ret = bq25601_charger_set_current(info, USB_SDP_CURRENT);
		if (ret)
			dev_err(info->dev, "set bq25601 set current failed\n");

		ret = bq25601_charger_set_safety_timer(info, REG05_CHG_TIMER_DISABLE);
		if (ret)
			dev_err(info->dev, "set safety timer failed\n");

		ret = bq25601_charger_set_watchdog_timer(info, 0);
		if (ret)
			dev_err(info->dev, "set watchdog timer failed\n");

		dev_warn(info->dev, "bq25601 hw init success, vmax %d, ibat %d\n",
					voltage_max_microvolt, current_max_ua);
		bq25601_print_regs(info);
	}
	return ret;
}

static void reg_debug_works(struct work_struct *work)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(reg_debug_array); i++) {
		if (reg_debug_array[i].set_en) {
			bq25601_write(charger_info, reg_debug_array[i].addr, reg_debug_array[i].value);
			reg_debug_array[i].set_en = false;
			pr_info("%s: set bq25601 reg[0x%x] 0x%.2x\n", __func__,
				reg_debug_array[i].addr, reg_debug_array[i].value);
		}
	}
}

static int reg_set(const char *val, const struct kernel_param *kp)
{
	int delay_secs;
	uint addr = 0xff, value;
	int num;

	num = sscanf(val, "%d %x %x", &delay_secs, &addr, &value);

	if (addr >= 0 && addr < ARRAY_SIZE(reg_debug_array)) {
		reg_debug_array[addr].set_en = true;
		reg_debug_array[addr].addr = addr & 0xff;
		reg_debug_array[addr].value = value & 0xff;
	} else {
		pr_info("%s: bq25601 error addr\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: set reg[0x%x]: 0x%x after %ds\n", __func__,
		reg_debug_array[addr].addr, reg_debug_array[addr].value, delay_secs);

	if (num != 3) {
		pr_err("%s: wrong args num %d. usage: echo <delay> <reg> > val\n", __func__, num);
		return -EINVAL;
	}

	cancel_delayed_work(&charger_info->reg_debug_work);
	schedule_delayed_work(&charger_info->reg_debug_work, msecs_to_jiffies(delay_secs * 1000));

	return 0;
}

static int reg_get(char *val, const struct kernel_param *kp)
{
	u8 addr;
	u8 data;
	u8 tmpbuf[200];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(val, PAGE_SIZE, "%s:\n", "bq2560x Reg");
	for (addr = 0x0; addr <= 0x0B; addr++) {
		ret = bq25601_read(charger_info, addr, &data);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[0x%.2x] = 0x%.2x\n", addr, data);
			memcpy(&val[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

module_param_call(reg, reg_set, reg_get, NULL, 0644);

static int bq25601_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct bq25601_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	pr_info("%s enter\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;
	mutex_init(&info->lock);
	INIT_WORK(&info->work, bq25601_charger_work);

	info->gpiod = devm_gpiod_get(dev, "otg-detect", GPIOD_IN);
	if (IS_ERR(info->gpiod)) {
		dev_warn(dev, "failed to get charger detection GPIO\n");
		info->gpiod = NULL;
	}

	info->usb_phy = devm_usb_get_phy_by_phandle(dev, "phys", 0);
	if (IS_ERR(info->usb_phy)) {
		dev_err(dev, "######@failed to find USB phy\n");
		return PTR_ERR(info->usb_phy);
	}

	ret = bq25601_charger_register_vbus_regulator(info);
	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		return ret;
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np) {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}
/*
	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->disable_pin_reg);
	if (ret) {
		dev_err(dev, "failed to get bq25601 disable pin reg\n");
		return -EINVAL;
	}
*/
	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	info->usb_notify.notifier_call = bq25601_charger_usb_change;
	ret = usb_register_notifier(info->usb_phy, &info->usb_notify);
	if (ret) {
		dev_err(dev, "failed to register notifier:%d\n", ret);
		return ret;
	}

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	info->psy_usb = devm_power_supply_register(dev,
						   &bq25601_charger_desc,
						   &charger_cfg);
	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return PTR_ERR(info->psy_usb);
	}

	ret = bq25601_charger_hw_init(info);
	if (ret) {
		usb_unregister_notifier(info->usb_phy, &info->usb_notify);
		return ret;
	}
	bq25601_charger_detect_status(info);
	INIT_DELAYED_WORK(&info->otg_work, bq25601_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work,
			  bq25601_charger_feed_watchdog_work);
	charger_info = info;
	INIT_DELAYED_WORK(&info->reg_debug_work, reg_debug_works);
	INIT_DELAYED_WORK(&info->vindpm_work, tuning_vindpm_work);

	pr_info("%s probe success\n", __func__);
	return 0;
}

static int bq25601_charger_remove(struct i2c_client *client)
{
	struct bq25601_charger_info *info = i2c_get_clientdata(client);

	usb_unregister_notifier(info->usb_phy, &info->usb_notify);

	return 0;
}

static const struct i2c_device_id bq25601_i2c_id[] = {
	{"bq25601_chg", 0},
	{}
};

static const struct of_device_id bq25601_charger_of_match[] = {
	{ .compatible = "ti,bq25601_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, bq25601_charger_of_match);

static struct i2c_driver bq25601_charger_driver = {
	.driver = {
		.name = "bq25601_chg",
		.of_match_table = bq25601_charger_of_match,
	},
	.probe = bq25601_charger_probe,
	.remove = bq25601_charger_remove,
	.id_table = bq25601_i2c_id,
};

module_i2c_driver(bq25601_charger_driver);
MODULE_DESCRIPTION("BQ25601 Charger Driver");
MODULE_LICENSE("GPL v2");
