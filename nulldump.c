#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>

#define DEVICE_NAME "nulldump"
#define CLASS_NAME  "nulldump_class"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("KDA");
MODULE_DESCRIPTION("nulldump");

static dev_t dev;
static struct cdev chrdev_cdev;
static struct class *chrdev_class;
static struct device *sdev;

static void my_print_hex_dump(const char *data, size_t len)
{
	char *buf;
	size_t i;
	int offset = 0;

	if (len == 0)
	{
		pr_info("nulldump: empty data\n");
		return;
	}

	buf = kmalloc(256, GFP_KERNEL);
	if (!buf)
		return;

	pr_info("nulldump: hex dump of %zu bytes:\n", len);

	for (int i = 0; i < len; i++) {
		if (i % 16 == 0) {
			if (i > 0) {
				buf[offset] = '\0';
				pr_info("%s", buf);
				offset = 0;
			}
			offset += snprintf(buf + offset, 256 - offset, "  %04zx: ", i);
		}
		offset += snprintf(buf + offset, 256 - offset, "%02x ", (unsigned char)data[i]);
	}

	if (offset > 0) {
		buf[offset] = '\0';
		pr_info("%s", buf);
	}

	kfree(buf);
}

static ssize_t nulldump_read(struct file *file, char __user *buffer, 
                             size_t length, loff_t *offset)
{
	pr_info("nulldump: read attempt by process %d (%s), requested %zu bytes\n", 
			current->pid, current->comm, length);
	return 0;
}

static ssize_t nulldump_write(struct file *file, const char __user *buffer, 
                              size_t length, loff_t *offset)
{
	char *kernel_buffer;
	size_t bytes_not_copied;

	pr_info("nulldump: write attempt by process %d (%s), %zu bytes\n",
		       current->pid, current->comm, length);	

	kernel_buffer = kmalloc(length, GFP_KERNEL);
	if (!kernel_buffer)
	{
		pr_err("nulldump: kmalloc failed\n");
		return -ENOMEM;
	}

	bytes_not_copied = copy_from_user(kernel_buffer, buffer, length);
	if (bytes_not_copied > 0) {
		pr_warn("nulldump: failed to copy %zu bytes from user\n",
				bytes_not_copied);
		kfree(kernel_buffer);
		return -EFAULT;
	}

	my_print_hex_dump(kernel_buffer, length);

	kfree(kernel_buffer);

	return length;
}

static int nulldump_open(struct inode *inode, struct file *file)
{
	pr_info("nulldump: opened by process %d (%s)\n",
			current->pid, current->comm);
	return 0;
}

static int nulldump_release(struct inode *inode, struct file *file)
{
	pr_info("nulldump: closed by process %d (%s)\n",
			current->pid, current->comm);
	return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = nulldump_open,
    .release = nulldump_release,
    .read = nulldump_read,
    .write = nulldump_write,
};

static int __init nulldump_init(void)
{
	int ret;

	if ((ret = alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)))
	{
		pr_err("nulldump: failed to allocate device number\n");
		return ret;
	}

	pr_info("nulldump: registered (major=%d, minor=%d)\n", MAJOR(dev), MINOR(dev));

	cdev_init(&chrdev_cdev, &fops);
	chrdev_cdev.owner = THIS_MODULE;

	if ((ret = cdev_add(&chrdev_cdev, dev, 1)))
	{
		pr_err("nulldump: cdev_add failed\n");
		goto err_unregister;
	}

	if (IS_ERR(chrdev_class = class_create(CLASS_NAME)))
	{
		pr_err("nulldump: class_create failed\n");
		ret = PTR_ERR(chrdev_class);
		goto err_cdev_del;
	}

	if (IS_ERR(sdev = device_create(chrdev_class, NULL, dev, NULL, DEVICE_NAME)))
	{
		pr_err("nulldump: device_create failed\n");
		ret = PTR_ERR(sdev);
		goto err_class_destroy;
	}

	pr_info("nulldump: module loaded\n");
	return 0;

err_class_destroy:
	class_destroy(chrdev_class);
err_cdev_del:
	cdev_del(&chrdev_cdev);
err_unregister:
	unregister_chrdev_region(dev, 1);
	return ret;
}

static void __exit nulldump_exit(void)
{
	device_destroy(chrdev_class, dev);
	class_destroy(chrdev_class);
	cdev_del(&chrdev_cdev);
	unregister_chrdev_region(dev, 1);

	pr_info("nulldump: module unloaded\n");
}

module_init(nulldump_init);
module_exit(nulldump_exit);
