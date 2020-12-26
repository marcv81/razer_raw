#include <linux/cdev.h>
#include <linux/hid.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/usb/input.h>

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define RAZER_MAX_DEVICES (10)

#define RAZER_VID (0x1532)
#define RAZER_PID_DEATHADDER_V2_PRO_WIRED (0x007c)
#define RAZER_PID_DEATHADDER_V2_PRO_WIRELESS (0x007d)

#define RAZER_REPORT_LENGTH (90)
#define RAZER_REPORT_VALUE (0x0300)
#define RAZER_REPORT_INDEX (0x0000)

/*
 * Global variables.
 */

struct razer_device
{
    int minor;
    struct cdev cdev;
    struct usb_device *usb_dev;
};

// Devices pool, indexed by minor ID.
static struct razer_device *razer_device[RAZER_MAX_DEVICES];

// Mutex to probe/remove devices one at a time.
static DEFINE_MUTEX(razer_lock);

static int razer_major;
static struct class *razer_class;

/*
 * Forward declarations.
 */

static int razer_custom_init(void);
static void razer_custom_exit(void);

static int razer_custom_probe(struct hid_device *hid_dev, const struct hid_device_id *id);
static void razer_custom_remove(struct hid_device *hid_dev);

static int razer_next_minor(void);

static ssize_t razer_read(struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t razer_write(struct file *file, const char __user *buf, size_t count, loff_t *offset);

static ssize_t razer_check_params(char *msg, struct file *file, size_t count, loff_t *offset);
static ssize_t razer_check_length(ssize_t length);

/*
 * Driver and devices initialization. Performs the standard HID initialization
 * and calls our custom driver init/exit and device probe/remove functions.
 */

MODULE_LICENSE("GPL");

static const struct hid_device_id razer_id_table[] = {
    {HID_USB_DEVICE(RAZER_VID, RAZER_PID_DEATHADDER_V2_PRO_WIRED)},
    {HID_USB_DEVICE(RAZER_VID, RAZER_PID_DEATHADDER_V2_PRO_WIRELESS)},
    {0},
};

MODULE_DEVICE_TABLE(hid, razer_id_table);

static int razer_init(struct hid_driver *hid_drv)
{
    int retval;

    // Custom driver initialization.
    retval = razer_custom_init();
    if (retval < 0)
        goto recover_0;

    // HID driver initialization.
    retval = hid_register_driver(hid_drv);
    if (retval < 0)
    {
        pr_err("hid_register_driver error %d\n", retval);
        goto recover_1;
    }

    return 0;

recover_1:
    razer_custom_exit();
recover_0:
    return retval;
}

static void razer_exit(struct hid_driver *hid_drv)
{
    hid_unregister_driver(hid_drv);
    razer_custom_exit();
}

static int razer_probe(struct hid_device *hid_dev, const struct hid_device_id *id)
{
    int retval;

    // Parses the HID report descriptor.
    retval = hid_parse(hid_dev);
    if (retval < 0)
    {
        hid_err(hid_dev, "hid_parse error %d\n", retval);
        goto recover_0;
    }

    // HID device initialization.
    retval = hid_hw_start(hid_dev, HID_CONNECT_DEFAULT);
    if (retval < 0)
    {
        hid_err(hid_dev, "hid_hw_start error %d\n", retval);
        goto recover_0;
    }

    // Custom device initialization.
    retval = razer_custom_probe(hid_dev, id);
    if (retval < 0)
        goto recover_1;

    return 0;

recover_1:
    hid_hw_stop(hid_dev);
recover_0:
    return retval;
}

static void razer_remove(struct hid_device *hid_dev)
{
    razer_custom_remove(hid_dev);
    hid_hw_stop(hid_dev);
}

static struct hid_driver razer_hid_driver = {
    .name = "razer_raw",
    .id_table = razer_id_table,
    .probe = razer_probe,
    .remove = razer_remove,
};

module_driver(razer_hid_driver, razer_init, razer_exit);

/*
 * Custom driver init/exit and device probe/remove functions definition.
 * Associates character devices to USB devices and registers them with sysfs.
 */

static int razer_custom_init(void)
{
    int retval;
    dev_t dev;

    // Registers the range of character devices.
    retval = alloc_chrdev_region(&dev, 0, RAZER_MAX_DEVICES, "razer");
    if (retval < 0)
    {
        pr_err("alloc_chrdev_region error %d\n", retval);
        goto recover_0;
    }
    razer_major = MAJOR(dev);

    // Creates the device class.
    razer_class = class_create(THIS_MODULE, "razer");
    if (IS_ERR(razer_class))
    {
        retval = PTR_ERR(razer_class);
        pr_err("class_create error %d\n", retval);
        goto recover_1;
    }

    return 0;

recover_1:
    unregister_chrdev_region(MKDEV(razer_major, 0), MINORMASK);
recover_0:
    return retval;
}

static void razer_custom_exit(void)
{
    class_unregister(razer_class);
    class_destroy(razer_class);
    unregister_chrdev_region(MKDEV(razer_major, 0), MINORMASK);
}

static const struct file_operations razer_fops = {.owner = THIS_MODULE, .read = razer_read, .write = razer_write};

static int razer_custom_probe(struct hid_device *hid_dev, const struct hid_device_id *id)
{
    int minor, retval;
    struct device *dev;
    struct usb_interface *usb_if;

    // Only proceed if this is the primary interface. Otherwise if a USB device
    // has multiple interfaces we might create duplicate character devices.
    usb_if = to_usb_interface(hid_dev->dev.parent);
    if (usb_if->cur_altsetting->desc.bInterfaceNumber != 0)
        return 0;

    mutex_lock(&razer_lock);

    // Finds an available minor ID in the devices pool.
    minor = razer_next_minor();
    if (minor < 0)
    {
        retval = -EINVAL;
        pr_err("out of minor IDs\n");
        goto recover_0;
    }

    // Allocates memory for the device data.
    razer_device[minor] = kzalloc(sizeof(struct razer_device), GFP_KERNEL);
    if (razer_device[minor] == NULL)
    {
        retval = -ENOMEM;
        pr_err("out of memory\n");
        goto recover_0;
    }

    razer_device[minor]->minor = minor;
    razer_device[minor]->usb_dev = interface_to_usbdev(usb_if);

    // Adds the character device to the system.
    cdev_init(&razer_device[minor]->cdev, &razer_fops);
    razer_device[minor]->cdev.owner = THIS_MODULE;
    retval = cdev_add(&razer_device[minor]->cdev, MKDEV(razer_major, minor), 1);
    if (retval < 0)
    {
        pr_err("cdev_add error %d\n", retval);
        goto recover_1;
    }

    hid_set_drvdata(hid_dev, razer_device[minor]);

    // Registers the device with sysfs. Exposes it to userspace and creates the device node.
    dev = device_create(razer_class, NULL, MKDEV(razer_major, minor), NULL, "razer%d", minor);
    if (IS_ERR(dev))
    {
        retval = PTR_ERR(dev);
        pr_err("device_create error %d\n", retval);
        goto recover_2;
    }

    mutex_unlock(&razer_lock);
    return 0;

recover_2:
    cdev_del(&razer_device[minor]->cdev);
recover_1:
    kfree(razer_device[minor]);
    razer_device[minor] = NULL;
recover_0:
    mutex_unlock(&razer_lock);
    return retval;
}

static void razer_custom_remove(struct hid_device *hid_dev)
{
    int minor;
    struct razer_device *razer_dev;

    // Only proceed if there is an associated character device.
    razer_dev = hid_get_drvdata(hid_dev);
    if (razer_dev == NULL)
        return;

    mutex_lock(&razer_lock);

    minor = razer_dev->minor;
    device_destroy(razer_class, MKDEV(razer_major, minor));
    cdev_del(&razer_device[minor]->cdev);
    kfree(razer_device[minor]);
    razer_device[minor] = NULL;

    mutex_unlock(&razer_lock);
}

static int razer_next_minor(void)
{
    int i;
    for (i = 0; i < RAZER_MAX_DEVICES; i++)
    {
        if (razer_device[i] == NULL)
            return i;
    }
    return -1;
}

/*
 * Character devices read/write operations.
 */

static ssize_t razer_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    int minor;
    char *data;
    ssize_t retval;
    struct usb_device *usb_dev;

    retval = razer_check_params("read", file, count, offset);
    if (retval < 0)
        goto final_0;

    // Allocates memory for the response.
    data = kmalloc(RAZER_REPORT_LENGTH, GFP_KERNEL);
    if (data == NULL)
    {
        retval = -ENOMEM;
        pr_err("out of memory\n");
        goto final_0;
    }

    minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
    usb_dev = razer_device[minor]->usb_dev;
    retval = razer_check_length(usb_control_msg(usb_dev, usb_rcvctrlpipe(usb_dev, 0), HID_REQ_GET_REPORT,
                                                USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN, RAZER_REPORT_VALUE,
                                                RAZER_REPORT_INDEX, data, RAZER_REPORT_LENGTH, USB_CTRL_SET_TIMEOUT));
    if (retval < 0)
        goto final_1;

    // Copies the response to the user buffer.
    if (copy_to_user(buf, data, RAZER_REPORT_LENGTH) != 0)
    {
        retval = -EFAULT;
        pr_err("copy_to_user error\n");
    }

final_1:
    kfree(data);
final_0:
    return retval;
}

static ssize_t razer_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
    int minor;
    char *data;
    ssize_t retval;
    struct usb_device *usb_dev;

    retval = razer_check_params("write", file, count, offset);
    if (retval < 0)
        goto final_0;

    // Allocates memory for the request.
    data = kmalloc(RAZER_REPORT_LENGTH, GFP_KERNEL);
    if (data == NULL)
    {
        retval = -ENOMEM;
        pr_err("out of memory\n");
        goto final_0;
    }

    // Copies the user buffer to the request.
    if (copy_from_user(data, buf, RAZER_REPORT_LENGTH) != 0)
    {
        retval = -EFAULT;
        pr_err("copy_from_user error\n");
        goto final_1;
    }

    minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
    usb_dev = razer_device[minor]->usb_dev;
    retval = razer_check_length(usb_control_msg(usb_dev, usb_sndctrlpipe(usb_dev, 0), HID_REQ_SET_REPORT,
                                                USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_OUT, RAZER_REPORT_VALUE,
                                                RAZER_REPORT_INDEX, data, RAZER_REPORT_LENGTH, USB_CTRL_SET_TIMEOUT));

final_1:
    kfree(data);
final_0:
    return retval;
}

static ssize_t razer_check_params(char *msg, struct file *file, size_t count, loff_t *offset)
{
    if (file->f_flags & O_NONBLOCK)
    {
        pr_info("%s failed: non-blocking not supported\n", msg);
        return -EIO;
    }

    if (*offset != 0)
    {
        pr_info("%s failed: offset=%lld, expected 0\n", msg, *offset);
        return -EIO;
    }

    if (count != RAZER_REPORT_LENGTH)
    {
        pr_info("%s failed: count=%ld, expected %d\n", msg, count, RAZER_REPORT_LENGTH);
        return -EIO;
    }

    return 0;
}

static ssize_t razer_check_length(ssize_t length)
{
    if (length < 0)
    {
        pr_err("usb_control_msg error %ld\n", length);
        return length;
    }

    if (length != RAZER_REPORT_LENGTH)
    {
        pr_err("usb_control_msg error: length=%ld, expected %d\n", length, RAZER_REPORT_LENGTH);
        return -EIO;
    }

    return length;
}
