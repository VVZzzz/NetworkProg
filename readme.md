#网络编程模型
本仓库为Windows,Linux下各种常见的网络模型总结.   
  
### windows
- 简单阻塞/非阻塞socket
- I/O复用(检测I/O事件再处理)模型
	```select```模型
- ```重叠I/O```+```Compelete Routine```模型
- ```WSAEventSelect```  ```WSAAsyncSelect```模型
- ```IOCP```完成端口模型

### Linux
- 简单阻塞/非阻塞socket
- I/O复用(检测I/O事件再处理)模型
 典型的有```select```,```poll```,```epoll```
- 一些异步的/网络库 网络模型