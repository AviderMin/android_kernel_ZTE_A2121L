#include <linux/init.h>
#include <linux/module.h>
#include <linux/miscdevice.h> /* misc_register */
#include <linux/fs.h> /* struct file_operations */
#include <linux/device.h>

MODULE_LICENSE("Dual BSD/GPL");

#define debug(fmt, ...) \
	pr_debug("zte_boardid: " fmt, ##__VA_ARGS__)

#define MAX_SIZE 50
#define EFUSE_ENABLE_VALUE 5

static int efuse_state = -1;
static int __init efuse_state_setup(char *str)
{
	long value;
	int err;

	err = kstrtol(str, 0, &value);
	if (err)
		return err;
	if (value == EFUSE_ENABLE_VALUE)
		efuse_state = 1;
	else
		efuse_state = 0;
	return 1;
}
__setup("efuse_state=", efuse_state_setup);

int zte_get_efuse_state(void)
{
	return efuse_state;
}
EXPORT_SYMBOL(zte_get_efuse_state);

static ssize_t zte_efuse_state_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	return snprintf(buf, MAX_SIZE, "%d\n", efuse_state);
}

static DEVICE_ATTR(efuse_state_show, S_IRUGO, zte_efuse_state_show, NULL);

static struct attribute *mvd_attrs[] = {
	&dev_attr_efuse_state_show.attr,
	NULL,
};

static struct attribute_group mvd_attr_group = {
	.attrs = mvd_attrs,
};

static int zte_boardid_misc_open(struct inode *inode, struct file *file)
{
	debug("open\n");
	return 0;
}

static long zte_boardid_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	debug("ioctl\n");

	return 0;
}

static struct file_operations zte_boardid_misc_fops = {
	.owner = THIS_MODULE,
	.open = zte_boardid_misc_open,
	.unlocked_ioctl = zte_boardid_misc_ioctl,
};

static struct miscdevice zte_boardid_misc_dev[] = {
	{
		.minor    = MISC_DYNAMIC_MINOR,
		.name    = "zte_boardid",
		.fops    = &zte_boardid_misc_fops,
	}
};

static int __init zte_boardid_init(void)
{
	int ret = 0;

	debug("zte_boardid init\n");

	ret = misc_register(&zte_boardid_misc_dev[0]);
	if (ret) {
		debug("fail to register misc driver: %d\n", ret);
		goto register_fail;
	}

	/*ret = device_create_file(zte_boardid_misc_dev[0].this_device, &dev_attr_id);*/
	ret  = sysfs_create_group(&zte_boardid_misc_dev[0].this_device->kobj, &mvd_attr_group);
	if (ret) {
		debug("fail to create file: %d\n", ret);
		goto register_fail;
	}
	ret = zte_get_efuse_state();
	pr_debug("zte_get_efuse_state(%d)\n", ret);

	return 0;

register_fail:
	return ret;
}

static void __exit zte_boardid_exit(void)
{
	debug("zte_boardid exit\n");
	sysfs_remove_group(&zte_boardid_misc_dev[0].this_device->kobj, &mvd_attr_group);
	misc_deregister(&zte_boardid_misc_dev[0]);
}

module_init(zte_boardid_init);
module_exit(zte_boardid_exit);
