
# 池化技术
* 事先建立线程池供http请求处理使用，避免了动态创建线程造成的性能开销
## http连接池初始化
* 根据传参设置并发模式，线程数，最大请求数，线程数组，数据库连接池
* 用`worker()`创建8个子线程，传入`this`指针
* 线程分离
## woker()工作线程函数
* C++程序中使用pthread_create函数时，该函数的第3个参数必须指向一个静态函数
* 静态成员函数不可以调用类的非静态成员,故将类的对象(this)作为参数传递给`worker()`，然后在其中引用对象，以调用其动态方法
```
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //根据传入的this参数获取本工作线程的
    threadpool *pool = (threadpool *)arg;
    pool->run();
    return pool;
}
```  
* 之后各个线程执行`run()`
* 阻塞等待在请求队列信号上`m_queuestat.wait()`抢占任务
* 一个线程获得任务后，首先对请求队列加锁操作，尝试从队列获取工作，之后解锁
* 获取工作后根据并发模式处理工作
 * Proactor：获取数据库实例，执行`http_conn`类的`process()`
 * Reactor:根据请求类型进行I/O操作，执行后更新`improv`标记连接已经I/O读写过，读取成功后获取数据库连接实例，执行`http_conn`类的`process()`，失败时设置超时标记位`tiemr_flag`
* `process()`详见http连接类；
* 而`process()`过程中可能会调用`close_conn()`，其内部操作了静态成员变量`m_user_count`，++操作非原子性，
* 多线程下可能造成竞态，是线程不安全的
