//
// Created by swx on 23-5-30.
//

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
