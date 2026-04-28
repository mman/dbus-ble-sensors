#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <velib/utils/ve_item_utils.h>
#include <velib/vecan/products.h>
#include <velib/utils/ve_logger.h>

#include "ble-dbus.h"
#include "rotarex.h"
#include "task.h"

#define FLUID_TYPE_LPG		8

static struct VeSettingProperties capacity_props = {
	.type			= VE_FLOAT,
	.def.value.Float	= 0.2,
	.min.value.Float	= 0,
	.max.value.Float	= 1000,
};

static struct VeSettingProperties fluid_type_props = {
	.type			= VE_SN32,
	.def.value.SN32		= FLUID_TYPE_LPG,
	.min.value.SN32		= 0,
	.max.value.SN32		= INT32_MAX - 3,
};

static const struct dev_setting rotarex_settings[] = {
	{
		.name	= "Capacity",
		.props	= &capacity_props,
	},
	{
		.name	= "FluidType",
		.props	= &fluid_type_props,
	},
};

static int rotarex_init(struct VeItem *root, void *data)
{
	VeVariant v;

	ble_dbus_set_item(root, "Remaining",
			  veVariantInvalidType(&v, VE_FLOAT), &veUnitm3);

	return 0;
}

static const struct dev_info rotarex_sensor = {
	/* TODO: change to VE_PROD_ID_ROTAREX_SENSOR once defined in velib */
	.product_id	= VE_PROD_ID_MOPEKA_SENSOR,
	.dev_instance	= 20,
	.dev_prefix	= "rotarex_",
	.role		= "tank",
	.num_settings	= array_size(rotarex_settings),
	.settings	= rotarex_settings,
	.init		= rotarex_init,
};

static const struct reg_info rotarex_adv[] = {
	{
		/* mfd[3] - battery level, 0..100 % */
		.type	= VE_UN8,
		.offset	= 1,
		.name	= "BatteryLevel",
		.format	= &veUnitNone,
	},
	{
		/* mfd[4] - LPG level, 0..100 % */
		.type	= VE_UN8,
		.offset	= 2,
		.name	= "Level",
		.format	= &veUnitNone,
	},
};

static void rotarex_update_remaining(struct VeItem *root)
{
	struct VeItem *item;
	float capacity;
	int level;
	float remain;
	VeVariant v;

	item = veItemByUid(root, "Capacity");
	if (!item)
		return;

	veItemLocalValue(item, &v);
	veVariantToFloat(&v);
	capacity = v.value.Float;

	level = veItemValueInt(root, "Level");

	if (level < 0 || level > 100) {
		veItemInvalidate(veItemByUid(root, "Level"));
		veItemInvalidate(veItemByUid(root, "Remaining"));
		ble_dbus_set_int(root, "Status", 4);
		return;
	}

	remain = capacity * level / 100.0f;

	item = veItemByUid(root, "Remaining");
	veItemOwnerSet(item, veVariantFloat(&v, remain));
}

int rotarex_handle_mfg(const bdaddr_t *addr, const uint8_t *buf, int len)
{
	struct VeItem *root;
	char name[24];
	char dev[16];

	/* Identify Rotarex by Bluegiga OUI 88:6B:0F */
	if (addr->b[5] != 0x88 ||
	    addr->b[4] != 0x6b ||
	    addr->b[3] != 0x0f)
		return -1;

	if (len < 3)
		return -1;

	snprintf(dev, sizeof(dev), "%02x%02x%02x%02x%02x%02x",
		 addr->b[5], addr->b[4], addr->b[3],
		 addr->b[2], addr->b[1], addr->b[0]);

	root = ble_dbus_create(dev, &rotarex_sensor, NULL);
	if (!root)
		return -1;

	snprintf(name, sizeof(name), "Rotarex %02X:%02X:%02X",
		 addr->b[2], addr->b[1], addr->b[0]);
	ble_dbus_set_name(root, name);

	if (!ble_dbus_is_enabled(root))
		return 0;

	ble_dbus_set_regs(root, rotarex_adv, array_size(rotarex_adv), buf, len);

	rotarex_update_remaining(root);
	ble_dbus_update(root);

	return 0;
}
