# eventloop分析
 
## 1 epoll_wait()获取活动事件
* 阻塞等待事件发生（`epoll_wait()`timeout参数-1）
* 获取当前活动事件数目number
* 如果报错退出eventloop

## 2 遍历有活动的events事件表
### 2.1 listenfd事件：dealclinetdata()
1. 创建 sockaddr_in 类型的客户端地址 client_address 和 地址长度client_addrlength
2. 选择触发模式
#### 2.1.1LT水平触发：

* 调用`accept()`从监听套接字获得创建连接fd connfd ，地址，地址长度存放于client_address，地址长度client_addrlength
* 创建connfd失败则写连接错误日志并返回false
* 如果当前用户数大于了设置的最大文件描述符数（65536）则通过工具类utils调用报错函数写服务器正忙，写日志，返回false
* 根据该fd的用户数据user_data记录中有无计时器判断是否可以注册用户数据，无则进入用户数据注册流程
  * 调用`create_user()`创建用户记录
  * http_conns连接记录初始化该fd的用户连接，传递相关设置
  * 设置该fd的user_data用户数据记录，其客户端地址和fd
  * 调用工具类的`add_timer()`注册计时器
  
* 总结：一次`epoll_wait()`里只处理listenfd上的一个连接事件，若仍有连接事件则在下一次`epoll_wait()`(即下一次`eventloop()`)里处理
####2.1.2ET边沿触发：
* 循环执行LT一样的`accept()`处理流程，直到`accept()`返回-1
* 原始版本没有区分正常处理`accept()`和`accept()`报错
  * 当循环处理完listenfd上的连接时间后，再次调用`accept()`函数返回-1，且满足`if((errno==EAGAIN)||(errno==EWOULDBLOCK)) `，此时正常返回true即可
  * 原始版本将所有-1返回值当作报错处理，写accept错误日志，并使`dealclinetdata()`返回false;不过因为就算返回了false也会继续执行`eventloop`所以仍可以正常工作
 
* 总结：一次`epoll_wait()`里处理listenfd上所有的连接事件，直到没有连接可接收
  
* 参考：http://blog.chinaunix.net/uid-28541347-id-4308612.html
  
### 2.2对于connfd的挂起/错误处理
* 该connfd的events成员EPOLLRDHUP | EPOLLHUP | EPOLLERR标志位置一
* 获取当前客户端数据client_data users_timer的成员计时器
* 用connfd 和计时器timer调用`del_timer()`处理
  * 调用timer的成员回调函数：内核事件表删除注册事件，关闭connfd，http连接计数减一
  * 工具类调用最小时间堆的提前删除函数，将计时器回调函数，用户数据指针置空，调整标记位设置为true，待自然触发该计时器时真正从堆中删除
  * 日志写关闭connfd

* 总结：关闭失效的http连接，删除事件表注册事件和计时器
  
### 2.3处理管道上的信号
* 当sockfd为管道0端，且有数据可读EPOLLIN
* 调用`dealwithsignal()`处理，传入timeout ,stop_server参数记录信号指令
  * 内部调用`recv(）`从管道0端读取信号信息到signals,并遍历检查
  * SIGALRM：timeout标志位置一，退出遍历
  * SIGTERM: stop_server标志位置一，退出遍历
  
* 总结：做了对中止和超时信号的处理
  
### 2.4connfd读事件处理
* 用connfd调用`dealwithread()`
* 获取当前connfd的计时器timer
#### 2.4.1 reactor反应模式
* 计时器未失效则调用调整函数`adjust_timer`：计时器后延15s，工具类调用顺序计时器链表调整函数，写计时器调整日志
* http连接数组users指针地址加上connfd获得请求id，用0传参调用http连接池的`append（）`将请求加入工作队列
* 根据http连接类的成员标志位improv和timer_flag循环调用`webserver::deal_timer()`处理客户端计时器直到超时（删除计时器）或者完成
#### 2.4.2 proactor反应模式
* 调用http连接的`read_once()`读取一次
* 如果读取成功则
  * 写处理日志
  * 调用`append_p()`http连接池追加该请求
  * 如果计时器有效则调用`web::adjust_timer()`调整计时器
* 读取失败
  * 调用`deal_timer()`处理失效套接字
  
### 2.5connfd写事件处理
* 用connfd调用`dealwithwrite()`
* 获取该套接字计时器timer
#### reactor反应模式
* 计时器未失效则调用调整函数`adjust_timer`：计时器后延15s，工具类调用顺序计时器链表调整函数，写计时器调整日志
* 用1传参调用http连接池的`append（）`将请求加入工作队列
* 根据http连接类的成员标志位improv和timer_flag循环调用`webserver::deal_timer()`处理客户端计时器直到超时（删除计时器）或者完成
#### proactor反应模式
* 调用http连接的`write()`写入
* 如果写入成功
  * 写日志
  * 如果计时器有效则调用`web::adjust_timer()`调整计时器
* 写入失败
  * 调用`deal_timer()`处理失效套接字

## 3 超时检查
* 检查timeout标志位
* 若存在超时事件则工具类调用计时器处理函数`utils.timer_handler()`
  * 内部调用顺序计时器链表的tick函数`m_timer_lst.tick()`，处理计时器链表
  * 调用`alarm(5)`每5s发出ALARM信号
* 日志写计时器tick
* 超时标志位timeout置零
