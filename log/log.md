# 日志类
## 单例模式
* 通过`get_instance()`获得唯一实例
## 初始化
* 根据参数中最大队列长度是否大于一设置设置同步或者异步操作，默认同步
* 异步则执行下列操作
    * 异步标志位置一
    * 以最大队列长度创建日志工作的阻塞字符串队列
    * 创建子线程，调用`flush_log_thread()`执行线程异步写日志
* 设置日志关闭标志位 初始化一定为0
* 设置日志缓冲区大小 默认2000
* 设置日志最大行数 默认80000
* 根据运行日期创建并打开日志文件，打开方式为追加写模式(a)；如果文件打开失败则返回`false`
## 写日志
* 通过宏调用`write_log()`和`flush()`来执行写日志并刷回硬盘
### 宏
  
```
#define LOG_DEBUG(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(0, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_INFO(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(1, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_WARN(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(2, format, ##__VA_ARGS__); Log::get_instance()->flush();}
#define LOG_ERROR(format, ...) if(0 == m_close_log) {Log::get_instance()->write_log(3, format, ##__VA_ARGS__); Log::get_instance()->flush();}

```
### 写日志的时机
* `LOG_INFO`标准信息日志：
 1. `http_conn`连接类处理未知文件头信息时，日志记录未知文件头
 2. `http_conn`连接类读处理时，每读取一行，日志记录读取的信息
 3. `http_conn`连接类写回复报文状态行和头部信息时，每次成功向写缓冲区写入，日志记录写入的状态行和头部信息
 4. `Webserver`类每次调用`adjust_timer()`调整计时器时，日志记录调整计时器一次
 5. `Webserver`类每次调用`deal_timer()`删除超时计时器并关闭相应连接套接字时，日志记录关闭的连接套接字
 6. Proactor模式下`Webserver`类调用I/O操作成功时，日志记录处理/发送客户端的IP
 7. `Webserver`类每次接收到`ALARM`信号时，即每5s更新计时器时，日志记录计时器一跳
* `LOG_ERROR`错误信息日志：
 1. `http_conn`连接类初始化SQL映射缓存，SQL查询出错时，日志记录mysql错误信息
 2. `Webserver`类处理客户端连接请求时，若`accept()`出错，日志记录accept错误和错误码
 3. `Webserver`类处理客户端连接请求时，若用户连接数超出最大文件描述符数目，日志记录服务器忙错误
 4. `Webserver`类调用`epoll_wait()`等待事件出现`EINTR`中断以外的错误时，日志记录epoll失败
 5. `Webserver`类调用`dealwithsignal()`处理信号出错时，日志记录处理客户端数据失败
  
### 日志写，数据刷回实现
#### write_log
* 获取当前时间
* 根据输入参数的编码选择日志信息头："[debug/info/warn/error/info]"
* 互斥锁上锁，进入临界区操作
    * 日志行计数加一
    * 如果日志日期不是当天或者行数超过了最大行数，否则直接退出该临界区
    * `fflush()`刷新日志文件流，将缓冲区内容写入日志文件
    * `fclose()`关闭日志文件流
    * 根据日期不同或者行数限制创建新日志文件并打开，追加写模式(a)，更新日志文件描述符
    * 解锁退出临界区
* 获取传入的参数列表里的日志信息，创建日志字符串
* 互斥锁上锁，进入临界区操作
    * 向缓冲区写当前时间，精确到微秒
    * 向缓冲区写日志信息
    * 缓冲区尾写"\n\0"终止符
    * 将缓冲区数据传入日志字符串
    * 解锁退出临界区
* 如果是异步写模式且日志队列未满
    * 将日志字符串压入日志工作阻塞队列，调用`block_queue`类的`push()`
* 同步则互斥锁加锁进入临界区
    * 调用`fputs()`将日志字符串写入日志文件
    * 解锁退出临界区
* 关闭局部参数列表

#### flushs刷新日志
* 互斥锁加锁进入临界区
    * 调用`fflush()`将缓冲区数据刷回日志文件
    * 解锁退出临界区

#### flush_log_thread异步模式子线程刷新日志
* 异步模式下由子线程调用
* 内部调用`async_write_log()`
    * 循环从日志工作阻塞队列获取日志字符串，调用`block_queue`类的`pop()`
    * 互斥锁加锁进入临界区，调用`fputs()`将日子字符串写入日志文件，解锁退出临界区

### 阻塞队列
* 内部为数组实现
* 每次操作队列都在临界区加锁进行，实现了线程安全
* 操作包括
 1. `~block_queue()`析构
 2. `full()`当前队列满则返回`true`，否则返回`false`
 3. `empty()`当前队列空则返回`true`，否则返回`false`
 4. `front()`返回队首元素
 5. `back()`返回队尾元素
 6. `size()`返回当前队列大小
 7. `max_size()`返回队列最大大小
 8. `push()`向队列压入元素
 9. `pop()`从队列取出元素
#### push
* 全程在互斥锁加锁临界区内进行
* 如果当前队列已达最大大小，条件变量广播通知其他线程，解锁，返回`false`
* 更新队尾标记，将元素添加到内部数组当前队尾
* 更新队列大小
* 条件变量广播通知其他线程
* 解锁，返回`true`

#### pop
* 全程在互斥锁加锁临界区内进行
* 如果当前队列为空，条件变量调用`wait()`等待条件变量广播可用，在其中解锁互斥锁，唤醒时加锁；若失败则返回`false`
* 更新队首标记，将队首元素传递到目标地址
* 更新队列大小
* 解锁，返回`true`
