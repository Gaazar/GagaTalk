# 依赖
c++ fmt
# 单独构建
```
make build
```

# 构建并运行
```
make run
```

# Systemd
修改gagatalk.service，填上路径(‘~’换成构建的父文件夹)
```
cp gagatalk.service /usr/lib/systemd/system/gagatalk.service
sudo systemctl daemon-reload

#开机启动
sudo systemctl enable gagatalk.service

#启动
sudo systemctl start gagatalk.service
```