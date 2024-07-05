//
// Created by swx on 23-12-28.
//

#ifndef RAFTRPC_H
#define RAFTRPC_H

#include "raftRPC.pb.h"

/// @brief 维护当前节点对其他某一个结点的所有rpc发送通信的功能
// 对于一个raft节点来说，对于任意其他的节点都要维护一个rpc连接，即MprpcChannel

//可简单理解为用于当前节点向指定节点(ip+port)发送rpc请求
class RaftRpcUtil {
 private:
  raftRpcProctoc::raftRpc_Stub *stub_; //用于发送rpc请求，一般客户端用stub来写

 public:
  //主动调用其他节点的三个方法,可以按照mit6824来调用，但是别的节点调用自己的好像就不行了，要继承protoc提供的service类才行

  //日志复制
  bool AppendEntries(raftRpcProctoc::AppendEntriesArgs *args, raftRpcProctoc::AppendEntriesReply *response);
  //快照安装
  /*
    raft通过日志来实现同步，随着时间的推移，日志条目越来越多，一占空间、二发送耗资源，还占空间，所以必须引入快照，记录某个时间点的完整状态
    后续只需传输快照及其之后的日志条目，即可完成同步
  */
  bool InstallSnapshot(raftRpcProctoc::InstallSnapshotRequest *args, raftRpcProctoc::InstallSnapshotResponse *response);
  //拉票
  bool RequestVote(raftRpcProctoc::RequestVoteArgs *args, raftRpcProctoc::RequestVoteReply *response);
  //响应其他节点的方法
  /**
   *
   * @param ip  远端ip
   * @param port  远端端口
   */
  RaftRpcUtil(std::string ip, short port);
  ~RaftRpcUtil();
};

#endif  // RAFTRPC_H
