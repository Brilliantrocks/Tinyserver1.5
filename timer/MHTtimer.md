
# 最小堆计时器
* 每次心搏函数获取下次超时的最小时间，用来设置下次触发心搏函数的时间
* 将计时器以最小堆实现，从而快速获得最小超时时间的计时器，时间复杂度为添加计时器O(log n)，删除计时器O(1)，修改计时器O(log n)
* 避免了定时更新计时器总是调用心搏函数造成的开销

## 最小堆计时器实现
```
class minheap_timer;//前向声明

struct client_data
{
    sockaddr_in addr;
    int sockfd;
    minheap_timer *timer;
};
/* 计时器类 */
class minheap_timer
{
public:
    minheap_timer(int delay){expire = time(NULL) + delay;};
    ~minheap_timer();
	public:
    time_t expire;//定时器生效绝对时间
    client_data *user_data;
    bool adjusted = false;//调整标记位
    void (* cb_func)(heap_client_data *);//回调函数
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
	void adjust_timer(minheap_timer *timer,time_t delay);
	void real_adjust();//在tick()中调用的真正修改函数
    minheap_timer* top() const;
    void pop();
    void tick();
    bool empty()const{return cur_size==0;}
private:
    //下滤，确保以hole为根节点的子树拥有最小堆性质
    void perocolate_down(int hole);
    //堆数组容量扩大一倍
    void resize()throw(std::exception);
private:
    minheap_timer **array;//堆数组
    int capacity;//当前堆容量
    int cur_size;//当前堆包含元素个数
};
```
### 计时器
* 构造函数：通过传入计时器生存时间设置超时绝对时间,调整标记位置false
* 析构函数：如果析构时user_data记录仍然有效则将该记录中的计时器指针置空
* 成员变量：
 * 超时绝对时间
 * 用户数据(包含连接套接字，用户网络地址，和该计时器指针）
 * 超时后调用的回调函数
 * 调整标记位
### 最小堆
* 是完全二叉树
* 堆顶元素的值最小
* 每个节点的值都小于或等于其子节点的值：对于序号为i的节点值，总是小于序号为2 * i和2 * i - 1的值
  
* 构造函数：通过传入最大容量初始化，内部为容量大小的计时器指针数组，根据是否传入计时器指针数组初始化填入该指针数组或初始化为空指针数组
 	* 传入指针数组和大小则对第(cur_size-1)/2 ~ 0个元素执行下滤操作`perocolate_down()`
  
* `add_timer`添加计时器：传入计时器
 	* 当前堆大小等于容量时调用`resize()`将计时器指针数组翻倍扩容
 	* 创建新的尾节点序号`hole = cur_size+1`，从尾节点到根执行上滤操作：
 	* 检查父节点`parent = (hole - 1)/2`计时器和插入计时器的绝对时间，若插入计时器的绝对时间较小则交换`hole`和`parent`计时器在堆中的位置，更新`hole = parent`
 	* 重复上述检查步骤直到在堆中找到新计时器超时时间不小于父节点计时器超时时间的的位置或检查到根
 	* 在堆中`hole`位置插入新计时器

* `del_timer`提前删除计时器
 	* 最小堆不支持随机访问，只能按顺序每次访问最小的堆顶计时器
 	* 若指定计时器不存在则直接返回 
 	* 仅将该计时器的回调函数置空，延迟销毁，节省真正删除该定时器造成的开销，但易导致堆数组膨胀
 	* 该计时器的用户数据指针置空，解除绑定，析构时不会再次修改，标志着该计时器无效
 	* 调整标准位置false，不再被调整
  
* `adjust_timer`调整计时器，目前仅支持计时器后延
 	* 更新新的绝对超时时间
 	* 调整标准位置true
 	* 待`tick()`正常轮询到该计时器时重新加入时间堆
    
* `real_adjust`在tick()中调用的真正修改函数
 	* 将计时器从堆中弹出，但不删除计时器实例，而是放在堆尾
 	* 将当前大小减一后执行下滤更新时间堆
 	* 再将当前大小加一后执行上滤更新时间堆
 	* 清空该计时器的调整标记位
  
* `resize()`堆扩容函数：
 	* 每次调用将容量`capacity`翻倍
 	* 用新容量申请新计时器指针数组内存
 	* 将新内存全部清空
 	* 将原内存中的指针数组迁移到新内存
 	* 删除旧指针数组，释放内存
 	* 更新最小堆的内存地址
  
* `tick()`心搏函数：
 	* 循环获取堆顶计时器直到为空
 	* 由于`alarm()`最低精度为1s，预留一秒空余
 	* 若堆顶计时器未超时：判断计时器是否被修改过，是则调用`perocolate()`下滤操作更新堆重入循环；否则表示已检查到正常未超时计时器，退出循环
 	* 对超时的堆顶计时器调用设置的回调函数
 	* 调用`pop()`更新最小堆
 	* 由于最小堆性质总是至少执行一次堆顶回调函数和`pop()`
 	* 较定时更新计时器大幅减少了调用次数
  
* `pop()`堆顶更新函数：
 	* 调用`empty()`检查堆是否为空，若空则直接返回
 	* 否则获取堆顶计时器，删除该计时器，`cur_size`减一
 	* 将尾计时器替换到堆顶
 	* 调用`perocolate()`从根执行下滤操作
  
* `empty()`返回堆是否为空：
 	* 根据堆当前大小`cur_size`是否为0判断
  
* `perocolate()`下滤操作：
 	* 以传入的位置`hole`为起点从上往下检查合适的位置更新该计时器节点在堆中的位置
 	* 检查两个子节点计时器和该节点计时器超时时间的大小
 	* 更新`child = hole * 2 + 1`
 	* 若`child+1`在堆中，更新`child`为`child+1`，`child`中超时时间较小的一个
 	* 比较`child`和`hole`计时器的超时时间
 	* 若`child`的超时时间小于`hole`的超时时间则互换两个计时器在堆中的位置
 	* 更新`hole`为`child`
 	* 对新`hole`循环执行上述检查直到该计时器的超时时间都小于两个子节点的超时时间或者不存在子节点为止
 	* 将该计时器插入在堆中找到的位置
  

## 工具类封装函数
* `timers_handler`计时器堆处理，堆`tick()`和`alarm()`的封装函数：每次心搏函数后，用更新的堆顶计时器的超时时间和当前时间差+1s为参数调用`alarm()`定时发起超时信号
* `add_timer()`计时器添加封装函数：根据传入的用户数据，创建用户连接计时器，如果时间堆为空则调用`alarm()`启动时间堆，将定时器添加到时间堆
* `adjust_timer()`计时器调整封装函数：调用时间堆的`adjust_timer()`，并写计时器调整日志
* `del_timer()`提前删除用户计时器封装函数：提前调用该计时器的回调函数处理连接，调用时间堆的`del_timer()`处理该计时器，写用户连接关闭日志
* `cb_func()`计时器回调函数：将连接套接字在内核事件表内注销，关闭连接套接字，更新当前用户数
