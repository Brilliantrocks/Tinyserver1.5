#ifndef UTILS
#define UTILS

#include<fcntl.h>
#include<sys/epoll.h>
#include<errno.h>
#include<signal.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "../timer/minheap_timer.h"
#include "../http/http_conn.h"
#include "../log/log.h"

class Utils
{
public:
    Utils() {}
    ~Utils();

    void init(int timeslot,int cap,int close_log,client_data** users_data, int debug_info);

    //对文件描述符设置非阻塞
    int setnonblocking(int fd);

    //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
    void addfd(int epollfd, int fd, bool one_shot, int TRIGMode);

    //信号处理函数
    static void sig_handler(int sig);

    //设置信号函数
    void addsig(int sig, void(handler)(int), bool restart = true);

    //定时器添加
    void add_timer(client_data* user_data);

    //定时器修改
    void adjust_timer(minheap_timer* timer);

    //定时器删除
    void del_timer(minheap_timer* timer,int sockfd);

    //定时处理任务，重新定时以不断触发SIGALRM信号
    void timer_handler();

    void show_error(int connfd, const char *info);

public:
    static int *u_pipefd;
    min_heap* m_minheap;
    static int u_epollfd;
    int m_timeslot;
    client_data** m_users_data;
    bool m_close_log;
    bool m_noalarm=true;
    bool m_debug_info;
};

void cb_func(client_data *user_data);

#endif