# KVstorageBaseRaft-cpp

> notice：本项目的目的是学习Raft的原理，实现一个简单的分布式k-v存储数据库。

## 第三方依赖
- boost， 
- muduo
- protoc

**安装说明**
- protoc，本地版本为3.12.4，ubuntu22使用`sudo apt install protobuf-compiler libprotobuf-dev`安装默认就是这个版本
- boost，`sudo apt-get install libboost-dev libboost-test-dev libboost-all-dev`
- muduo,https://blog.csdn.net/QIANGWEIYUAN/article/details/89023980

## 编译
```bash
cd build/ && rm -rf *
cmake .. && make -j8
```
## 运行
```bash
cd bin/

#启动raft集群
./raftCoreRun -n 3 -f test.conf

#启动客户端
./callerMain
```

## 代码讲解




