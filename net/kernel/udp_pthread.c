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

/*****************************************************************************/
/* define */
/*****************************************************************************/

#define CONFIG_USE_SIM_DATA 0
#define CONFIG_RECEIVE_ENABLE  1

#define IP_ADDR "10.3.16.96"
#define IP_REMOTE_PORT 22225
#define IP_LOCAL_PORT  22223

#define DATA_SEND_CNT 4492

#define DATA_RECV_CNT 652
#define DATA_RECV_DATA_HAEDER_CNT 12
#define DATA_RECV_DATA_CNT   640


/*****************************************************************************/
/* globals */
/*****************************************************************************/
struct socket *sock;

static struct task_struct *Send_tsk;
static struct task_struct *Recv_tsk;

char start_flag = 0;
struct kvec  G_st_Sendvec;
struct kfifo G_st_Spkfifo;

#if CONFIG_USE_SIM_DATA
unsigned char Sim_data[DATA_SEND_CNT];
#endif


/*****************************************************************************/
/*  EXPORT */
/*****************************************************************************/
unsigned int RTchain_slave2host_data_cb(void *data, unsigned int len)
{
    if (data != NULL)
    {
#if CONFIG_USE_SIM_DATA
        Sim_data[2] = FrameNr&0xff;
        Sim_data[3] = (FrameNr&0x00ff)>>8;
        G_st_Sendvec.iov_base = Sim_data;
#else
        G_st_Sendvec.iov_base = data;
#endif
        G_st_Sendvec.iov_len  = len;
    }
    else
    {
        printk("data addr can not be NULL\n");
        return -1;
    }

    start_flag = 1;
    wake_up_process(Send_tsk);

    return 1;
}

static int SendTDMDataThread(void *data)
{
    int ret;
    unsigned int cnt;
    struct msghdr msg;
    while(!kthread_should_stop())
    {
        set_current_state(TASK_RUNNING);
        if (start_flag)
        {
            cnt ++;
            if (!(cnt%100))
                printk(KERN_INFO "thread send framNum:%d\n", ntohs(((unsigned short *)G_st_Sendvec.iov_base)[1]));

            start_flag = 0;
            ret = kernel_sendmsg(sock, &msg, &G_st_Sendvec, 1, DATA_SEND_CNT); /*send message */
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
static int RecvSpkDataThread(void *data)
{
    int ret;
    unsigned int cnt;
    struct msghdr msg;
    struct kvec recvkvec;
    int Recv_start_flag = 0;
    char *recvbuf = NULL;
    recvbuf = kmalloc(DATA_RECV_CNT, GFP_KERNEL);

    while(!kthread_should_stop())
    {
        memset(&recvkvec, 0, sizeof(recvkvec));
        memset(&msg, 0, sizeof(msg));
        recvkvec.iov_base = recvbuf;
        recvkvec.iov_len = DATA_RECV_CNT;
        ret = kernel_recvmsg(sock, &msg, &recvkvec, 1, DATA_RECV_CNT, MSG_DONTWAIT); /*receive message*/
        if (ret == DATA_RECV_CNT)
        {
            if (Recv_start_flag == 0)
            {
                cnt = ntohs(((unsigned short *)recvbuf)[1]);
                Recv_start_flag = 1;
            }
            cnt ++;
            if (!(cnt%100))
                printk(KERN_INFO "thread receive framNum:%d\n", ntohs(((unsigned short *)recvbuf)[1]));

            if (kfifo_len(&G_st_Spkfifo) > (PAGE_SIZE - DATA_RECV_DATA_CNT))
            {
                printk("fifo full, reset fifo\n");
                kfifo_reset(&G_st_Spkfifo);
            }
            kfifo_in(&G_st_Spkfifo, recvbuf+DATA_RECV_DATA_HAEDER_CNT, DATA_RECV_DATA_CNT);
        }
        else if (ret != -11)
            printk("error recv %d data\n", ret);
    }

    kfree(recvbuf);
    return 0;
}
#endif
/*****************************************************************************/
/* Inti */
/*****************************************************************************/
int init(void)
{
    struct sockaddr_in s_addr;
    int ret = 0;

    memset(&s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(IP_LOCAL_PORT);
    s_addr.sin_addr.s_addr = 0;

    sock = (struct socket *)kmalloc(sizeof(struct socket),GFP_KERNEL);

    /*create a socket*/
    ret = sock_create_kern(AF_INET, SOCK_DGRAM, IPPROTO_UDP, &sock);
    if(ret < 0)
    {
        printk("client:socket create error!\n");
        return ret;
    }
    printk("client: socket create ok!\n");

    ret = sock->ops->bind(sock, (struct sockaddr *)&s_addr, sizeof(s_addr));
    if(ret != 0)
    {
        printk("client:bind error!\n");
        return ret;
    }
    printk("client:bind ok!\n");

    s_addr.sin_addr.s_addr = in_aton(IP_ADDR);
    s_addr.sin_port = htons(IP_REMOTE_PORT);
    /*connect server*/
    ret = sock->ops->connect(sock,(struct sockaddr *)&s_addr, sizeof(s_addr), 0);
    if(ret != 0)
    {
        printk("client:connect error!\n");
        return ret;
    }
    else
        printk("client:connect ok!\n");

    Send_tsk = kthread_run(SendTDMDataThread, NULL, "SendTDMDataThread%d", 1);
    if (IS_ERR(Send_tsk))
    {
        printk("create send kthread failed!\n");
        return -1;
    }
    else
        printk("create send ktrhead ok!\n");

    ret = kfifo_alloc(&G_st_Spkfifo, PAGE_SIZE, GFP_KERNEL);
#if CONFIG_RECEIVE_ENABLE
    Recv_tsk = kthread_run(RecvSpkDataThread, NULL, "RecvSpkDataThread%d", 1);
    if (IS_ERR(Recv_tsk))
    {
        printk("create recv kthread failed!\n");
        return -1;
    }
    else
        printk("create recv ktrhead ok!\n");
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
    return 0;
}

static int client_init(void)
{
    printk("init\n");
    return (init());
}

static void client_exit(void)
{
    if (!IS_ERR(Send_tsk))
    {
        int ret = kthread_stop(Send_tsk);
        printk("thread send has exit %ds\n", ret);
    }
#if CONFIG_RECEIVE_ENABLE
    if (!IS_ERR(Recv_tsk))
    {
        int ret = kthread_stop(Recv_tsk);
        printk("thread recv has exit %ds\n", ret);
    }
#endif
    sock_release(sock);
    kfifo_free(&G_st_Spkfifo);
    kfree(sock);
    printk("exit!\n");
}

module_init(client_init);
module_exit(client_exit);
EXPORT_SYMBOL(RTchain_slave2host_data_cb);
EXPORT_SYMBOL(RTchain_host2slave_data_cb);
MODULE_LICENSE("GPL");

