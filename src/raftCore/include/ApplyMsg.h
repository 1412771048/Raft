#ifndef APPLYMSG_H
#define APPLYMSG_H
#include <string>

//应用消息类，可用于命令应用、快照应用
class ApplyMsg {
 public:
  bool CommandValid;
  std::string Command;
  int CommandIndex;
  bool SnapshotValid;
  std::string Snapshot;
  int SnapshotTerm;
  int SnapshotIndex;

 public:
  //两个valid最开始要赋予false！！
  ApplyMsg(): CommandValid(false),
              Command(),
              CommandIndex(-1),
              SnapshotValid(false),
              SnapshotTerm(-1),
              SnapshotIndex(-1)
  {

  }
};

#endif  // APPLYMSG_H