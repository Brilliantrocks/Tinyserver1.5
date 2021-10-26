# epoll
## 内核事件表
* epoll是Linux特有的I/O复用函数
* epoll把用户关心的文件描述符上的事件放在内核里的一个事件表中，从而无须像select和poll那样每次调用都要重复传入文件描述符集或事件集
* 但epoll需要使用一个额外的文件描述符，来唯一标识内核中的这个事件表：
 `int epoll_create(int size)`  
* size参数现在并不起作用，只是给内核一个提示，告诉它事件表需要多大
* 该函数返回的文件描述符将用作其他所有epoll系统调用的第一个参数，以指定要访问的内核事件表
  
* `int epoll_ctl(int epfd,int op,int fd,struct epoll_event*event)`操作内核事件表
* fd参数是要操作的文件描述符，op参数则指定操作类型。操作类型有如下3种：
	 1. EPOLL_CTL_ADD，往事件表中注册fd上的事件
	 2. EPOLL_CTL_MOD，修改fd上的注册事件
	 3. EPOLL_CTL_DEL，删除fd上的注册事件

* event参数指定事件，它是epoll_event结构指针类型。epoll_event的定义如下：
```
struct epoll_event
{
	__uint32_t events;//epoll
	epoll_data_t data;//用户数据
}
```
* events成员描述事件类型，支持的事件类型和poll基本相同，在poll相应的宏前加E，如EPOLLIN
* epoll特有两个额外的事件类型：EPOLLET 和 EPOLLONESHOT
  
* data成员用于存储用户数据：
```
typedef union epoll_data
{
	void* ptr;
	int fd;
	uint32_t u32;
	uint64_t u64;
}epoll_data_t
```
* union中使用最多的成员是fd，它指定事件所从属的目标文件描述符
* ptr成员可用来指定与fd相关的用户数据
* 由于不能同时使用多种成员，如果要将文件描述符和用户数据关联起来（如将句柄和事件处理器绑定），以实现快速的数据访问，只能使用其他手段，比如放弃使用epoll_data_t的fd成员，而在ptr指向的用 户数据中包含fd
* epoll_ctl成功时返回0，失败则返回-1并设置errno
  
## epoll事件表
  
  
|事件|描述|是否可以作为输入|是否可作为输出|
|---|---|---|---|
|EPOLLIN|数据(普通/优先)可读|是|是|
|EPOLLRDNORM|普通数据可读|是|是|
|EPOLLRDBAND|优先带数据可读(linux不支持)|是|是|
|EPOLLPRI|高级优先数据可读如TCP带外数据|是|是|
|EPOLLOUT|数据(普通/优先)可写|是|是|
|EPOLLWRNORM|普通数据可写|是|是|
|EPOLLWRBAND|优先级带数据可写|是|是|
|EPOLLRDHUP|TCP连接被对方关闭，或者对方关闭了写操作，GNU引入|是|是|
|EPOLLERR|错误|否|是|
|EPOLLHUP|挂起，如管道的写端关闭后，读端描述符上接收到EPOLLHUP|否|是|
|EPOLLNVAL|文件描述符没有打开|否|是|
|EPOLLET|标识该文件描述符以ET模式操作|/|/|
|EPOLLONESHOT||||
  
* NORM BAND后缀事件是XOPEN规范定义，用于细分输入输出事件，以区别对待普通优先数据，linux不完全支持
## epoll_wait
* epoll系列系统调用的主要接口是epoll_wait函数。它在一段超时时间内等待一组文件描述符上的事件， 其原型如下
`int epoll_wait(int epfd,struct epoll_event*events,int maxevents,int timeout);`
* timeout参数同poll接口的timeout参数  
	 * 成功时返回就绪的文件描述符的个数，失败时返回-1并设置errno
	 * timeout参数指定epoll的超时值，单位是毫秒
	 * 当timeout为-1时，epoll调用将永远阻塞，直到某个事件发生；当timeout为0时，poll调用将立即返回
* maxevents 参数指定最多监听多少个事件，它必须大于0
  
* epoll_wait函数如果检测到事件，就将所有就绪的事件从内核事件表（由epfd参数指定）中复制到它的第二个参数events指向的数组中
* 这个数组只用于输出epoll_wait检测到的就绪事件，而不像select和poll的数组参数那样既用于传入用户注册的事件，又用于输出内核检测到的就绪事件
  
## epoll的LT和ET模式
* epoll对文件描述符的操作有两个模式LT水平触发和ET边沿触发
* LT模式是默认的工作模式，这种模式下epoll相当于一个效率较高的poll
* 当往epoll内核事件表中注册一个文件描述符上的EPOLLET事件时，epoll将以ET模式来操作该文件描述符
* ET模式是epoll的高效工作模式
  
### LT水平触发
* 当epoll_wait检测到其上有事件发生并将此事件通知应用程序后， 应用程序可以不立即处理该事件
* 这样，当应用程序下一次调用epoll_wait时，epoll_wait还会再次向应用程序通告此事件，直到该事件被处理
* 关键：同一事件epoll_wait可以在处理前重复检测，在events事件表中不需要立即处理
  
```
//将文件描述符设置为非阻塞
int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

//文件描述符fd上的EPOLLIN注册到epollfd的epoll内核事件表中，enable_et指定是否启用ET
void addfd(int epollfd, int fd, bool enable_et)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN;
	if(enable_et)
	{
		event.events |= EPOLLET;
	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}
 
// LT工作流程
void lt(epoll_event*events,int number,int epollfd,int listenfd)
{
	char buf[BUFFER_SIZE];
	for(int i=0;i＜number;i++)
	{
		int sockfd=events[i].data.fd;
		if(sockfd==listenfd)
		{
			struct sockaddr_in client_address;
			socklen_t client_addrlength=sizeof(client_address);
			int connfd=accept(listenfd,(struct sockaddr*)＆client_address, ＆client_addrlength); 
			addfd(epollfd,connfd,false);/*对connfd禁用ET模式*/ 
		}
		else if(events[i].events＆EPOLLIN) 
		{
			/*只要socket读缓存中还有未读出的数据，这段代码就被触发*/ 
			printf("event trigger once\n"); 
			memset(buf,'\0',BUFFER_SIZE); 
			int ret=recv(sockfd,buf,BUFFER_SIZE-1,0); 
			if(ret＜=0) 
			{
				close(sockfd); 
				continue; 
			}
			printf("get%d bytes of content:%s\n",ret,buf);
		}
		else 
		{
			printf("something else happened\n"); 
		}
	}
}
```
  
### ET边沿触发
* 当epoll_wait检测到其上有事件发生并将此事件通知应用程序后，应用程序必须立即处理该事件，因为后续的epoll_wait调用将不再向应用程序通知这一事件
* 很大程度上降低了同一个epoll事件被重复触发的次数，因此效率要比LT模式高
* 关键：一旦检测并写入events事件表，必须完全处理，epollwait不再检测该事件
```
//ET工作流程
void et(epoll_event*events,int number,int epollfd,int listenfd) 
{
	char buf[BUFFER_SIZE]; 
	for(int i=0;i＜number;i++) 
	{
		int sockfd=events[i].data.fd; 
		if(sockfd==listenfd) 
		{
			struct sockaddr_in client_address;
			socklen_t client_addrlength=sizeof(client_address); 
			int connfd=accept(listenfd,(struct sockaddr*)＆client_address,＆client_addrlength); 
			addfd(epollfd,connfd,true);/*对connfd开启ET模式*/ 
		}
		else if(events[i].events＆EPOLLIN) 
		{
			/*这段代码不会被重复触发，所以我们循环读取数据，以确保把socket读缓存中的所有数据读出*/ printf("event trigger once\n"); 
			while(1) 
			{
				memset(buf,'\0',BUFFER_SIZE); 
				int ret=recv(sockfd,buf,BUFFER_SIZE-1,0); 
				if(ret＜0) 
				{
					/*
					对于非阻塞IO，下面的条件成立表示数据已经全部读取完毕。此后，epoll就能再次触发sockfd上的EPOLLIN事件，以驱动下一次读操作
					*/ 
					if((errno==EAGAIN)||(errno==EWOULDBLOCK)) 
					{
						printf("read later\n"); 
						break; 
					}
					close(sockfd);
					break;
				}
				else if(ret==0) 
				{
					close(sockfd);
				}
				else 
				{
					printf("get%d bytes of content:%s\n",ret,buf); 
				}
			}
		}
		else 
		{
			printf("something else happened\n"); 
		}
	}
}
```
* ET模式下事件被触发的次数比LT模式少很多
* 每个使用ET模式的文件描述符都是非阻塞的
  
##EPOLLONESHOT事件
* ET模式下同一套接字的某个事件仍可能被多次触发，如线程a读取套接字s上的数据后正在处理数据，s上又有新的数据可读(EPOLLIN)再次触发，线程b被唤醒处理新数据；此时存在线程a和b同时操作套接字s的局面
* 需要使同一连接套接字无论何时都只被一个线程处理，故使用EPOLLONESHOT事件实现
  
* 对于注册了EPOLLONESHOT事件的文件描述符，操作系统最多触发其上注册的一个可读、可写或者异常事件，且只触发一次，除非我们使用epoll_ctl函数重置该文件描述符上注册的EPOLLONESHOT事件
* 当一个线程在处理某个socket时，其他线程是不可能有机会操作该socket的
* 注册了EPOLLONESHOT事件的socket一旦被某个线程处理完毕，该线程就应该立即重置这个socket上的EPOLLONESHOT事件，以确保这个socket下一次可读时，其EPOLLIN事件能被触发，进而让其他工作线程有机会继续处理这个socket
  
* listenfd上是不能注册EPOLLONESHOT事件的，否则应用程序只能处理一个客户连接，因为后续的客户连接请求将不再触发listenfd上的EPOLLIN事件
  
```
void reset_oneshot(int epollfd,int fd) 
{
	epoll_event event; 
	event.data.fd=fd; 
	event.events=EPOLLIN|EPOLLET|EPOLLONESHOT; 
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,＆event);
}
/*工作线程*/ 
void*worker(void*arg) 
{
	int sockfd=((fds*)arg)-＞sockfd;
 	int epollfd=((fds*)arg)-＞epollfd; 
	printf("start new thread to receive data on fd:%d\n",sockfd); 
	char buf[BUFFER_SIZE]; 
	memset(buf,'\0',BUFFER_SIZE); 
	/*循环读取sockfd上的数据，直到遇到EAGAIN错误*/ 
	while(1) 
	{
		int ret=recv(sockfd,buf,BUFFER_SIZE-1,0); 
		if(ret==0) 
		{
			close(sockfd); 
			printf("foreiner closed the connection\n"); 
			break; 
		}
		else if(ret＜0) 
		{
			if(errno==EAGAIN) 
			{
				reset_oneshot(epollfd,sockfd);//重置EPOLLONESHOT事件 
				printf("read later\n"); 
				break; 
			}
		}
		else 
		{
			printf("get content:%s\n",buf);
			/*休眠5s，模拟数据处理过程*/ 
			sleep(5);
		}
	}
printf("end thread receiving data on fd:%d\n",sockfd); 
}
```
* 如果一个工作线程处理完某个socket上的一次请求（我们用休眠5s来模拟这个过程）之后，又接收到socket上新的客户请求，套接字可读，则该线程将继续为这个socket服务
* 并且因为该socket上注册了EPOLLONESHOT事件，其他线程没有机会接触这个socket，如果工作线程等待5s后仍然没收到该socket上的下一批客户数据，则它将放弃为该socket服务
* 调用reset_oneshot函数来重置该socket上的注册事件，这将使epoll有机会再次检测到该socket上的EPOLLIN事件，进而使得其他线程有机会为该socket服务
