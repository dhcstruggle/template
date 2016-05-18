/*char dev template*/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/device.h>

#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>

#define CHAR_MAJOR   12    //主设备号cat /proc/devices找一个尚未使用的
#define CHAR_MINOR   12    //次设备号
#define CHAR_COUNT   1

#define CHARNAME "char_dev"

#define CHAR_DEV_IOCTL_PRINT   _IOW('C', 0X1, int)

static int char_major = 0;
static int char_minor = 0;
static dev_t devno;

//自动创建设备文件(结点)
struct class *dev_class = NULL;
struct device *dev_device = NULL;

/*
insmod char_dev.ko char_major=100 char_minor=200
*/
module_param(char_major, int, S_IRUGO);
module_param(char_minor, int, S_IRUGO);

struct cdev cdev;  //字符设备描述符

/*
使用下边两个宏从inode中获得主设备号和此设备号
unsigned int iminor(struct inode *inode);
unsigned int imajor(struct inode *inode);
*/
int char_open(struct inode *inode, struct file *filp)
{
    return 0;
}

int char_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t char_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
    return 0;
}

static ssize_t char_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
    return 0;
}

#if 0
static loff_t char_llseek(struct file *filp, loff_t offset, int whence)
{
    return 0;
}
#endif

static long char_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case CHAR_DEV_IOCTL_PRINT:
            {
                char buf[64];

                if (copy_from_user(&buf, (void __user*)arg, sizeof(buf)) != 0)
                    return -EFAULT;
                printk("char dev printk:%s\n", buf);
                break;
            }
        default:
            return  - EINVAL;
  }

  return 0;
}

static const struct file_operations char_fops =
{
    .owner = THIS_MODULE,
    //.llseek = mem_llseek,
    .read = char_read,
    .write = char_write,
    .open = char_open,
    .release = char_release,
    .unlocked_ioctl = char_ioctl,
};
/*
MAJOR(dev_t dev);
MINOR(dev_t dev);
MKDEV(int major,int minor);
*/
static int chardev_init(void)
{
    int ret;

    devno = MKDEV(char_major, char_minor);
    /* 静态申请设备号*/
    if (char_major)
        ret = register_chrdev_region(devno, CHAR_COUNT, CHARNAME);
    else { /* 动态分配设备号 */
        ret = alloc_chrdev_region(&devno, char_minor, CHAR_COUNT, CHARNAME);
        char_major = MAJOR(devno);
        char_minor = MINOR(devno);
    }

    if(ret < 0){
        printk("register_chrdev_region failed!\n");
        goto failure_register_chrdev;
    }

    printk("char_major = %d\n", char_major);
    printk("char_minor = %d\n", char_minor);
    printk("char_name  = %s\n", CHARNAME);
    /*初始化cdev结构*/
    cdev_init(&cdev, &char_fops);
    cdev.owner = THIS_MODULE;

    /* 注册字符设备 */
    ret = cdev_add(&cdev, MKDEV(char_major, 0), 1);
    if(ret < 0){
        printk("cdev_add failed!\n");
        goto failure_cdev_add;
    }

    /*自动创建设备节点文件*/
    //1.注册设备类  /sys/class/char_class的文件夹
    dev_class = class_create(THIS_MODULE, "char_class");
    if(IS_ERR(dev_class)){
        printk("class_create failed!\n");
        ret = PTR_ERR("dev_class");
        goto failure_class_create;
    }
    //2.注册设备  /sys/class/char_class/char0   /dev/char0
    dev_device = device_create(dev_class, NULL, devno, NULL, "char%d", char_minor);
    if(IS_ERR(dev_device)){
        printk("device_create failed!\n");
        ret = PTR_ERR(dev_device);
        goto failure_device_create;
    }
    return 0;

failure_device_create:
    class_destroy(dev_class);
failure_class_create:
    cdev_del(&cdev);
failure_cdev_add:
    unregister_chrdev_region(devno, CHAR_COUNT);
failure_register_chrdev:
    return ret;

}

static void chardev_exit(void)
{
/*逆序消除*/
    //从内核中删除设备
    device_destroy(dev_class, devno);
    //从内核中删除设备类
    class_destroy(dev_class);
    //从内核中删除cdev
    cdev_del(&cdev);
    //注销设备号
    unregister_chrdev_region(devno, CHAR_COUNT);
    printk("rmmod char_dev success\n");
}

MODULE_AUTHOR("chaos");
MODULE_LICENSE("GPL");

module_init(chardev_init);
module_exit(chardev_exit);

