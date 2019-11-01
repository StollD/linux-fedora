// SPDX-License-Identifier: GPL-2.0+
/*
 * Broadcom AVS RO thermal sensor driver
 *
 * based on brcmstb_thermal
 *
 * Copyright (C) 2019 Stefan Wahren
 */

#include <linux/bitops.h>
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/thermal.h>

#include "../thermal_hwmon.h"

#define AVS_RO_TEMP_STATUS		0x00
 #define AVS_RO_TEMP_STATUS_valid_msk	(BIT(16) | BIT(10))
 #define AVS_RO_TEMP_STATUS_data_msk	GENMASK(9, 0)

struct bcm2711_thermal_priv {
	void __iomem *base;
	struct device *dev;
	struct clk *clk;
	struct thermal_zone_device *thermal;
};

static int bcm2711_get_temp(void *data, int *temp)
{
	struct bcm2711_thermal_priv *priv = data;
	int slope = thermal_zone_get_slope(priv->thermal);
	int offset = thermal_zone_get_offset(priv->thermal);
	u32 val;
	long t;

	val = __raw_readl(priv->base + AVS_RO_TEMP_STATUS);

	if (!(val & AVS_RO_TEMP_STATUS_valid_msk)) {
		dev_err(priv->dev, "reading not valid\n");
		return -EIO;
	}

	val &= AVS_RO_TEMP_STATUS_data_msk;

	/* Convert a HW code to a temperature reading (millidegree celsius) */
	t = slope * val + offset;
	if (t < 0)
		*temp = 0;
	else
		*temp = t;

	return 0;
}

static const struct thermal_zone_of_device_ops bcm2711_thermal_of_ops = {
	.get_temp	= bcm2711_get_temp,
};

static const struct of_device_id bcm2711_thermal_id_table[] = {
	{ .compatible = "brcm,bcm2711-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, bcm2711_thermal_id_table);

static int bcm2711_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal;
	struct bcm2711_thermal_priv *priv;
	struct resource *res;
	int ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->base))
		return PTR_ERR(priv->base);

	priv->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(priv->clk)) {
		ret = PTR_ERR(priv->clk);
		if (ret != -EPROBE_DEFER)
			dev_err(&pdev->dev, "could not get clk: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	thermal = devm_thermal_zone_of_sensor_register(&pdev->dev, 0, priv,
						       &bcm2711_thermal_of_ops);
	if (IS_ERR(thermal)) {
		ret = PTR_ERR(thermal);
		dev_err(&pdev->dev, "could not register sensor: %d\n", ret);
		goto err_clk;
	}

	priv->thermal = thermal;

	thermal->tzp->no_hwmon = false;
	ret = thermal_add_hwmon_sysfs(thermal);
	if (ret)
		return ret;

	return 0;

err_clk:
	clk_disable_unprepare(priv->clk);

	return ret;
}

static int bcm2711_thermal_remove(struct platform_device *pdev)
{
	struct bcm2711_thermal_priv *priv = platform_get_drvdata(pdev);

	clk_disable_unprepare(priv->clk);

	return 0;
}

static struct platform_driver bcm2711_thermal_driver = {
	.probe = bcm2711_thermal_probe,
	.remove = bcm2711_thermal_remove,
	.driver = {
		.name = "bcm2711_thermal",
		.of_match_table = bcm2711_thermal_id_table,
	},
};
module_platform_driver(bcm2711_thermal_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stefan Wahren");
MODULE_DESCRIPTION("Broadcom AVS RO thermal sensor driver");
