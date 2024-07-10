/*
持久化哪些内容？
持久化的内容为两部分：1.raft节点的部分信息；2.kvDb的快照

raft节点的部分信息
m_currentTerm ：当前节点的Term，避免重复到一个Term，可能会遇到重复投票等问题。
m_votedFor ：当前Term给谁投过票，避免故障后重复投票。
m_logs ：raft节点保存的全部的日志信息。

kvDb的快照
m_lastSnapshotIncludeIndex ：快照的信息，快照最新包含哪个日志Index
m_lastSnapshotIncludeTerm ：快照的信息，快照最新包含哪个日志Term，与m_lastSnapshotIncludeIndex 是对应的。
Snapshot是kvDb的快照，也可以看成是日志，因此:全部的日志 = snapshot + m_logs
因为Snapshot是kvDB生成的，kvDB肯定不知道raft的存在，而什么term、什么日志Index都是raft才有的概念，因此snapshot中肯定没有term和index信息。
所以需要raft自己来保存这些信息。
故，快照与m_logs联合起来理解即可。

为什么要持久化这些内容: 共识安全、优化(除了快照, 其他都是为了共识安全)

什么时候需要进行持久化操作(落盘操作)：当需要持久化的内容发送变化时

怎么实现持久化的：目前是把数据转成string落盘，后续可以说是用proto序列化成2进制数据落盘
*/

#ifndef SKIP_LIST_ON_RAFT_PERSISTER_H
#define SKIP_LIST_ON_RAFT_PERSISTER_H
#include <fstream>
#include <mutex>

//用于持久化状态和快照,即把m_raftState和m_snapshot两个string落盘
class Persister {
 private:
  std::mutex m_mtx;
  std::string m_raftState;
  std::string m_snapshot;
  /**
   * m_raftStateFileName: raftState文件名
   */
  const std::string m_raftStateFileName;
  /**
   * m_snapshotFileName: snapshot文件名
   */
  const std::string m_snapshotFileName;
  /**
   * 保存raftState的输出流
   */
  std::ofstream m_raftStateOutStream;
  /**
   * 保存snapshot的输出流
   */
  std::ofstream m_snapshotOutStream;
  /**
   * 保存raftStateSize的大小
   * 避免每次都读取文件来获取具体的大小
   */
  long long m_raftStateSize;

 public:
  void Save(std::string raftstate, std::string snapshot); //保存m_raftState和m_snapshot
  std::string ReadSnapshot();                             //读取快照
  void SaveRaftState(const std::string& data);            //只保存state
  long long RaftStateSize();                              //读取Raft.size
  std::string ReadRaftState();                            //读取state
  explicit Persister(int me);                             //构造函数初始化
  ~Persister();

 private:
  void clearRaftState();                                  //这3个都是清空
  void clearSnapshot();
  void clearRaftStateAndSnapshot();
};

#endif  // SKIP_LIST_ON_RAFT_PERSISTER_H
