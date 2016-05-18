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

#define CHAR_MAJOR   12    //���豸��cat /proc/devices��һ����δʹ�õ�
#define CHAR_MINOR   12    //���豸��
#define CHAR_COUNT   1

#define CHARNAME "char_dev"

#define CHAR_DEV_IOCTL_PRINT   _IOW('C', 0X1, int)

static int char_major = 0;
static int char_minor = 0;
static dev_t devno;

//�Զ������豸�ļ�(���)
struct class *dev_class = NULL;
struct device *dev_device = NULL;

/*
insmod char_dev.ko char_major=100 char_minor=200
*/
module_param(char_major, int, S_IRUGO);
module_param(char_minor, int, S_IRUGO);

struct cdev cdev;  //�ַ��豸������

/*
ʹ���±��������inode�л�����豸�źʹ��豸��
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
    /* ��̬�����豸��*/
    if (char_major)
        ret = register_chrdev_region(devno, CHAR_COUNT, CHARNAME);
    else { /* ��̬�����豸�� */
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
    /*��ʼ��cdev�ṹ*/
    cdev_init(&cdev, &char_fops);
    cdev.owner = THIS_MODULE;

    /* ע���ַ��豸 */
    ret = cdev_add(&cdev, MKDEV(char_major, 0), 1);
    if(ret < 0){
        printk("cdev_add failed!\n");
        goto failure_cdev_add;
    }

    /*�Զ������豸�ڵ��ļ�*/
    //1.ע���豸��  /sys/class/char_class���ļ���
    dev_class = class_create(THIS_MODULE, "char_class");
    if(IS_ERR(dev_class)){
        printk("class_create failed!\n");
        ret = PTR_ERR("dev_class");
        goto failure_class_create;
    }
    //2.ע���豸  /sys/class/char_class/char0   /dev/char0
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
/*��������*/
    //���ں���ɾ���豸
    device_destroy(dev_class, devno);
    //���ں���ɾ���豸��
    class_destroy(dev_class);
    //���ں���ɾ��cdev
    cdev_del(&cdev);
    //ע���豸��
    unregister_chrdev_region(devno, CHAR_COUNT);
    printk("rmmod char_dev success\n");
}

MODULE_AUTHOR("chaos");
MODULE_LICENSE("GPL");

module_init(chardev_init);
module_exit(chardev_exit);

