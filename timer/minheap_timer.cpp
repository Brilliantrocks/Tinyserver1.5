#include"minheap_timer.h"

min_heap::min_heap(int cap)throw(std::exception)
{
    array = new minheap_timer*[cap];
    if(!array)
    {
        throw std::exception();
    }
    for(int i;i < cap;++i)
    {
        array[i] = NULL;
    }
    capacity = cap;
}

min_heap::min_heap(minheap_timer **timerArray,int size,int cap)throw(std::exception):cur_size(size),capacity(cap)
{
    if(capacity < size)
    {
        throw std::exception();
    }
    array  = new minheap_timer*[cap];
    if(!array)
    {
        throw std::exception();
    }
    for(int i;i < cap;++i)
    {
        array[i] = NULL;
    }
    if(0!=size)
    {
        for(int i = 0;i <size;++i)
        {
            array[i] = timerArray[i];
        }
        // 对第(cur_size-1)/2 ~ 0个元素执行下滤操作
        for(int i = (cur_size-1)/2; i >= 0; --i)
        {
            perocolate_down(i);
        }
    }
}

min_heap::~min_heap()
{
    for(int i = 0; i < cur_size;++i)
    {
        delete array[i];
    }
    delete[] array;
}

void min_heap::add_timer(minheap_timer *timer)throw(std::exception)
{
    if(!timer) return;
    //当前容量不够
    if(cur_size >= capacity){
        resize();
    }
    // hole为堆的新空穴
    int hole = cur_size++,parent = 0,exp= timer->expire;
    // 对空穴到根路径上所有节点上滤
    for(;hole > 0;hole = parent){
        parent=(hole-1)/2;
        if(array[parent]->expire <= exp){
            break;
        }
        array[hole]=array[parent];
    }
    array[hole] = timer;
    DEBUG_INFO("add a tiemr,now have total %d tiemrs\n",cur_size);
}

void min_heap::del_timer(minheap_timer* timer)
{
    if(!timer){
        return;
    }
    //仅将回调函数置空，延迟销毁，节省真正删除该定时器造成的开销，但易导致堆数组膨胀
    timer->cb_func=NULL;
    //解除绑定，析构时不会再次修改定时器
    timer->user_data=NULL;
    //调整标志位置零避免多余调整
    timer->adjusted = false;
}
//将后延的超时时间更新，设置调整标记位，待正常轮询到该计时器时重新加入时间堆
void min_heap::adjust_timer(minheap_timer* timer,time_t new_expire)
{
    timer->expire = new_expire;
    timer->adjusted = true;
}
//先将堆顶调整计时器弹出后下滤，再将其上滤加入时间堆
void min_heap::real_adjust()
{
    minheap_timer* tmp = array[0];
    if(cur_size>1)
    {
        //将原堆顶元素替换为尾元素
        array[0]=array[--cur_size];
        //对新堆顶执行下滤操作
        perocolate_down(0);
        int hole = cur_size++,parent = 0;
        // hole为堆的新空穴
        // 对空穴到根路径上所有节点上滤
        for(;hole > 0;hole = parent){
            parent=(hole-1)/2;
            if(array[parent]->expire<=tmp->expire){
                break;
            }
            array[hole]=array[parent];
        }
        array[hole] = tmp;
        //调整标记位置零
    }
    tmp->adjusted = 0;
}

minheap_timer* min_heap::top() const
{
    if(empty()){
        return NULL;
    }
    return array[0];
}

void min_heap::pop()
{
    if(empty()){
        return;
    }
    if (array[0])
    {
        delete array[0];
        //将原堆顶元素替换为尾元素
        array[0]=array[--cur_size];
        array[cur_size]=NULL;
        //对新堆顶执行下滤操作
        perocolate_down(0);
    }
    if(m_debug_info)
    {
        if(!empty()) printf("pop a tiemr,now have total %d tiemrs,next alram is %d\n",cur_size,min_heap::top()->expire-time(NULL));
        else printf("pop a tiemr,now have no tiemr\n");
    }
}

void min_heap::tick()
{
    minheap_timer* tmp = array[0];
    time_t cur = time(NULL);
    //循环处理堆中到期的定时器
    while (!empty())
    {
        if(!tmp) break;
        //由于alarm()最低精度为1s，预留一秒
        //对于未到期计时器
        if(tmp->expire > cur+1)
        {
            // 如果是调整过的计时器，则执行新超时时间的堆重排
            if(tmp->adjusted)
            {
                real_adjust();
                continue;
            }
            //未修改则表示已找到正常未超时计时器，退出循环
            else break;
        }
        //执行堆顶计时器的任务
        if(array[0]->cb_func)
        {
            array[0]->cb_func(array[0]->user_data);
        }
        pop();
        tmp = array[0];
    }
}

void min_heap::perocolate_down(int hole)
{
    minheap_timer* tmp = array[hole];
    int child = 0;
    for(;((hole * 2 +1) <= (cur_size-1));hole = child)
    {
        child=hole*2 + 1;
        if((child<(cur_size-1)) && (array[child+1]->expire < array[child]->expire))
        {
            ++child;
        }
        if(array[child]->expire < tmp->expire)
        {
            array[hole] = array[child];
        }
        else break;
    }
    array[hole] = tmp;
}

void min_heap::resize()throw(std::exception)
{
    DEBUG_INFO("now have total %d tiemrs,resize the heap\n",cur_size);
    minheap_timer** tmp = new minheap_timer*[2*capacity];
    for(int i=0; i < 2*capacity;++i)
    {
        tmp[i] = NULL;
    }
    if(!tmp){
        throw std::exception();
    }
    capacity = 2*capacity;
    for(int i = 0;i < cur_size; ++i)
    {
        tmp[i]=array[i];
    }
    delete[] array;
    array = tmp;
}
