# Mysql连接类connection_pool
## 获取实例
* 单例模式，私有化构造函数，通过`GetInstance()`获取唯一实例
## 初始化
* 初始化数据库连接池
 * 设置url，用户，密码，数据库名称，端口（3306），最大连接数（8），日志开关（0关）
 * 创建8个MYSQL实体，建立真实数据库连接，添加至连接链表connlist，增加可用连接数m_FreeConn
 * 以可用连接数8创建信号量reserve，多线程锁信号量sem
 * 用当前可用连接数设置最大连接数m_Maxconn
  
## RAII资源获取即初始化(释放即回收)
* 通过`connectionRAII`类实例来申请数据库连接，可在类失效时自动调用析构函数回收可用的数据库连接，以实现自动化资源回收
* 在工作线程`run()`过程中调用`process()`处理事务前申请连接实例，退出当前`run()`后回收

* 构造函数根据传入的SQL连接池和http_conn类成员mysql的地址将获取的SQL连接实例传递到mysql中
 * 内部调用SQL连接池的成员函数`GetConnection()`来获取连接
 * 用`connectionRAII`的成员变量记录使用的SQL实例和SQL连接池以便析构时回收
* 析构函数调用SQL连接池类的成员函数`ReleaseConnection()`回收申请的连接

## 获取SQL连接GetConnection
* 首先判断SQL连接链表`connList`是否非空，空则返回`NULL`
* `reseve`信号量调用`wait()`，信号量值减一
 * 当信号量值已经为0则表示无可用连接，阻塞等待到有连接可用
* 加锁进行临界区操作
 * 从连接链表`connList`获取一个连接并弹出
 * 更新SQL连接池的可用连接计数和空闲连接计数
 * 解锁退出临界区
* 返回获得的SQL连接

## 释放SQL连接ReleaseConnection
* 首先判断待释放连接是否为空，空则返回`false`
* 加锁进行临界区操作
 * 更新SQL连接池的可用连接计数和当前连接计数
 * 解锁退出临界区
* `reserve`信号量调用`post()`将信号量值加一，以便唤醒一个处于阻塞等待信号量的线程（如果有）工作
* 返回`true`

## SQL连接池的析构
* 析构函数调用`DestroyPool()`销毁连接池
* 内部加锁进入临界区操作
 * 判断`connList`连接链表非空，空则解锁退出
 * 遍历链表，对每个连接调用`mysql_close()`关闭连接
 * SQL连接池的当前连接计数和可用连接计数归零
 * 链表`connList`调用`clear()`清空
