#ifndef MINHEAP_TIMER
#define MINHEAP_TIMER

#include <netinet/in.h>
#include <exception>
#include <cstdio>
#include <time.h>

#define DEBUG_INFO(format, ...) if(1 == m_debug_info) {printf(format, ##__VA_ARGS__);}

class minheap_timer;//前向声明

struct client_data
{
    sockaddr_in addr;
    int sockfd;
    minheap_timer *timer=NULL;
};
/* 计时器类 */
class minheap_timer
{
public:
    minheap_timer(int delay){expire = time(NULL) + delay;};
    ~minheap_timer()
    {
        // 有无用户数据判断计时器是否有效，有效则注销用户数据的计时器
        if (user_data != NULL) 
        {
            user_data->timer = NULL;
            // printf("timer destory,set it's user_data->timer = NULL\n");
            // fflush(stdout);
        }
    }
public:
    time_t expire;//定时器生效绝对时间
    client_data *user_data = NULL;
    bool adjusted = false;
    void (* cb_func)(client_data *) = NULL;//回调函数
};
/* 最小堆类 */
class min_heap
{
public:
    min_heap(int cap)throw(std::exception);
    //已有数组初始化堆
    min_heap(minheap_timer **timerArray,int size,int cap)throw(std::exception);
    ~min_heap();

    void add_timer(minheap_timer *timer)throw(std::exception);
    void del_timer(minheap_timer *timer);
    // 调整计时器，目前仅支持后延
    void adjust_timer(minheap_timer *timer,time_t delay);
    void real_adjust();
    minheap_timer* top() const;
    void pop();
    void tick();
    bool empty()const{return cur_size==0;}
private:
    //下滤，确保以hole为根节点的子树拥有最小堆性质
    void perocolate_down(int hole);
    //堆数组容量扩大一倍
    void resize()throw(std::exception);
public:
    bool m_debug_info = false;
private:
    minheap_timer **array;//堆数组
    int capacity=64;//当前堆容量
    int cur_size=0;//当前堆包含元素个数
};
#endif