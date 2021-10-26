  
# http连接类
* 负责处理用于http应用的connfd，读取web请求，分析请求报文，连接数据库，逻辑处理请求，写回复报文
## 状态参数设置
* 设置枚举变量
  
* 请求方法METHOD
   * GET
   * POST
   * HEAD
   * PUT
   * DELETE
   * TRACE
   * OPTIONS
   * CONNECT
   * PATH
* 报文分析状态CHECK_STATE
   * CHECK_STATE_REQUESTLINE
   * CHECK_STATE_HEADER
   * CHECK_STATE_CONTENT
* http连接状态HTTP_CODE
   * NO_REQUEST
   * GET_REQUEST
   * BAD_REQUEST
   * NO_RESOURCE
   * FORBIDDEN_REQUEST
   * FILE_REQUEST
   * INTERNAL_ERROR
   * CLOSED_CONNECTION
* 缓冲区行状态LINE_STATUS
   * LINE_OK
   * LINE_BAD
   * LINE_OPEN
## http类工作流程
### 初始化init()
`void init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);`  

* 设置类内成员
   * 套接字
   * 客户端地址
   * 根目录路径
   * 触发模式
   * 日志开关标志位
   * 数据库用户，密码和目标数据库名称
* 根据触发模式(ET/LT)向内核事件表注册该connfd，开启ONESHOT事件属性
* 静态类成员m_user_count用户计数增加；线程共享，但`init()`仅由主线程操作，故线程安全
* 调用无参多态版本进行连接初始化
   * 数据库实例置空
   * 缓冲区字节计数（待发/已发）归零
   * http状态码复位为`CHECK_STATE_REQUESTLINE`
   * 保持连接标志位`linger`置零
   * url地址相关变量归零
   * 读写序列归零
   * cgi标志位归零（用于开启POST分析）
   * 连接状态复位为读
   * 超时标志位置零
   * 完成标志位置零
   * 清空读写缓冲区和服务器本地文件
  
### I/O操作
#### read_once()读取一次
* 循环读取客户端数据，直到无数据可读或对方关闭连接
* 非阻塞ET工作模式下，需要一次性将数据读完

1. 判断可读标记`m_read_idx`是否超出读取缓冲区大小，溢出则报错
2. LT：
   * 调用`recv()`从connfd读取数据
   * 根据读取的字节数更新可读标记
3. ET:
   * 循环调用`recv()`从connfd读取数据
   * 直到无数据可读，返回-1且错误码为`errno == EAGAIN || errno == EWOULDBLOCK`时正常退出
   * 每次内循环更新可读标记
#### write()写入
* 检查待发送字节数，为0则修改套接字内核事件注册表为EPOLLIN等待下次可读,初始化该套接字上的连接，返回`true`
* 否则表示有数据需要写回，循环向套接字写数据：
   * 调用`writev()`向套接字集中写iovec数组的内容
   * 若返回的写回字节数小于0：错误码为`EAGAIN`则修改套接字内核事件注册表为EPOLLOUT表示可写，返回`true`继续写；否则表示出错，调用`unmap()`解除指定文件映射，返回`false`
   * 根据写回字节数更新已发字节数和待发字节数
   * 根据已发字节数和待发字节数更新iovec向量信息
   * 如果待发字节数小于等于0，表示发送完毕，调用`unmap()`解除指定文件映射，修改套接字内核事件表为EPOLLIN等待下次可读，如果设置了保持连接标志位则调用`init()`无参版本初始化http连接，返回`true`；否则返回`false`
#### readv函数和writev函数
* readv将数据从文件描述符读到分散的内存，分散读
* writev将多内存地址上的数据写入到文件描述符，集中写
  
```
#include＜sys/uio.h＞ 
ssize_t readv(int fd,const struct iovec*vector,int count)； 
ssize_t writev(int fd,const struct iovec*vector,int count);
```
* fd参数是被操作的目标文件描述符
* vector参数的类型是iovec结构数组:
   * iovec结构体描述一块内存区
   * 内部由内存基地址和内存长度组成
* count参数是vector数组的长度，即有多少块内存数据需要从fd读出或写到fd
* readv和writev在成功时返回读出/写入fd的字节数，失败则返回-1并设置errno
* 相当于简化版的recvmsg和sendmsg函数
  
* 通过集中写同一将写缓冲区里的回复报文信息和请求的目标文件映射写入到http_conn套接字
  

### 请求处理流程Process()
* 工作线程通过线程池类成员函数`run()`调用本函数
#### process_read()读取请求
* 初始化行状态局部变量为`LINE_OK`
* 初始化http状态码局部变量为`NO_REQUEST`
* 初始化文本`text`
  
* 有限状态机循环读请求：
* 循环条件：
   * a.检查状态码为报文内容且行状态为`LINE_OK`时
   * b.调用`parse_line()`更新行状态，当`LINE_OK`时
1. `get_line()`获取本次循环读缓冲区中的行到`text`
2. 更新开始行计数`m_start_line`
3. 根据检查状态码执行检查：
   1. `CHECK_STATE_REQUESTLINE`请求行:调用`parse_request_line()`分析，失败则修改http状态码`BAD_REQUEST`
   2. `CHECK_STATE_HEADER`报文头：调用`parse_headers()`分析，失败则修改http状态码`BAD_REQUEST`，若返回`GET_REQUEST`则调用`do_request()`完成要求，剩下的情况请求未读完则继续下一轮循环
   3. `CHECK_STATE_CONTENT`报文内容：调用`parse_content()`分析，若返回`GET_REQUEST`则调用`do_request()`完成要求，否则标记行状态为`LINE_OPEN`未读完
   4. 其他检查状态码返回终端错误
  
* `parse_line()`从状态机，行分析
   * 从读缓冲区的检查标记处读取内容直到行末('\r\n')或者可读标记，更新检查标记
   * 未读完返回`LINE_OPEN`
   * 读完返回`LINE_OK`
   * 其他状况返回`LINE_BAD`
  
* `parse_request_line()`分析请求行，获取请求方法，目标url和http版本号
   * `strpbrk()`搜索`text`中\t的位置，截断，获得请求方法字段
   * `strcasecmp()`忽略大小写匹配`GET`或者`POST`(开启cgi)获取请求方法，其他请求类返回`BAD_REQUST`
   * `strspn()`搜索第一个非\t字符，并跳过；`strpbrk()`搜索下一个\t的位置并截断，获取http版本号
   * `strcasecmp()`匹配"HTTP/1.1"，失败则返回`BAD_REQUST`
   * 对剩下字符串用`strcasecmp()`以`strchr()`后向搜索"/"为边界匹配获取目标url地址
   * 如果未指明请求内容则对url使用`strcat()`追加web主页文件名路径
   * 检查状态码复位为`CHECK_STATE_HEADER`
   * 返回http状态码`NO_REQUEST`
  
* `parse_headers()`分析头部信息
   * 若到达字符串尾'\0'，当无内容长度时返回状态码`GET_REQUEST`;有则设置检查状态码为`CHECK_STATE_CONTENT`，返回状态码`NO_REQUEST`
   * `strncasecmp()`匹配"Connection:"且匹配"keep-alive"，设置保持连接标记位`m_linger`
   * `strncasecmp()`匹配"Content-length:",获取报文内容长度，表示存在报文内容需要处理
   * `strncasecmp()`匹配"Host:"，获取主机地址
   * 其他头部信息不做处理，写未知头部日志
   * 返回状态码`NO_REQUEST`
  
* `parse_content()`分析报文内容是否被完整读入
   * 当可读标记位小于检查标记位加上内容长度，表示连接失效，返回状态码`NO_REQUEST`
   * 否则表示完全接收，将`text`存入`m_string`等待分析
  
* `do_request()`完成请求
   * 根据web的实现有各种不同
   * 处理cgi接口
   * 分析登录信息，调用数据库接口查询/注册
    * 注册：获取用户报文中的用户名和密码，首先查询数据库缓存映射表中是否存在同名项目，有则返回重名错误页面；否则加锁调用`mysql_query()`插入记录，并在缓存映射表中插入映射对，解锁，根据数据库操作结果返回登录后页面或者注册错误页面
    * 登录：直接查询缓存映射表是否存在匹配记录，根据结果返回登录后页面或者登录错误页面
 * 逻辑事务处理获得待写回的真实文件名路径`m_real_file`以在写回时指定文件
 * 根据获取指定文件状态信息`m_file_stat`设置http状态码：
    * 信息错误则表示找不到资源文件，返回`NO_RESOURCE`
    * 若`S_IROTH`标志位置一则表示权限错误，返回`FORBIDDEN_REQUEST`
    * 如果指定文件为目录名则表示请求错误，返回`BAD_REQUEST`
    * 此外则表示指定文件状态：正常只读打开指定文件，调用`mmap()`只读，私有地获取指定文件的映射，以便直接从磁盘访问指定文件而不用经过上下文切换，而后关闭打开文件描述符；完成后返回`FILE_REQUEST`
  
* 由`process_read()`返回的状态码判断下一步操作：
   * 当返回`NO_REQUEST`无请求，修改套接字在内核事件表中注册的事件为EPOLLIN等待下次可读
   * 其他返回值表示有数据需要处理，用返回值调用`process_write()`
  
#### process_write()写回请求
* 传入读取处理后设置的http状态码为参数，根据参数做出相应处理
* `INTERNAL_ERROR`：服务器内部错误
   * 调用`add_status_line()`写500错误状态行，其内部调用`add_response()`通过`vsnprintf()`将信息写入写缓冲区，并更新写出标记
   * 调用`add_headers()`写500错误报文头，`add_response()`向写缓冲区写报文内容长度，连接保持或关闭和空行
   * 调用`add_content()`写500错误报文内容，`add_response()`向写缓冲区写具体的错误描述信息
* `BAD_REQUEST`：网页请求错误
   * 同上，向写缓冲区写404错误报文
* `FORBIDDEN_REQUEST`：请求没有权限
   * 同上，向写缓冲区写403错误报文
* 以上错误码处理后，设置写缓冲区到iovec向量，供I/O写回，iovec计数为1，由可写标记获得待发送字节数，返回`true` 
* `FILE_REQUEST`：请求成功，写请求需要的回复报文
   * 调用`add_status_line()`写请求成功状态行
   * 如果如果逻辑处理获得的文件信息大小为了0，报文头和内容写空，返回`false`
   * 否则正常处理请求文件，报文头写指定文件的长度，设置写缓冲区和指定文件映射到iovec向量，以便`writev()`集中写，iovec计数置2，由可写标记和指定文件大小获得待发生字节数，返回`true`
* 其他状态码未处理，返回`false`
  
* 根据`process_read()`的返回值执行操作
   * 失败则调用`close_conn()`关闭连接：控制台打印关闭的套接字，内核事件表移除套接字，类内套接字设置为-1，用户计数减一 **此处操作http_conn类静态成员变量m_user_count为非原子性，线程不安全**
   * 成功则修改套接字内核注册事件为EPOLLOUT表示可写


  
