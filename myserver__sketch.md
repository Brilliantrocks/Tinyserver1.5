# myserver工作流程解析

## 启动参数

* -p，自定义端口号
	* 默认9006
* -l，选择日志写入方式，默认同步写入
	* 0，同步写入
	* 1，异步写入
* -m，listenfd和connfd的模式组合，默认使用LT + LT
	* 0，表示使用LT + LT
	* 1，表示使用LT + ET
    * 2，表示使用ET + LT
    * 3，表示使用ET + ET
* -o，优雅关闭连接，默认不使用
	* 0，不使用
	* 1，使用
* -s，数据库连接数量
	* 默认为8
* -t，线程数量
	* 默认为8
* -c，关闭日志，默认打开
	* 0，打开日志
	* 1，关闭日志
* -a，选择反应堆模型，默认Proactor
	* 0，Proactor模型
	* 1，Reactor模型
* -d，开启debug运行信息控制台输出，默认关闭
	* 0，关闭
	* 1，开启

## main
### 1 内建参数设置
* 数据库用户
* 数据库密码
* 数据名称

### 2命令行解析 config.parse_arg
* 分析启动参数并传递
  
### 3构造并初始化server.init
* 构造webserver实例server
	 * 创建用户的http_conn数组users
	 * 设置服务器文件夹路径
	 * 创建用户数据client_data数组user_timers
* 由设置参数初始化服务器设置
  
### 4日志设置
* 单例模式获取实例
* 根据参数设置日志模式
* Log::get_instance()->init（）
#### 日志类
* 默认同步
* 异步则执行下列操作
	 * 异步标志位置一
	 * 以最大队列长度创建日志工作的阻塞字符串队列
	 * 创建子线程，调用`flush_log_thread()`执行线程异步写日志
* 设置日志关闭标志位 初始化一定为0
* 设置日志缓冲区大小 默认2000
* 设置日志最大行数 默认80000
* 根据运行日期创建打开日志文件
  
### 5数据库连接池
* 单例模式获取实例
* 初始化数据库连接池
	 * 设置url，用户，密码，数据库名称，端口（3306），最大连接数（8），日志开关（0关）
	 * 创建8个MYSQL实体con，建立真实数据库连接，添加至连接链表connlist，增加可用连接数m_FreeConn
	 * 以可用连接数8创建信号量reserve 多线程锁信号量sem
	 * 用当前可用连接数设置最大连接数m_Maxconn
* 以用户http_conn连接中的一个初始化数据库读取表
  
### 6线程池
* 初始化线程池
* 设置并发模型（proactor)线程数（8）最大请求数10000
* 创建8个的线程数组，依次构造线程，并分离
	 * 子线程调用http连接类的`worker()`函数，传入线程池指针
	 * 子线程调用线程池的`run()`函数

#### 线程池run()工作流程
##### 1多线程抢占任务
* 各个工作线程阻塞循环等待工作，调用`m_queuestat.wait()`，内部使用信号量多线程抢占任务
* 一个线程获取工作后，调用`m_queuelocker.lock()`上锁，内部使用mutex互斥锁
	 * 如果工作队列为空，则直接解锁，进入下一轮阻塞等待
* 从工作队列获取请求`m_workqueue.front()`，完成后解锁`m_queuelocker.unlock()`
##### 2.1 Reactor模式
###### 请求状态m_state为0：读取
* 调用http的`read_once()`读取一次
* 读取成功
	 * 将请求http的improv标志位置一
	 * 关联数据库连接池中的一个
	 * 调用http类的`process()`
* 读取失败
	 * improv标志位置一
	 * timer_flag标志位置一  
###### 请求状态m_state为1：写入
* 写入成功
	 * improv标志位置一
* 写入失败
	 * improv标志位置一
	 * timer_flag标志位置一
  
* 总结：由子线程完成事务处理I/O操作  
##### 2.2 Proactor模式
* 为http请求分配数据库连接池
* 调用http请求的`process`

* 总结：由主线程完成所有I/O操作
  
### 7触发模式设置
* epoll模式设置
* 由参数设置listenfd和connfd的epoll模式
* 默认都是LT水平触发  

### 8工具函数类初始化
* 设置基准时槽5s
* 初始化计时器最小时间堆，默认初始容量1024
* 传递日志开关，debug消息开关设置，用户数据结构指针
* 清空alarm信号，设置无alarm信号标志位为true

### 9事件监听
* 创建监听套接字m_listenfd
* 根据参数设置优雅连接 默认关闭
* 根据参数设置主机IP地址及端口号
* 将监听fd和地址绑定
* 开始监听，缓存队列5
    
* 创建epoll内核事件表 默认最大事件数10000
* 创建epoll实例 默认提示参数5
* 工具类根据listendfd触发模式（LT）将监听事件(EPOLLIN|EPOLLRDHUP(|EPOLLET边沿触发时追加)注册到内核事件表，oneshot关
	 * 设置listenfd非阻塞
* 将epoll实例用于设置http连接类的epollfd
* socketpair创建双向管道pipefd，pipefd[1]设置非阻塞，pipefd[0]注册到epoll内核事件表
    
* 工具类设置信号函数
	 * SIGPIPE向读端关闭的管道或者socket中写数据触发 设置忽略
	 * SIGALRM 用工具类sig_handler处理 计时器alarm触发，设置信号中断系统调用可重启
	 * SIGTERM 用工具类sig_handler处理 终止信号，设置信号中断系统调用可重启

* 设置工具类的pipe和epollfd

### 10事件循环
* 超时标志位，停止标志位置零
    
#### 大循环 

1. 阻塞模式调用epoll_wait直到有事件触发，获取epoll事件表中等待的事件数number
	 * 如果小于0报错退出大循环
2. 遍历事件表
	 1. 如果是listenfd：调用dealclientdata()处理客户端数据；失败仍然继续循环
	 2. 事件有EPOLLRDHUP | EPOLLHUP | EPOLLERR标志位：关闭连接，调用del_timer提前删除定时器

	 3. 如果事件管道发送了信号：调用dealwithsignal()处理事件信号
	 4. 如果事件发送数据EPOLLIN：调用dealwithread(）读取
	 5. 如果事件请求数据EPOLLOUT：调用dealwithwrite(）写入
3. 事件表遍历结束后检查超时标记位
	 * 如果存在超时事件：调用工具类的计时器处理utils.timer_handler();写日志；超时标记位置零
4. 重复循环
