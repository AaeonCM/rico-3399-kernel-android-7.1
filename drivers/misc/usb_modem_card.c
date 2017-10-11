#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>

static struct class *usb_modem_card_class;

struct usb_modem_card {
	struct device *dev;
	struct device sys_dev;

	struct gpio_desc *enable_gpio;
};

static ssize_t enable_show(struct device *sys_dev,
				struct device_attribute *attr,
				char *buf)
{
	struct usb_modem_card *gpiod = container_of(sys_dev, struct usb_modem_card,
						sys_dev);

	return sprintf(buf, "%d\n", gpiod_get_value(gpiod->enable_gpio));
}

static ssize_t enable_store(struct device *sys_dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct usb_modem_card *gpiod = container_of(sys_dev, struct usb_modem_card,
						sys_dev);
	int val = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return ret;

	if (val)
		val = 1;
	gpiod_set_value(gpiod->enable_gpio, val);

	return count;
}
static DEVICE_ATTR_RW(enable);

static struct attribute *usb_modem_card_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};
ATTRIBUTE_GROUPS(usb_modem_card);

static int usb_modem_card_device_register(struct usb_modem_card *gpiod)
{
	int ret;
	struct device *dev = &gpiod->sys_dev;
	const char *name = {"enable"};

	dev->class = usb_modem_card_class;
	dev_set_name(dev, "%s", name);
	dev_set_drvdata(dev, gpiod);
	ret = device_register(dev);

	return ret;
}

static int usb_modem_card_probe(struct platform_device *pdev)
{
	struct usb_modem_card *gpiod;
	struct device *dev = &pdev->dev;
	int ret = 0;

	usb_modem_card_class = class_create(THIS_MODULE, "usb_modem_card");
	if (IS_ERR(usb_modem_card_class)) {
		dev_err(dev, "%s: failed to create class, err=%ld\n", __func__,
				PTR_ERR(usb_modem_card_class));
		return PTR_ERR(usb_modem_card_class);
	}
	usb_modem_card_class->dev_groups = usb_modem_card_groups;

	gpiod = devm_kzalloc(dev, sizeof(*gpiod), GFP_KERNEL);
	if (!gpiod)
		return -ENOMEM;

	gpiod->dev = dev;

	ret = usb_modem_card_device_register(gpiod);
	if (ret < 0) {
		dev_err(dev, "%s: failed to register device\n", __func__);
		goto error_alloc;
	}

	gpiod->enable_gpio = devm_gpiod_get_optional(gpiod->dev,
								"enable", 0);
	if (IS_ERR(gpiod->enable_gpio)) {
		dev_warn(dev, "%s: failed to get enable-gpios!\n", __func__);
		gpiod->enable_gpio = NULL;
	}

	if (gpiod->enable_gpio) {
		ret = gpiod_direction_output(gpiod->enable_gpio, 0);
		if (ret < 0) {
			dev_err(dev, "%s: failed to pull down enable gpio\n", __func__);
		}
		msleep(50);
		ret = gpiod_direction_output(gpiod->enable_gpio, 1);
		if (ret < 0) {
			dev_err(dev, "%s: failed to pull up enable gpio\n", __func__);
		}
	}

	dev_info(dev, "%s: probe success\n", __func__);

	return 0;

error_alloc:
	class_destroy(usb_modem_card_class);
	return ret;
}

static const struct of_device_id usb_modem_card_match[] = {
	{ .compatible = "usb-modem-card" },
	{ /* sentinel */ }
};

static int usb_modem_card_remove(struct platform_device *pdev)
{
	if (!IS_ERR(usb_modem_card_class))
		class_destroy(usb_modem_card_class);

	return 0;
}

static struct platform_driver usb_modem_card_driver = {
	.probe = usb_modem_card_probe,
	.remove = usb_modem_card_remove,
	.driver = {
		.name = "usb_modem_card",
		.owner = THIS_MODULE,
		.of_match_table	= usb_modem_card_match,
	},
};

module_platform_driver(usb_modem_card_driver);

MODULE_ALIAS("platform:usb_modem_card");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("USB Modem Card Driver");
