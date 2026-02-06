
### TODO
分布式存储
分布一致性
接入数据库存储

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

### store Payload
实现store接口和get接口，目前还是内存存储
{
  "image_id": "img_0001",
  "width": 1920,
  "height": 1080,
  "image_hash": "a1b2c3d4...",
  "boxes": [
    { "class_id": 0, "x": 0.52, "y": 0.33, "w": 0.12, "h": 0.18, "confidence": 0.94 },
    { "class_id": 2, "x": 0.22, "y": 0.61, "w": 0.08, "h": 0.10, "confidence": 0.88 }
  ]
}
