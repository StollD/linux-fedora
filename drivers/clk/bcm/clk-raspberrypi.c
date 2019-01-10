// SPDX-License-Identifier: GPL-2.0+
/*
 * Raspberry Pi CPU clock driver
 *
 * Copyright (C) 2018 Stefan Wahren <stefan.wahren@i2se.com>
 */

#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <soc/bcm2835/raspberrypi-firmware.h>

#define VCMSG_ID_ARM_CLOCK 0x000000003	/* Clock/Voltage ID's */

struct rpi_cpu_clkgen {
	struct clk_hw hw;
	struct rpi_firmware *fw;
};

/* tag part of the message */
struct prop {
       u32 id;			/* the ID of the clock/voltage to get or set */
       u32 val;			/* the value (e.g. rate (in Hz)) to set */
} __packed;

static int rpi_cpu_clock_property(struct rpi_firmware *fw, u32 tag, u32 *val)
{
	int ret;
	struct prop msg = {
		.id = VCMSG_ID_ARM_CLOCK,
		.val = *val,
	};

	ret = rpi_firmware_property(fw, tag, &msg, sizeof(msg));
	if (ret)
		return ret;

	*val = msg.val;

	return 0;
}

static unsigned long rpi_cpu_get_rate(struct clk_hw *hw,
				      unsigned long parent_rate)
{
	struct rpi_cpu_clkgen *cpu = container_of(hw, struct rpi_cpu_clkgen, hw);
	u32 rate = 0;

	rpi_cpu_clock_property(cpu->fw, RPI_FIRMWARE_GET_CLOCK_RATE, &rate);

	return rate;
}

static long rpi_cpu_round_rate(struct clk_hw *hw, unsigned long rate,
				     unsigned long *parent_rate)
{
	return rate;
}

static int rpi_cpu_set_rate(struct clk_hw *hw, unsigned long rate,
			    unsigned long parent_rate)
{
	struct rpi_cpu_clkgen *cpu = container_of(hw, struct rpi_cpu_clkgen, hw);
	u32 new_rate = rate;

	return rpi_cpu_clock_property(cpu->fw, RPI_FIRMWARE_SET_CLOCK_RATE,
				      &new_rate);
}

static const struct clk_ops rpi_cpu_ops = {
	.recalc_rate = rpi_cpu_get_rate,
	.round_rate = rpi_cpu_round_rate,
	.set_rate = rpi_cpu_set_rate,
};

static int rpi_cpu_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *fw_node;
	struct rpi_cpu_clkgen *cpu;
	struct clk_init_data *init;
	int ret;

	cpu = devm_kzalloc(dev, sizeof(*cpu), GFP_KERNEL);
	if (!cpu)
		return -ENOMEM;

	init = devm_kzalloc(dev, sizeof(*init), GFP_KERNEL);
	if (!init)
		return -ENOMEM;

	fw_node = of_find_compatible_node(NULL, NULL,
					  "raspberrypi,bcm2835-firmware");
	if (!fw_node) {
		dev_err(dev, "Missing firmware node\n");
		return -ENOENT;
	}

	cpu->fw = rpi_firmware_get(fw_node);
	of_node_put(fw_node);
	if (!cpu->fw)
		return -EPROBE_DEFER;

	init->name = dev->of_node->name;
	init->ops = &rpi_cpu_ops;

	cpu->hw.init = init;
	ret = devm_clk_hw_register(dev, &cpu->hw);
	if (ret)
		return ret;

	return of_clk_add_hw_provider(dev->of_node, of_clk_hw_simple_get,
				      &cpu->hw);
}

static const struct of_device_id rpi_cpu_of_match[] = {
	{ .compatible = "raspberrypi,bcm2835-cpu", },
	{},
};
MODULE_DEVICE_TABLE(of, rpi_cpu_of_match);

static struct platform_driver rpi_cpu_driver = {
	.driver = {
		.name = "raspberrypi-cpu",
		.of_match_table = rpi_cpu_of_match,
	},
	.probe		= rpi_cpu_probe,
};
builtin_platform_driver(rpi_cpu_driver);

MODULE_AUTHOR("Stefan Wahren <stefan.wahren@i2se.com>");
MODULE_DESCRIPTION("Raspberry Pi CPU clock driver");
MODULE_LICENSE("GPL v2");
