# Tinyserver1.5
基于最小堆计时器实现的多线程IO复用高性能web服务器
* 使用线程池避免了动态生成线程造成的性能下降。
* 以epoll实现的IO多路复用，可选Proactor/Reactor模型的并发事务处理。
* 有限状态机模型解析http报文，编写cgi接口支持解析GET和POST方式的报文。
* 使用mysql数据库储存用户密码数据，实现了注册登陆功能，以便获得服务器端图片视频等数据。
* 可选同步/异步日志记录服务器的运行状态。
* 计时器以最小堆的堆顶最小超时时间设置心搏函数计时，较之定长时间心搏计时器性能显著提升，支持计时器删除和修改

服务器框架：
---------------------
<div align=center><img src="https://www.hualigs.cn/image/6061eb624735b.jpg"/> </div>
--------  

[个人服务器实践站点](http://82.157.21.191:9006/)  

参考与致谢  
--------
* 理论出自《Linux高性能服务器编程》-游双
* 初版代码来源[TinyWebServer](https://github.com/qinguoyi/TinyWebServer)-qinguoyi

细节详解
-----
* [服务器运行流程](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/myserver__sketch.md)  
* [Proactor/Reactor并发模式](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/proactor&reactor.md)
* [epoll实现的IO多路复用](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/epoll.md)
* [深入eventloop事件循环](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/eventloop.md)
* [http连接类](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/http/httpconn.md)
* [httpconn池化](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/threadpool/httpconn_pool.md)
* [mysql连接池](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/CGImysql/mysqlconn_pool.md)
* [同步日志与阻塞队列异步日志](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/log/log.md)
* [最小堆计时器](https://github.com/Brilliantrocks/Tinyserver1.5/blob/main/timer/MHTtiemr.md)

