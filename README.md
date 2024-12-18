# linux-cpp-rudp

> 在Linux下使用CPP编写的可靠UDP协议实现

## 实现以下功能：

- 建立连接三次握手
- 差错检测：检查消息类型、序列号、校验和
- 确认重传：包括差错重传和超时重传
- 流量控制：停等机制
- 断开连接四次握手

对文件传输进行了测试

## 编译

- 克隆项目到本地：

```bash
    git clone https://github.com/pqcqaq/linux-cpp-rudp.git
```

- 进入项目目录：

```bash 
    cd linux-cpp-rudp
```

- 新建build目录

```bash
    mkdir build && cd build
```

- 使用cmake进行编译

```bash
    cmake .. && make
````

## 使用编译后的可执行文件：

- 使用./server \<port\> \<filename\>的形式启动服务器.
- 使用./client \<host\>:\<port\> \<filename\>的形式来打开客户端

> 在成功建立连接后，会将客户端的文件传输到服务端，然后再将服务端的文件下载下来 在下载完后，进行挥手，关闭连接


