#include "utils.h"

void Utils::init(int timesolt,int cap,int close_log,client_data** users_data, int debug_info)
{
    m_timeslot = timesolt;
    m_minheap = new min_heap(cap);
    m_close_log = close_log;
    m_users_data = users_data;
    alarm(0);//清空alarm信号
    m_noalarm = true;
    m_debug_info = debug_info;
    m_minheap->m_debug_info = debug_info;
    DEBUG_INFO("utils inits ,clear alarm signal\n");
}

//对文件描述符设置非阻塞
int Utils::setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}
/*  用于：
    1.向内核事件表注册监听事件，并发模式(LT)，关闭ONESHOT
    2.向内核事件表注册读事件，并发模式(LT)，选择开启EPOLLONESHOT
    设置非阻塞IO
 */
void Utils::addfd(int epollfd, int fd, bool one_shot, int TRIGMode)
{
    epoll_event event;
    event.data.fd = fd;

    if (1 == TRIGMode)
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

//信号处理函数
void Utils::sig_handler(int sig)
{
    //为保证函数的可重入性，保留原来的errno
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

//设置信号函数
void Utils::addsig(int sig, void(handler)(int), bool restart)
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if (restart)
        sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void Utils::add_timer(client_data* user_data)
{
    DEBUG_INFO("add sockfd %d's tiemr\n",user_data->sockfd);
    minheap_timer* timer = new minheap_timer(3 * m_timeslot);
    timer->user_data = user_data;
    timer->cb_func = cb_func;
    //如果当前没有信号则发起alarm信号启动时间堆
    if (m_noalarm) 
    {
        alarm(3 * m_timeslot);
        DEBUG_INFO("no alarm , trigger alarm %ds,now have an alarm\n",3 * m_timeslot);
        m_noalarm = false;
    }
    //else printf("no alarm when add timer\n");
    user_data->timer = timer;
    //注册到时间堆
    m_minheap->add_timer(timer);
}

void Utils::adjust_timer(minheap_timer* timer)
{
    m_minheap->adjust_timer(timer,time(NULL) + 3 * m_timeslot);
    // 调整后仍然需要按原本的超时时间触发alarm信号，后设置的alarm信号会覆盖之前的设置，总是通过timer_handler()来设置定时信号
    // printf("adjust timer once,\n");
    // fflush(stdout);
    LOG_INFO("%s", "adjust timer once");
}

void Utils::del_timer(minheap_timer* timer,int sockfd)
{
    DEBUG_INFO("delete sockfd %d's timer,\n",sockfd);
    // if(sockfd != timer->user_data->sockfd) printf("wrrong user_data->sockfd %d\n",timer->user_data->sockfd);
    // if(timer != timer->user_data->timer) printf("wrrong user_data different from userdata[sockfd] \n");
    // if(m_users_data[sockfd]!= timer->user_data) printf("wrrong user_data->timer \n");
    if (timer->cb_func != NULL) timer->cb_func(m_users_data[sockfd]);
    m_minheap->del_timer(timer);
    LOG_INFO("close fd %d", sockfd);
}

//定时处理任务，触发SIGALRM信号
void Utils::timer_handler()
{
    m_minheap->tick();
    auto timer = m_minheap->top();
    if (timer != NULL)
    {
        //设置最小超时时间后一秒触发下一次定时信号
        int t = timer->expire - time(NULL) + 1;
        if (t < 1) t = 1;
        alarm(t);
        DEBUG_INFO("tick done, alarm %ds,now have an alarm\n",t);
        m_noalarm = false;
    } 
}

void Utils::show_error(int connfd, const char *info)
{
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int *Utils::u_pipefd = 0;
int Utils::u_epollfd = 0;

Utils::~Utils()
{
    delete m_minheap;
};

void cb_func(client_data *user_data)
{
    // printf("close sockfd %d\n",user_data->sockfd);
    epoll_ctl(Utils::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    user_data->timer=NULL;
    http_conn::m_user_count--;
}