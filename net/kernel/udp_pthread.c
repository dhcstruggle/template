/*
 * ChangeLog:
 * v0.1  - creat file
 */


 /*****************************************************************************/
 /* include */
 /*****************************************************************************/
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/device.h>
#include <linux/cdev.h>

/*****************************************************************************/
/* define */
/*****************************************************************************/

#define CONFIG_USE_SIM_DATA 0
#define CONFIG_RECEIVE_ENABLE  1

#define IP_ADDR_DEFAULT "10.3.16.154"
#define IP_REMOTE_PORT 22225
#define IP_LOCAL_PORT  22223

#define DATA_SEND_CNT 4492

#define DATA_RECV_CNT 652
#define DATA_RECV_DATA_HAEDER_CNT 12
#define DATA_RECV_DATA_CNT   640

#define MAJORNR  200
#define MINORNR  100

/*****************************************************************************/
/* globals */
/*****************************************************************************/
struct socket *p_sock;
struct socket sock;

static struct task_struct *Send_tsk;
static struct task_struct *Recv_tsk;

char start_flag = 0;
char G_senddata[DATA_SEND_CNT];
struct kfifo G_st_Spkfifo;

#if CONFIG_USE_SIM_DATA
unsigned char Sim_data[DATA_SEND_CNT];
#endif

static dev_t devno;
struct cdev cdev;
struct class *dev_class = NULL;
struct device *dev_device = NULL;

typedef struct{
    unsigned int u32_SendFrameCnt;
    unsigned int u32_RecvFrameCnt;
    unsigned short u16_SeqNumberNowSending;
    unsigned short u16_SeqNumberNowRecving;
}trans_status;

trans_status G_st_trans_status;

char ipaddr[20] = IP_ADDR_DEFAULT;

bool init_flag = false;
char print_flag = 0;
bool recv_flag = false;
/*****************************************************************************/
/*  CMD */
/*****************************************************************************/
#define DEV_IOCTL_PRINT_STAT    _IOR('D', 0X1, trans_status)
#define DEV_IOCTL_START         _IO('D', 0X2)
#define DEV_IOCTL_STOP          _IO('D', 0X3)
#define DEV_IOCTL_IPADDR        _IOW('D', 0X4, char)
#define DEV_IOCTL_PRINT         _IOW('D', 0X5, char)

/*****************************************************************************/
/*  EXPORT */
/*****************************************************************************/
unsigned int RTchain_slave2host_data_cb(void *data, unsigned int len)
{
    if (init_flag)
    {
        if (data != NULL)
        {
#if CONFIG_USE_SIM_DATA
            memcpy(G_senddata, Sim_data, DATA_SEND_CNT);
#else
            memcpy(G_senddata, data, DATA_SEND_CNT);
#endif
        }
        else
        {
            printk("data addr can not be NULL\n");
            return -1;
        }

        start_flag = 1;
        wake_up_process(Send_tsk);
    }
    return 1;
}

ssize_t ksend(struct socket * p_socket, const void *buffer, size_t length, int flags)
{
    struct socket *sk;
    struct msghdr msg;
    struct iovec iov;
    int len;
#ifndef KSOCKET_ADDR_SAFE
    mm_segment_t old_fs;
#endif

    sk = p_socket;

    iov.iov_base = (void *)buffer;
    iov.iov_len = (__kernel_size_t)length;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    msg.msg_flags = flags;

#ifndef KSOCKET_ADDR_SAFE
    old_fs = get_fs();
    set_fs(KERNEL_DS);
#endif
    len = sock_sendmsg(sk, &msg, length);
#ifndef KSOCKET_ADDR_SAFE
    set_fs(old_fs);
#endif

    return len;//len ?
}

static int SendTDMDataThread(void *data)
{
    int ret;
    while(!kthread_should_stop())
    {
        set_current_state(TASK_RUNNING);
        if (start_flag)
        {
            G_st_trans_status.u32_SendFrameCnt ++;
            G_st_trans_status.u16_SeqNumberNowSending = ntohs(((unsigned short *)G_senddata)[1]);
            if ((!(G_st_trans_status.u32_SendFrameCnt%100)) && (print_flag == 1))
                printk("send:%d\n", G_st_trans_status.u16_SeqNumberNowSending);

            start_flag = 0;
            ret = ksend(p_sock, G_senddata, DATA_SEND_CNT, MSG_CONFIRM); /*send message */
            if(ret < 0)
                printk("client: kernel_sendmsg error(%d)!\n", ret);
            else if(ret!=DATA_SEND_CNT)
                printk("client: ret==(%d)\n", ret);
        }
        else
        {
            set_current_state(TASK_INTERRUPTIBLE);
            schedule();
        }
    }

    return 0;
}

/*****************************************************************************/
/*  EXPORT */
/*****************************************************************************/
unsigned int RTchain_host2slave_data_cb(void *data, unsigned int len)
{
    int ret = 0;
    if (kfifo_len(&G_st_Spkfifo) > len)
    {
        ret = kfifo_out(&G_st_Spkfifo, data, len);
        if (ret != len)
        {
            printk("kfifo_out error cnt %d\n", ret);
            return 0;
        }
    }
    return ret;
}

#if CONFIG_RECEIVE_ENABLE
ssize_t krecv(struct socket * p_socket, void *buffer, size_t length, int flags)
{
    struct socket *sk;
    struct msghdr msg;
    struct iovec iov;
    int ret;
#ifndef KSOCKET_ADDR_SAFE
    mm_segment_t old_fs;
#endif

    sk = p_socket;

    iov.iov_base = (void *)buffer;
    iov.iov_len = (__kernel_size_t)length;

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

#ifndef KSOCKET_ADDR_SAFE
    old_fs = get_fs();
    set_fs(KERNEL_DS);
#endif
    ret = sock_recvmsg(sk, &msg, length, flags);
#ifndef KSOCKET_ADDR_SAFE
    set_fs(old_fs);
#endif
    if (ret < 0)
        goto out_krecv;

out_krecv:
    return ret;

}

static int RecvSpkDataThread(void *data)
{
    int ret;
    char recvbuf[DATA_RECV_CNT];

    while(!kthread_should_stop())
    {
        if (recv_flag == true)
        {
            ret = krecv(p_sock, recvbuf, DATA_RECV_CNT, MSG_DONTWAIT);
            if (ret == DATA_RECV_CNT)
            {
                G_st_trans_status.u32_RecvFrameCnt ++;
                G_st_trans_status.u16_SeqNumberNowRecving = ntohs(((unsigned short *)recvbuf)[1]);
                if ((!(G_st_trans_status.u32_RecvFrameCnt%100)) && (print_flag == 1))
                    printk("recv:%d\n", G_st_trans_status.u16_SeqNumberNowRecving);

                if (kfifo_len(&G_st_Spkfifo) > (PAGE_SIZE - DATA_RECV_RTAF_DATA_CNT))
                {
                    printk("fifo full, reset fifo\n");
                    kfifo_reset(&G_st_Spkfifo);
                }
                kfifo_in(&G_st_Spkfifo, recvbuf+DATA_RECV_RTAF_HAEDER_CNT, DATA_RECV_RTAF_DATA_CNT);
            }
            else if (ret != -11)
                printk("error recv %d data\n", ret);
        }
    }

    return 0;
}
#endif
/*****************************************************************************/
/* Inti */
/*****************************************************************************/
int trans_start(void)
{
    struct sockaddr_in s_addr;
    int ret = 0;
    if (init_flag == false)
    {
        memset(&s_addr, 0, sizeof(s_addr));
        s_addr.sin_family = AF_INET;
        s_addr.sin_port   = htons(IP_LOCAL_PORT);
        s_addr.sin_addr.s_addr = 0;

        p_sock = &sock;
        ret = sock_create_kern(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &p_sock);
        if(ret < 0)
        {
            printk("client:socket create error!\n");
            return ret;
        }
        printk("client: socket create ok!\n");

        ret = p_sock->ops->bind(p_sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
        if(ret != 0)
        {
            printk("client:bind error!\n");
            return ret;
        }
        printk("client:bind ok!\n");

        s_addr.sin_addr.s_addr = in_aton(ipaddr);
        s_addr.sin_port        = htons(IP_REMOTE_PORT);
        ret = p_sock->ops->connect(p_sock,(struct sockaddr *)&s_addr, sizeof(s_addr), 0);
        if(ret != 0)
        {
            printk("client:connect error!\n");
            return ret;
        }
        else
            printk("client:connect ok!\n");

        start_flag = 0;
        Send_tsk = kthread_create(SendTDMDataThread, NULL, "SendTDMDataThread");
        if (IS_ERR(Send_tsk))
        {
            printk("create send kthread failed!\n");
            return -1;
        }
        else
            printk("create send ktrhead ok!\n");

#if CONFIG_RECEIVE_ENABLE
        Recv_tsk = kthread_run(RecvSpkDataThread, NULL, "RecvSpkDataThread");
        if (IS_ERR(Recv_tsk))
        {
            printk("create recv kthread failed!\n");
            return -1;
        }
        else
            printk("create recv ktrhead ok!\n");
        recv_flag = true;
#endif
#if CONFIG_USE_SIM_DATA
        {
            unsigned short * p_u16_tmp;
            int i;
            Sim_data[0] = 0x80;
            Sim_data[1] = 0x17;

            p_u16_tmp = (unsigned short *)Sim_data;
            for (i=6; i< (DATA_SEND_CNT -14)/2; i+=7)
            {
                p_u16_tmp[i] = 1;
                p_u16_tmp[i + 1] = 2;
                p_u16_tmp[i + 2] = 3;
                p_u16_tmp[i + 3] = 4;
                p_u16_tmp[i + 4] = 5;
                p_u16_tmp[i + 5] = 6;
                p_u16_tmp[i + 6] = 7;
            }
        }
#endif
        init_flag = true;
    }
    else
    {
        printk("trans already start!!\n");
    }
    return 0;
}

void trans_stop(void)
{
    if (init_flag)
    {
        if (!IS_ERR(Send_tsk))
        {
            int ret = kthread_stop(Send_tsk);
            printk("thread send has exit %ds\n", ret);
        }
#if CONFIG_RECEIVE_ENABLE
        recv_flag = false;
        if (!IS_ERR(Recv_tsk))
        {
            int ret = kthread_stop(Recv_tsk);
            printk("thread recv has exit %ds\n", ret);
        }
#endif
        init_flag = false;
        sock_release(p_sock);
        printk("stop!\n");
    }
}

int client_open(struct inode *inode, struct file *filp)
{
    memset(&G_st_trans_status, 0, sizeof(trans_status));
    return 0;
}

int client_release(struct inode *inode, struct file *filp)
{
    trans_stop();
    return 0;
}

static long client_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
        case DEV_IOCTL_PRINT_STAT:
        {
            if (copy_to_user((void __user*)arg, &G_st_trans_status, sizeof(trans_status)) != 0)
                return -EFAULT;
            break;
        }
        case DEV_IOCTL_START:
        {
            trans_start();
            break;
        }
        case DEV_IOCTL_STOP:
        {
            trans_stop();
            break;
        }
        case DEV_IOCTL_PRINT:
        {
            if (copy_from_user(&print_flag, (void __user*)arg, sizeof(char)) != 0)
                    return -EFAULT;
            break;
        }
        case DEV_IOCTL_IPADDR:
        {
            if (!init_flag)
            {
                if (copy_from_user(ipaddr, (void __user*)arg, sizeof(ipaddr)) != 0)
                    return -EFAULT;
                printk("your ip addr is:%s\n", ipaddr);
            }
            else
            {
                printk("please stop trans first!\n");
                return 1;
            }
            break;
        }
        default:
            return  - EINVAL;
  }

  return 0;
}

static const struct file_operations client_fops =
{
    .owner          = THIS_MODULE,
    .open           = client_open,
    .release        = client_release,
    .unlocked_ioctl = client_ioctl,
};

static int client_init(void)
{
    int ret;
    printk("client_init\n");

    devno = MKDEV(MAJORNR, MINORNR);
    ret = register_chrdev_region(devno, 1, "client");

    if(ret < 0){
        printk("register_chrdev_region failed!\n");
        goto failure_register_chrdev;
    }

    cdev_init(&cdev, &client_fops);
    cdev.owner = THIS_MODULE;

    ret = cdev_add(&cdev, devno, 1);
    if(ret < 0){
        printk("cdev_add failed!\n");
        goto failure_cdev_add;
    }

    dev_class = class_create(THIS_MODULE, "client_class");
    if(IS_ERR(dev_class)){
        printk("class_create failed!\n");
        ret = PTR_ERR("dev_class");
        goto failure_class_create;
    }

    dev_device = device_create(dev_class, NULL, devno, NULL, "client%d", 0);
    if(IS_ERR(dev_device)){
        printk("device_create failed!\n");
        ret = PTR_ERR(dev_device);
        goto failure_device_create;
    }
    ret = kfifo_alloc(&G_st_Spkfifo, PAGE_SIZE, GFP_KERNEL);
    if (ret != 0)
    {
        printk("kfifo_alloc fail %d\n", ret);
    }
    return 0;

failure_device_create:
    class_destroy(dev_class);
failure_class_create:
    cdev_del(&cdev);
failure_cdev_add:
    unregister_chrdev_region(devno, 1);
failure_register_chrdev:
    return ret;
}

static void client_exit(void)
{
    trans_stop();
    kfifo_free(&G_st_Spkfifo);
    device_destroy(dev_class, devno);
    class_destroy(dev_class);
    cdev_del(&cdev);
    unregister_chrdev_region(devno, 1);
    printk("rmmod char_dev success\n");
    printk("client_exit\n");
}

module_init(client_init);
module_exit(client_exit);
EXPORT_SYMBOL(RTchain_slave2host_data_cb);
EXPORT_SYMBOL(RTchain_host2slave_data_cb);
MODULE_LICENSE("GPL");

