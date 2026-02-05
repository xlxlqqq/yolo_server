

# 日志系统
- 支持fmt
- 支持__FUNC__, __LINE__
- 支持时间戳

# 服务配置系统
- 支持从配置文件读取端口号等配置
- 支持

# 生命周期
main()
 └── Server server
      ├── start()
      │    ├── setupSocket()
      │    └── 启动 accept 线程
      │          └── acceptLoop()
      │                └── accept() 阻塞
      │
      ├── wait()
      │    └── 等 accept 线程结束
      │
SIGINT / SIGTERM
 └── handleSignal()
      └── server.stop()
           ├── m_running = false
           ├── close(listen_fd)
           └── accept() 返回 → 线程退出

Server
 └── acceptLoop()
      └── accept()
           └── fd
                └── Connection(fd)
                     └── start()
                          └── recv/send/close