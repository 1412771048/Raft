# CPPRaft系列-raft算法主要流程函数实现-03

[来自： 代码随想录](https://wx.zsxq.com/dweb2/index/group/88511825151142)

![用户头像](https://images.zsxq.com/FgTbimn1Oo952em4DBScoknjuuI7?e=2064038400&token=kIxbL07-8jAj8w1n4s9zv64FuZZNEATmlU_Vm6zD:J8cjZzXS1mXqiuPBLCSl0Kt_XBA=)思无邪

2023年12月24日 21:25



raft算法主要流程函数实现



本节主要讲解raft算法的关键函数实现。

> 开篇之前先到其他部分学习下 快照snapshot 的概念。
>
> 终于可以见到代码了，代码仓库件文末 【后期内容预告！】部分

### raft类的定义



重点关注成员变量的作用，成员函数很多都是辅助功能，重点的函数会在后面详细讲解的。



无注释版

class Raft :

{

private:

​    std::mutex m_mtx;

​    std::vector<std::shared_ptr< RaftRpc >> m_peers; 

​    std::shared_ptr<Persister> m_persister;  

​    int m_me;             

​    int m_currentTerm;   

​    int m_votedFor;       

​    std::vector<mprrpc:: LogEntry> m_logs; 

​    int m_commitIndex;

​    int m_lastApplied; 

​    std::vector<int> m_nextIndex; 

​    std::vector<int> m_matchIndex;

​    enum Status

​    {

​        Follower,

​        Candidate,

​        Leader

​    };

​    // 身份

​    Status m_status;

​    std::shared_ptr<LockQueue<ApplyMsg>> applyChan  ;    

​    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime;

​    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime;

​    int m_lastSnapshotIncludeIndex;

​    int m_lastSnapshotIncludeTerm;

public:

​    void AppendEntries1(const mprrpc::AppendEntriesArgs *args, mprrpc::AppendEntriesReply *reply);

​    void applierTicker();

​    bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot);

​    void doElection();

​    void doHeartBeat();

​    

​    void electionTimeOutTicker();

​    std::vector<ApplyMsg> getApplyLogs();

​    int getNewCommandIndex();

​    void getPrevLogInfo(int server, int *preIndex, int *preTerm);

​    void GetState(int *term, bool *isLeader);

​    void InstallSnapshot( const mprrpc::InstallSnapshotRequest *args, mprrpc::InstallSnapshotResponse *reply);

​    void leaderHearBeatTicker();

​    void leaderSendSnapShot(int server);

​    void leaderUpdateCommitIndex();

​    bool matchLog(int logIndex, int logTerm);

​    void persist();

​    void RequestVote(const mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply *reply);

​    bool UpToDate(int index, int term);

​    int getLastLogIndex();

​    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm);

​    int getLogTermFromLogIndex(int logIndex);

​    int GetRaftStateSize();

​    int getSlicesIndexFromLogIndex(int logIndex);

​    bool sendRequestVote(int server , std::shared_ptr<mprrpc::RequestVoteArgs> args ,  std::shared_ptr<mprrpc::RequestVoteReply> reply,   std::shared_ptr<int> votedNum) ;

​    bool sendAppendEntries(int server ,std::shared_ptr<mprrpc::AppendEntriesArgs> args , std::shared_ptr<mprrpc::AppendEntriesReply> reply , std::shared_ptr<int> appendNums ) ;

​    void pushMsgToKvServer(ApplyMsg msg);

​    void readPersist(std::string data);

​    std::string persistData();

​    void Start(Op command,int* newLogIndex,int* newLogTerm,bool* isLeader ) ;

​    void Snapshot(int index , std::string snapshot );

public:

​    void init(std::vector<std::shared_ptr< RaftRpc >> peers,int me,std::shared_ptr<Persister> persister,std::shared_ptr<LockQueue<ApplyMsg>> applyCh);





带注释版：



class Raft :

{

private:

​    std::mutex m_mtx;

​    std::vector<std::shared_ptr< RaftRpc >> m_peers; //需要与其他raft节点通信，这里保存与其他结点通信的rpc入口

​    std::shared_ptr<Persister> m_persister;   //持久化层，负责raft数据的持久化

​    int m_me;             //raft是以集群启动，这个用来标识自己的的编号

​    int m_currentTerm;    //记录当前的term

​    int m_votedFor;       //记录当前term给谁投票过

​    std::vector<mprrpc:: LogEntry> m_logs; //// 日志条目数组，包含了状态机要执行的指令集，以及收到领导时的任期号

​    // 这两个状态所有结点都在维护，易失

​    int m_commitIndex;

​    int m_lastApplied; // 已经汇报给状态机（上层应用）的log 的index

​    // 这两个状态是由leader来维护，易失 ，这两个部分在内容补充的部分也会再讲解

​    std::vector<int> m_nextIndex; // 这两个状态的下标1开始，因为通常commitIndex和lastApplied从0开始，应该是一个无效的index，因此下标从1开始

​    std::vector<int> m_matchIndex;

​    enum Status

​    {

​        Follower,

​        Candidate,

​        Leader

​    };

​    // 保存当前身份

​    Status m_status;

​    std::shared_ptr<LockQueue<ApplyMsg>> applyChan;     // client从这里取日志，client与raft通信的接口

​    // ApplyMsgQueue chan ApplyMsg // raft内部使用的chan，applyChan是用于和服务层交互，最后好像没用上

​	

​    // 选举超时

​    std::chrono::_V2::system_clock::time_point m_lastResetElectionTime;

​    // 心跳超时，用于leader

​    std::chrono::_V2::system_clock::time_point m_lastResetHearBeatTime;

​    // 用于传入快照点

​    // 储存了快照中的最后一个日志的Index和Term

​    int m_lastSnapshotIncludeIndex;

​    int m_lastSnapshotIncludeTerm;

public:

​    

​    void AppendEntries1(const mprrpc::AppendEntriesArgs *args, mprrpc::AppendEntriesReply *reply); //日志同步 + 心跳 rpc ，重点关注

​    void applierTicker();     //定期向状态机写入日志，非重点函数

​    

​    bool CondInstallSnapshot(int lastIncludedTerm, int lastIncludedIndex, std::string snapshot);    //快照相关，非重点

​    void doElection();    //发起选举

​    void doHeartBeat();    //leader定时发起心跳

​    // 每隔一段时间检查睡眠时间内有没有重置定时器，没有则说明超时了

// 如果有则设置合适睡眠时间：睡眠到重置时间+超时时间

​    void electionTimeOutTicker();   //监控是否该发起选举了

​    std::vector<ApplyMsg> getApplyLogs();

​    int getNewCommandIndex();

​    void getPrevLogInfo(int server, int *preIndex, int *preTerm);

​    void GetState(int *term, bool *isLeader);  //看当前节点是否是leader

​    void InstallSnapshot( const mprrpc::InstallSnapshotRequest *args, mprrpc::InstallSnapshotResponse *reply);  

​    void leaderHearBeatTicker(); //检查是否需要发起心跳（leader）

​    void leaderSendSnapShot(int server);  

​    void leaderUpdateCommitIndex();  //leader更新commitIndex

​    bool matchLog(int logIndex, int logTerm);  //对应Index的日志是否匹配，只需要Index和Term就可以知道是否匹配

​    void persist();   //持久化

​    void RequestVote(const mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply *reply);    //变成candidate之后需要让其他结点给自己投票

​    bool UpToDate(int index, int term);   //判断当前节点是否含有最新的日志

​    int getLastLogIndex();

​    void getLastLogIndexAndTerm(int *lastLogIndex, int *lastLogTerm);

​    int getLogTermFromLogIndex(int logIndex);

​    int GetRaftStateSize();

​    int getSlicesIndexFromLogIndex(int logIndex);   //设计快照之后logIndex不能与在日志中的数组下标相等了，根据logIndex找到其在日志数组中的位置

​    bool sendRequestVote(int server , std::shared_ptr<mprrpc::RequestVoteArgs> args ,  std::shared_ptr<mprrpc::RequestVoteReply> reply,   std::shared_ptr<int> votedNum) ; // 请求其他结点的投票

​    bool sendAppendEntries(int server ,std::shared_ptr<mprrpc::AppendEntriesArgs> args , std::shared_ptr<mprrpc::AppendEntriesReply> reply , std::shared_ptr<int> appendNums ) ;  //Leader发送心跳后，对心跳的回复进行对应的处理

​    //rf.applyChan <- msg //不拿锁执行  可以单独创建一个线程执行，但是为了同意使用std:thread ，避免使用pthread_create，因此专门写一个函数来执行

​    void pushMsgToKvServer(ApplyMsg msg);  //给上层的kvserver层发送消息

​    void readPersist(std::string data);    

​    std::string persistData();

​    void Start(Op command,int* newLogIndex,int* newLogTerm,bool* isLeader ) ;   // 发布发来一个新日志

// 即kv-server主动发起，请求raft（持久层）保存snapshot里面的数据，index是用来表示snapshot快照执行到了哪条命令

​    void Snapshot(int index , std::string snapshot );

public:

​    void init(std::vector<std::shared_ptr< RaftRpc >> peers,int me,std::shared_ptr<Persister> persister,std::shared_ptr<LockQueue<ApplyMsg>> applyCh);		//初始化



看到这么多函数说实话人都麻了，那梳理一下吧。



对于这么多函数其实需要关注的并不多，重点需要关注的：



1. Raft的主要流程：领导选举（`sendRequestVote RequestVote` ） 日志同步、心跳（`sendAppendEntries`  `AppendEntries` ）
2. 定时器的维护：主要包括raft向状态机定时写入（`applierTicker` ）、心跳维护定时器（`leaderHearBeatTicker` ）、选举超时定时器（`electionTimeOutTicker` ）。



1. 持久化相关：包括哪些内容需要持久化，什么时候需要持久化（persist）



这样看起来关键的就是只有几个函数了。

> 需要再次说明的是，本系列的作用是帮助快速理解raft，但是无法代替思考。比如raft在各种情况下的故障如何保证正确性需要自行多多思考。



### 启动初始化



无注释版本：



void Raft::init(std::vector<std::shared_ptr<RaftRpc>> peers, int me, std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyCh) {

​    m_peers = peers;  

​    m_persister = persister;

​    m_me = me;

​    m_mtx.lock();

​    //applier

​    this->applyChan = applyCh;

//    rf.ApplyMsgQueue = make(chan ApplyMsg)

​    m_currentTerm = 0;

​    m_status = Follower;

​    m_commitIndex = 0;

​    m_lastApplied = 0;

​    m_logs.clear();

​    for (int i =0;i<m_peers.size();i++){

​        m_matchIndex.push_back(0);

​        m_nextIndex.push_back(0);

​    }

​    m_votedFor = -1;

​    m_lastSnapshotIncludeIndex =0;

​    m_lastSnapshotIncludeTerm = 0;

​    m_lastResetElectionTime = now();

​    m_lastResetHearBeatTime = now();

​    // initialize from state persisted before a crash

​    readPersist(m_persister->ReadRaftState());

​    if(m_lastSnapshotIncludeIndex > 0){

​        m_lastApplied = m_lastSnapshotIncludeIndex;

​        //rf.commitIndex = rf.lastSnapshotIncludeIndex   todo ：崩溃恢复为何不能读取commitIndex

​    }

​    m_mtx.unlock();

​    // start ticker goroutine to start elections

​    std::thread t(&Raft::leaderHearBeatTicker, this);

​    t.detach();

​    std::thread t2(&Raft::electionTimeOutTicker, this);

​    t2.detach();

​    std::thread t3(&Raft::applierTicker, this);

​    t3.detach();

}





带注释版：



void Raft::init(std::vector<std::shared_ptr<RaftRpc>> peers, int me, std::shared_ptr<Persister> persister, std::shared_ptr<LockQueue<ApplyMsg>> applyCh) {

​    m_peers = peers;     //与其他结点沟通的rpc类

​    m_persister = persister;   //持久化类

​    m_me = me;    //标记自己，毕竟不能给自己发送rpc吧

​    m_mtx.lock();

​    //applier

​    this->applyChan = applyCh;   //与kv-server沟通

//    rf.ApplyMsgQueue = make(chan ApplyMsg)

​    m_currentTerm = 0;   //初始化term为0

​    m_status = Follower;   //初始化身份为follower

​    m_commitIndex = 0;  

​    m_lastApplied = 0;

​    m_logs.clear();

​    for (int i =0;i<m_peers.size();i++){

​        m_matchIndex.push_back(0);

​        m_nextIndex.push_back(0);

​    }

​    m_votedFor = -1;    //当前term没有给其他人投过票就用-1表示

​    m_lastSnapshotIncludeIndex = 0;

​    m_lastSnapshotIncludeTerm = 0;

​    m_lastResetElectionTime = now();

​    m_lastResetHearBeatTime = now();

​    // initialize from state persisted before a crash

​    readPersist(m_persister->ReadRaftState());

​    if(m_lastSnapshotIncludeIndex > 0){

​        m_lastApplied = m_lastSnapshotIncludeIndex;

​        //rf.commitIndex = rf.lastSnapshotIncludeIndex 崩溃恢复不能读取commitIndex

​    }

​    m_mtx.unlock();

​    // start ticker  开始三个定时器

​    std::thread t(&Raft::leaderHearBeatTicker, this);

​    t.detach();

​    std::thread t2(&Raft::electionTimeOutTicker, this);

​    t2.detach();

​    std::thread t3(&Raft::applierTicker, this);

​    t3.detach();

}



从上面可以看到一共产生了三个定时器，分别维护：选举、日志同步和心跳、raft节点与kv-server的联系。相互之间是比较隔离的。



### 选leader



主要涉及函数及其流程：



![img](https://article-images.zsxq.com/FjzFfi6jY-SnRVKFrYXoDaKPSNry)





**electionTimeOutTicker**：负责查看是否该发起选举，如果该发起选举就执行doElection发起选举。



**doElection**：实际发起选举，构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。



**sendRequestVote**：负责发送选举中的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。



**RequestVote**：接收别人发来的选举请求，主要检验是否要给对方投票。





#### **electionTimeOutTicker**:



选举超时由于`electionTimeOutTicker` 维护。



void Raft::electionTimeOutTicker() {

​    // Check if a Leader election should be started.

​    while (true) {

​        m_mtx.lock();

​        auto nowTime = now(); //睡眠前记录时间

​        auto suitableSleepTime = getRandomizedElectionTimeout() + m_lastResetElectionTime - nowTime;

​        m_mtx.unlock();

​        if (suitableSleepTime.count() > 1) {

​            std::this_thread::sleep_for(suitableSleepTime);

​        }

​        if ((m_lastResetElectionTime - nowTime).count() > 0) {  //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠

​            continue;

​        }

​        doElection();

​    }

}



在死循环中，首先计算距离下一次超时应该睡眠的时间suitableSleepTime，然后睡眠这段时间，醒来后查看睡眠的这段时间选举超时定时器是否被触发，如果没有触发就发起选举。



> “举超时定时器是否被触发”：选举定时器的触发条件：收到leader发来的appendEntryRPC 、给其他的节点选举投票



在死循环中，首先计算距离上次重置选举计时器的时间加上随机化的选举超时时间，然后线程根据这个时间决定是否睡眠。若超时时间未到，线程进入睡眠状态，若在此期间选举计时器被重置，则继续循环。若超时时间已到，调用`doElection()` 函数启动领导者选举过程。



#### **doElection** ：



无注释版

void Raft::doElection() {

​    lock_guard<mutex> g(m_mtx);

​    if (m_status != Leader) {

​        

​        m_status = Candidate;

​        m_currentTerm += 1;

​        m_votedFor = m_me; 

​        persist();

​        std::shared_ptr<int> votedNum = std::make_shared<int>(1); 

​        m_lastResetElectionTime = now();

​        for (int i = 0; i < m_peers.size(); i++) {

​            if (i == m_me) {

​                continue;

​            }

​            int lastLogIndex = -1, lastLogTerm = -1;

​            getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);//获取最后一个log的term和下标

​            std::shared_ptr<mprrpc::RequestVoteArgs> requestVoteArgs = std::make_shared<mprrpc::RequestVoteArgs>();

​            requestVoteArgs->set_term(m_currentTerm);

​            requestVoteArgs->set_candidateid(m_me);

​            requestVoteArgs->set_lastlogindex(lastLogIndex);

​            requestVoteArgs->set_lastlogterm(lastLogTerm);

​            std::shared_ptr<mprrpc::RequestVoteReply> requestVoteReply = std::make_shared<mprrpc::RequestVoteReply>();

​            std::thread t(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply,

​                          votedNum); 

​            t.detach();

​        }

​    }

}





带注释版：

void Raft::doElection() {

​    lock_guard<mutex> g(m_mtx); //c11新特性，使用raii避免死锁

​    if (m_status != Leader) {

​        DPrintf("[       ticker-func-rf(%d)              ]  选举定时器到期且不是leader，开始选举 \n", m_me);

​        //当选举的时候定时器超时就必须重新选举，不然没有选票就会一直卡住

​        //重竞选超时，term也会增加的

​        m_status = Candidate;

​        ///开始新一轮的选举

​        m_currentTerm += 1;  //无论是刚开始竞选，还是超时重新竞选，term都要增加

​        m_votedFor = m_me; //即是自己给自己投票，也避免candidate给同辈的candidate投

​        persist();   

​        std::shared_ptr<int> votedNum = std::make_shared<int>(1); // 使用 make_shared 函数初始化 !! 亮点

​        //	重新设置定时器

​        m_lastResetElectionTime = now();

​        //	发布RequestVote RPC

​        for (int i = 0; i < m_peers.size(); i++) {

​            if (i == m_me) {

​                continue;

​            }

​            int lastLogIndex = -1, lastLogTerm = -1;

​            getLastLogIndexAndTerm(&lastLogIndex, &lastLogTerm);//获取最后一个log的term和下标，以添加到RPC的发送

​            //初始化发送参数

​            std::shared_ptr<mprrpc::RequestVoteArgs> requestVoteArgs = std::make_shared<mprrpc::RequestVoteArgs>();

​            requestVoteArgs->set_term(m_currentTerm);

​            requestVoteArgs->set_candidateid(m_me);

​            requestVoteArgs->set_lastlogindex(lastLogIndex);

​            requestVoteArgs->set_lastlogterm(lastLogTerm);

​            std::shared_ptr<mprrpc::RequestVoteReply> requestVoteReply = std::make_shared<mprrpc::RequestVoteReply>();

​            //使用匿名函数执行避免其拿到锁

​            std::thread t(&Raft::sendRequestVote, this, i, requestVoteArgs, requestVoteReply,

​                          votedNum); // 创建新线程并执行函数，并传递参数

​            t.detach();

​        }

​    }

}



#### **sendRequestVote**：

无注释版：

bool Raft::sendRequestVote(int server, std::shared_ptr<mprrpc::RequestVoteArgs> args, std::shared_ptr<mprrpc::RequestVoteReply> reply,

​                           std::shared_ptr<int> votedNum) {

​    auto start = now();

​    bool ok = m_peers[server]->RequestVote(args.get(),reply.get());

​    if (!ok) {

​        return ok;

​    }

​    lock_guard<mutex> lg(m_mtx);

​    if(reply->term() > m_currentTerm){

​        m_status = Follower; //三变：身份，term，和投票

​        m_currentTerm = reply->term();

​        m_votedFor = -1;

​        persist();

​        return true;

​    } else if ( reply->term()   < m_currentTerm   ) {

​        return true;

​    }

​    if(!reply->votegranted()){

​        return true;

​    }

​    *votedNum = *votedNum + 1;

​    if (*votedNum >=  m_peers.size()/2+1) {

​        *votedNum = 0;

​        m_status = Leader;

​        int lastLogIndex =   getLastLogIndex();

​        for (int i = 0; i <m_nextIndex.size()  ; i++) {

​            m_nextIndex[i] = lastLogIndex + 1 ;

​            m_matchIndex[i] = 0;     

​        }

​        std::thread t(&Raft::doHeartBeat, this); 

​        t.detach();

​        persist();

​    }

​    return true;

}

带注释版：

bool Raft::sendRequestVote(int server, std::shared_ptr<mprrpc::RequestVoteArgs> args, std::shared_ptr<mprrpc::RequestVoteReply> reply,

​                           std::shared_ptr<int> votedNum) {

​    bool ok = m_peers[server]->RequestVote(args.get(),reply.get());

​    if (!ok) {

​        return ok;//rpc通信失败就立即返回，避免资源消耗

​    }

​    lock_guard<mutex> lg(m_mtx);

​    if(reply->term() > m_currentTerm){

​        //回复的term比自己大，说明自己落后了，那么就更新自己的状态并且退出

​        m_status = Follower; //三变：身份，term，和投票

​        m_currentTerm = reply->term();

​        m_votedFor = -1;  //term更新了，那么这个term自己肯定没投过票，为-1

​        persist(); //持久化

​        return true;

​    } else if ( reply->term()   < m_currentTerm   ) {

​        //回复的term比自己的term小，不应该出现这种情况

​        return true;

​    }

​    if(!reply->votegranted()){  //这个节点因为某些原因没给自己投票，没啥好说的，结束本函数

​        return true;

​    }

  //给自己投票了

​    *votedNum = *votedNum + 1; //voteNum多一个

​    if (*votedNum >=  m_peers.size()/2+1) {

​        //变成leader

​        *votedNum = 0;   //重置voteDNum，如果不重置，那么就会变成leader很多次，是没有必要的，甚至是错误的！！！

​        //	第一次变成leader，初始化状态和nextIndex、matchIndex

​        m_status = Leader;

​        int lastLogIndex =   getLastLogIndex();

​        for (int i = 0; i <m_nextIndex.size()  ; i++) {

​            m_nextIndex[i] = lastLogIndex + 1 ;//有效下标从1开始，因此要+1

​            m_matchIndex[i] = 0;               //每换一个领导都是从0开始，见论文的fig2

​        }

​        std::thread t(&Raft::doHeartBeat, this); //马上向其他节点宣告自己就是leader

​        t.detach();

​        persist();  

​    }

​    return true;

}





只有leader才需要维护`m_nextIndex和m_matchIndex` 。



#### **RequestVote**：



无注释版：



void Raft::RequestVote( const mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply *reply) {

​    lock_guard<mutex> lg(m_mtx);

​    Defer ec1([this]() -> void { //应该先持久化，再撤销lock

​        this->persist();

​    });

​    

​    if (args->term() < m_currentTerm) {

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Expire);

​        reply->set_votegranted(false);

​        return;

​    }

​    if (args->term() > m_currentTerm) {

​        m_status = Follower;

​        m_currentTerm = args->term();

​        m_votedFor = -1;

​    }

​    int lastLogTerm = getLastLogIndex();

​    if (!UpToDate(args->lastlogindex(), args->lastlogterm())) {

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Voted);

​        reply->set_votegranted(false);

​        return;

​    }

​    if (m_votedFor != -1 && m_votedFor != args->candidateid()) {

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Voted);

​        reply->set_votegranted(false);

​        return;

​    } else {

​        m_votedFor = args->candidateid();

​        m_lastResetElectionTime = now();

​        

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Normal);

​        reply->set_votegranted(true);

​        return;

​    }

}





带注释版：



void Raft::RequestVote( const mprrpc::RequestVoteArgs *args, mprrpc::RequestVoteReply *reply) {

​    lock_guard<mutex> lg(m_mtx);

​    Defer ec1([this]() -> void { //应该先持久化，再撤销lock，因此这个写在lock后面

​        this->persist();

​    });

​    //对args的term的三种情况分别进行处理，大于小于等于自己的term都是不同的处理

​    //reason: 出现网络分区，该竞选者已经OutOfDate(过时）

​    if (args->term() < m_currentTerm) {

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Expire);

​        reply->set_votegranted(false);

​        return;

​    }

​    //论文fig2:右下角，如果任何时候rpc请求或者响应的term大于自己的term，更新term，并变成follower

​    if (args->term() > m_currentTerm) {

​        m_status = Follower;

​        m_currentTerm = args->term();

​        m_votedFor = -1;

​        //	重置定时器：收到leader的ae，开始选举，透出票

​        //这时候更新了term之后，votedFor也要置为-1

​    }

​    //	现在节点任期都是相同的(任期小的也已经更新到新的args的term了)

​    //	要检查log的term和index是不是匹配的了

​    int lastLogTerm = getLastLogIndex();

​    //只有没投票，且candidate的日志的新的程度 ≥ 接受者的日志新的程度 才会授票

​    if (!UpToDate(args->lastlogindex(), args->lastlogterm())) {

​        //日志太旧了

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Voted);

​        reply->set_votegranted(false);

​        return;

​    }

   

//    当因为网络质量不好导致的请求丢失重发就有可能！！！！

//    因此需要避免重复投票

​    if (m_votedFor != -1 && m_votedFor != args->candidateid()) {

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Voted);

​        reply->set_votegranted(false);

​        return;

​    } else {

​        //同意投票

​        m_votedFor = args->candidateid();

​        m_lastResetElectionTime = now();//认为必须要在投出票的时候才重置定时器，

​        reply->set_term(m_currentTerm);

​        reply->set_votestate(Normal);

​        reply->set_votegranted(true);

​        return;

​    }

}









### 日志复制|心跳





![img](https://article-images.zsxq.com/Fqql9dbJTAJS5EA6pby_AIIw7mVo)



> 可以从流程图看到，函数实现上我尽量将心跳日志复制的流程统一，方便理解和后期统一修改
>
> 理解AppendEntry 相关内容，snapshot的逻辑是类似的。



**leaderHearBeatTicker**:负责查看是否该发送心跳了，如果该发起就执行doHeartBeat。



**doHeartBeat**:实际发送心跳，判断到底是构造需要发送的rpc，并多线程调用sendRequestVote处理rpc及其相应。



**sendAppendEntries**:负责发送日志的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。



**leaderSendSnapShot**:负责发送快照的RPC，在发送完rpc后还需要负责接收并处理对端发送回来的响应。



**AppendEntries**:接收leader发来的日志请求，主要检验用于检查当前日志是否匹配并同步leader的日志到本机。



**InstallSnapshot**:接收leader发来的快照请求，同步快照到本机。







#### leaderHearBeatTicker:



无注释版：



void Raft::leaderHearBeatTicker() {

​    while (true) {

​        auto nowTime = now();

​        m_mtx.lock();

​        auto suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - nowTime;

​        m_mtx.unlock();

​        if (suitableSleepTime.count() < 1) {

​            suitableSleepTime = std::chrono::milliseconds(1);

​        }

​        std::this_thread::sleep_for(suitableSleepTime);

​        if ((m_lastResetHearBeatTime - nowTime).count() > 0) { 

​            continue;

​        }

​        doHeartBeat();

​    }

}



带注释版：

void Raft::leaderHearBeatTicker() {

​    while (true) {

​        auto nowTime = now();

​        m_mtx.lock();

​        auto suitableSleepTime = std::chrono::milliseconds(HeartBeatTimeout) + m_lastResetHearBeatTime - nowTime;

​        m_mtx.unlock();

​        if (suitableSleepTime.count() < 1) {

​            suitableSleepTime = std::chrono::milliseconds(1);

​        }

​        std::this_thread::sleep_for(suitableSleepTime);

​        if ((m_lastResetHearBeatTime - nowTime).count() > 0) { //说明睡眠的这段时间有重置定时器，那么就没有超时，再次睡眠

​            continue;

​        }

​        doHeartBeat();

​    }

}



其基本逻辑和选举定时器electionTimeOutTicker一模一样，不一样之处在于设置的休眠时间不同，这里是根据HeartBeatTimeout来设置，而`electionTimeOutTicker中是根据getRandomizedElectionTimeout()` 设置。



#### **doHeartBeat**：

> 这里目前逻辑写的不统一，发送快照leaderSendSnapShot和发送日志sendAppendEntries的rpc值的构造没有统一，且写在一坨。
>
> 可以抽离出来。目前先将就，关注主要逻辑。



无注释版：

void Raft::doHeartBeat() {

​    std::lock_guard<mutex> g(m_mtx);

​    if (m_status == Leader) {

​        auto appendNums = std::make_shared<int>(1); //正确返回的节点的数量

​        for (int i = 0; i < m_peers.size(); i++) {

​            if(i == m_me){

​                continue;

​            }

​            if (m_nextIndex[i] <= m_lastSnapshotIncludeIndex) {

​                std::thread t(&Raft::leaderSendSnapShot, this, i); 

​                t.detach();

​                continue;

​            }

​            int preLogIndex = -1;

​            int PrevLogTerm = -1;

​            getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);

​            std::shared_ptr<mprrpc::AppendEntriesArgs> appendEntriesArgs = std::make_shared<mprrpc::AppendEntriesArgs>();

​            appendEntriesArgs->set_term(m_currentTerm);

​            appendEntriesArgs->set_leaderid(m_me);

​            appendEntriesArgs->set_prevlogindex(preLogIndex);

​            appendEntriesArgs->set_prevlogterm(PrevLogTerm);

​            appendEntriesArgs->clear_entries();

​            appendEntriesArgs->set_leadercommit(m_commitIndex);

​            if (preLogIndex != m_lastSnapshotIncludeIndex) {

​                for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {

​                    mprrpc::LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();

​                    *sendEntryPtr = m_logs[j]; 

​                }

​            } else {

​                for (const auto& item: m_logs) {

​                    mprrpc::LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();

​                    *sendEntryPtr = item; 

​                }

​            }

​            int lastLogIndex = getLastLogIndex();

​            //构造返回值

​            const std::shared_ptr<mprrpc::AppendEntriesReply> appendEntriesReply = std::make_shared<mprrpc::AppendEntriesReply>();

​            appendEntriesReply->set_appstate(Disconnected);

​            std::thread t(&Raft::sendAppendEntries, this, i, appendEntriesArgs, appendEntriesReply,

​                          appendNums); 

​            t.detach();

​        }

​        m_lastResetHearBeatTime = now();

​    }

}



带注释版：

void Raft::doHeartBeat() {

​    std::lock_guard<mutex> g(m_mtx);

​    if (m_status == Leader) {

​        auto appendNums = std::make_shared<int>(1); //正确返回的节点的数量

​        //对Follower（除了自己外的所有节点发送AE）

​        for (int i = 0; i < m_peers.size(); i++) {

​            if(i == m_me){ //不对自己发送AE

​                continue;

​            }

​            //日志压缩加入后要判断是发送快照还是发送AE

​            if (m_nextIndex[i] <= m_lastSnapshotIncludeIndex) {

​				//改发送的日志已经被做成快照，必须发送快照了

​                std::thread t(&Raft::leaderSendSnapShot, this, i); 

​                t.detach();

​                continue;

​            }

​            //发送心跳，构造发送值

​            int preLogIndex = -1;

​            int PrevLogTerm = -1;

​            getPrevLogInfo(i, &preLogIndex, &PrevLogTerm);  //获取本次发送的一系列日志的上一条日志的信息，以判断是否匹配

​            std::shared_ptr<mprrpc::AppendEntriesArgs> appendEntriesArgs = std::make_shared<mprrpc::AppendEntriesArgs>();

​            appendEntriesArgs->set_term(m_currentTerm);

​            appendEntriesArgs->set_leaderid(m_me);

​            appendEntriesArgs->set_prevlogindex(preLogIndex);

​            appendEntriesArgs->set_prevlogterm(PrevLogTerm);

​            appendEntriesArgs->clear_entries();

​            appendEntriesArgs->set_leadercommit(m_commitIndex);

​            // 作用是携带上prelogIndex的下一条日志及其之后的所有日志

​            //leader对每个节点发送的日志长短不一，但是都保证从prevIndex发送直到最后

​            if (preLogIndex != m_lastSnapshotIncludeIndex) {

​                for (int j = getSlicesIndexFromLogIndex(preLogIndex) + 1; j < m_logs.size(); ++j) {

​                    mprrpc::LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();

​                    *sendEntryPtr = m_logs[j];  

​                }

​            } else {

​                for (const auto& item: m_logs) {

​                    mprrpc::LogEntry *sendEntryPtr = appendEntriesArgs->add_entries();

​                    *sendEntryPtr = item;  

​                }

​            }

​            int lastLogIndex = getLastLogIndex();

​            //初始化返回值

​            const std::shared_ptr<mprrpc::AppendEntriesReply> appendEntriesReply = std::make_shared<mprrpc::AppendEntriesReply>();

​            std::thread t(&Raft::sendAppendEntries, this, i, appendEntriesArgs, appendEntriesReply,

​                          appendNums); // 创建新线程并执行b函数，并传递参数

​            t.detach();

​        }

​        m_lastResetHearBeatTime = now(); //leader发送心跳，重置心跳时间，

​    }

}



与选举不同的是`m_lastResetHearBeatTime` 是一个固定的时间，而选举超时时间是一定范围内的随机值。



这个的具体原因是为了避免很多节点一起发起选举而导致一直选不出`leader` 的情况。



为何选择随机时间而不选择其他的解决冲突的方法具体可见raft论文。



#### **sendAppendEntries**

无注释版：

bool

Raft::sendAppendEntries(int server, std::shared_ptr<mprrpc::AppendEntriesArgs> args, std::shared_ptr<mprrpc::AppendEntriesReply> reply,

​                        std::shared_ptr<int> appendNums) {

​    bool ok = m_peers[server]->AppendEntries(args.get(), reply.get());

​    if (!ok) {

​        return ok;

​    }

   

​    if (reply->appstate() == Disconnected) {  

​        return ok;

​    }

​    lock_guard<mutex> lg1(m_mtx);

​    if(reply->term() > m_currentTerm){

​        m_status = Follower;

​        m_currentTerm = reply->term();

​        m_votedFor = -1;

​        return ok;

​    } else if (reply->term() < m_currentTerm) {

​        return ok;

​    }

​    if (m_status != Leader) { 

​        return ok;

​    }

​    if (!reply->success()){

​        if (reply->updatenextindex()  != -100) {

​            m_nextIndex[server] = reply->updatenextindex(); 

​        }

​    } else {

​        *appendNums = *appendNums +1;

​        m_matchIndex[server] = std::max(m_matchIndex[server],args->prevlogindex()+args->entries_size()   );

​        m_nextIndex[server] = m_matchIndex[server]+1;

​        int lastLogIndex = getLastLogIndex();

​        if (*appendNums >= 1+m_peers.size()/2) { 

​            *appendNums = 0;

​            if(args->entries_size() >0 && args->entries(args->entries_size()-1).logterm() == m_currentTerm){

​                m_commitIndex = std::max(m_commitIndex,args->prevlogindex() + args->entries_size());

​            }

​        }

​    }

​    return ok;

}



带注释版：



bool

Raft::sendAppendEntries(int server, std::shared_ptr<mprrpc::AppendEntriesArgs> args, std::shared_ptr<mprrpc::AppendEntriesReply> reply,

​                        std::shared_ptr<int> appendNums) {

​    // todo： paper中5.3节第一段末尾提到，如果append失败应该不断的retries ,直到这个log成功的被store

​    bool ok = m_peers[server]->AppendEntries(args.get(), reply.get());

​    if (!ok) {

​        return ok;

​    }

​    lock_guard<mutex> lg1(m_mtx);

​    //对reply进行处理

​    // 对于rpc通信，无论什么时候都要检查term

​    if(reply->term() > m_currentTerm){

​        m_status = Follower;

​        m_currentTerm = reply->term();

​        m_votedFor = -1;

​        return ok;

​    } else if (reply->term() < m_currentTerm) {//正常不会发生

​        return ok;

​    }

​    if (m_status != Leader) { //如果不是leader，那么就不要对返回的情况进行处理了

​        return ok;

​    }

​    //term相等

​    if (!reply->success()){

​        //日志不匹配，正常来说就是index要往前-1，既然能到这里，第一个日志（idnex = 1）发送后肯定是匹配的，因此不用考虑变成负数

​        //因为真正的环境不会知道是服务器宕机还是发生网络分区了

​        if (reply->updatenextindex()  != -100) {  //-100只是一个特殊标记而已，没有太具体的含义

​            // 优化日志匹配，让follower决定到底应该下一次从哪一个开始尝试发送

​            m_nextIndex[server] = reply->updatenextindex();  

​        }

​        //	如果感觉rf.nextIndex数组是冗余的，看下论文fig2，其实不是冗余的

​    } else {

​        *appendNums = *appendNums +1;   //到这里代表同意接收了本次心跳或者日志

​        

​        m_matchIndex[server] = std::max(m_matchIndex[server],args->prevlogindex()+args->entries_size()   );  //同意了日志，就更新对应的m_matchIndex和m_nextIndex

​        m_nextIndex[server] = m_matchIndex[server]+1;

​        int lastLogIndex = getLastLogIndex();

​        if (*appendNums >= 1 + m_peers.size()/2) { //可以commit了

​            //两种方法保证幂等性，1.赋值为0 	2.上面≥改为==

​            *appendNums = 0;  //置0

​            //日志的安全性保证！！！！！ leader只有在当前term有日志提交的时候才更新commitIndex，因为raft无法保证之前term的Index是否提交

​            //只有当前term有日志提交，之前term的log才可以被提交，只有这样才能保证“领导人完备性{当选领导人的节点拥有之前被提交的所有log，当然也可能有一些没有被提交的}”

​            //说白了就是只有当前term有日志提交才会提交

​            if(args->entries_size() >0 && args->entries(args->entries_size()-1).logterm() == m_currentTerm){

​            

​                m_commitIndex = std::max(m_commitIndex,args->prevlogindex() + args->entries_size());

​            }

​        }

​    }

​    return ok;

}





 m_nextIndex[server] = reply->updatenextindex(); 中涉及日志寻找匹配加速的优化



#### **AppendEntries**:



无注释版：



void Raft::AppendEntries1(const mprrpc:: AppendEntriesArgs *args,  mprrpc::AppendEntriesReply *reply) {

​    std::lock_guard<std::mutex> locker(m_mtx);

​    reply->set_appstate(AppNormal);

​    if (args->term() < m_currentTerm) {

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(-100); 

​         DPrintf("[func-AppendEntries-rf{%d}] 拒绝了 因为Leader{%d}的term{%v}< rf{%d}.term{%d}\n", m_me, args->leaderid(),args->term() , m_me, m_currentTerm) ;

​        return; 

​    }

​    Defer ec1([this]() -> void { this->persist(); });

​    if (args->term() > m_currentTerm) {

​        m_status = Follower;

​        m_currentTerm = args->term();

​        m_votedFor = -1; 

​    }

​    

​    m_status = Follower; 

​    m_lastResetElectionTime = now();  

​    //	那么就比较日志，日志有3种情况

​    if (args->prevlogindex() > getLastLogIndex()) {

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(getLastLogIndex() + 1);

​       

​        return;

​    } else if (args->prevlogindex() < m_lastSnapshotIncludeIndex) { 

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1); 

​    }

​    if (matchLog(args->prevlogindex(), args->prevlogterm())) {

​        for (int i = 0; i < args->entries_size(); i++) {

​            auto log = args->entries(i);

​            if (log.logindex() > getLastLogIndex()) { 

​                m_logs.push_back(log);

​            } else { 

​                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() == log.logterm() &&

​                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command() != log.command()

​                        ) { 

​                    myAssert(false,

​                             format("[func-AppendEntries-rf{%d}] 两节点logIndex{%d}和term{%d}相同，但是其command{%d:%d}    {%d:%d}却不同！！\n",

​                                    m_me, log.logindex(), log.logterm(), m_me,

​                                    m_logs[getSlicesIndexFromLogIndex(log.logindex())].command(), args->leaderid(),

​                                    log.command()));

​                }

​                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) { //不匹配就更新

​                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log;

​                }

​            }

​        }

​        myAssert(getLastLogIndex() >= args->prevlogindex() + args->entries_size(),

​                 format("[func-AppendEntries1-rf{%d}]rf.getLastLogIndex(){%d} != args.PrevLogIndex{%d}+len(args.Entries){%d}",

​                        m_me, getLastLogIndex(), args->prevlogindex(), args->entries_size()));

​        if (args->leadercommit() > m_commitIndex) {

​            m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());

​        }

​        myAssert(getLastLogIndex() >= m_commitIndex,

​                 format("[func-AppendEntries1-rf{%d}]  rf.getLastLogIndex{%d} < rf.commitIndex{%d}", m_me,

​                        getLastLogIndex(), m_commitIndex));

​        reply->set_success(true);

​        reply->set_term(m_currentTerm);

​        return;

​    } else {

​        reply->set_updatenextindex(args->prevlogindex());

​        for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {

​            if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {

​                reply->set_updatenextindex(index + 1);

​                break;

​            }

​        }

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        return;

​    }

}





带注释版：



void Raft::AppendEntries1(const mprrpc:: AppendEntriesArgs *args,  mprrpc::AppendEntriesReply *reply) {

​    std::lock_guard<std::mutex> locker(m_mtx);

//	不同的人收到AppendEntries的反应是不同的，要注意无论什么时候收到rpc请求和响应都要检查term

​    if (args->term() < m_currentTerm) {

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(-100); // 论文中：让领导人可以及时更新自己

​         DPrintf("[func-AppendEntries-rf{%d}] 拒绝了 因为Leader{%d}的term{%v}< rf{%d}.term{%d}\n", m_me, args->leaderid(),args->term() , m_me, m_currentTerm) ;

​        return; // 注意从过期的领导人收到消息不要重设定时器

​    }

​    Defer ec1([this]() -> void { this->persist(); });//由于这个局部变量创建在锁之后，因此执行persist的时候应该也是拿到锁的.    //本质上就是使用raii的思想让persist()函数执行完之后再执行

​    if (args->term() > m_currentTerm) {

​        // 三变 ,防止遗漏，无论什么时候都是三变

​        m_status = Follower;

​        m_currentTerm = args->term();

​        m_votedFor = -1; // 这里设置成-1有意义，如果突然宕机然后上线理论上是可以投票的

​        // 这里可不返回，应该改成让改节点尝试接收日志

​        // 如果是领导人和candidate突然转到Follower好像也不用其他操作

​        // 如果本来就是Follower，那么其term变化，相当于“不言自明”的换了追随的对象，因为原来的leader的term更小，是不会再接收其消息了

​    }

​    // 如果发生网络分区，那么candidate可能会收到同一个term的leader的消息，要转变为Follower，为了和上面，因此直接写

​    m_status = Follower; // 这里是有必要的，因为如果candidate收到同一个term的leader的AE，需要变成follower

​    // term相等

​    m_lastResetElectionTime = now();    //重置选举超时定时器

​    // 不能无脑的从prevlogIndex开始阶段日志，因为rpc可能会延迟，导致发过来的log是很久之前的

​    //	那么就比较日志，日志有3种情况

​    if (args->prevlogindex() > getLastLogIndex()) {

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(getLastLogIndex() + 1);

​        return;

​    } else if (args->prevlogindex() < m_lastSnapshotIncludeIndex) { // 如果prevlogIndex还没有更上快照

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        reply->set_updatenextindex(m_lastSnapshotIncludeIndex + 1); 

​    }

​    //	本机日志有那么长，冲突(same index,different term),截断日志

​    // 注意：这里目前当args.PrevLogIndex == rf.lastSnapshotIncludeIndex与不等的时候要分开考虑，可以看看能不能优化这块

​    if (matchLog(args->prevlogindex(), args->prevlogterm())) {

​        //日志匹配，那么就复制日志

​        for (int i = 0; i < args->entries_size(); i++) {

​            auto log = args->entries(i);

​            if (log.logindex() > getLastLogIndex()) { //超过就直接添加日志

​                m_logs.push_back(log);

​            } else {  //没超过就比较是否匹配，不匹配再更新，而不是直接截断

​                if (m_logs[getSlicesIndexFromLogIndex(log.logindex())].logterm() != log.logterm()) { //不匹配就更新

​                    m_logs[getSlicesIndexFromLogIndex(log.logindex())] = log;

​                }

​            }

​        }

​        if (args->leadercommit() > m_commitIndex) {

​            m_commitIndex = std::min(args->leadercommit(), getLastLogIndex());// 这个地方不能无脑跟上getLastLogIndex()，因为可能存在args->leadercommit()落后于 getLastLogIndex()的情况

​        }

​        // 领导会一次发送完所有的日志

​        reply->set_success(true);

​        reply->set_term(m_currentTerm);

​        return;

​    } else {

​        // 不匹配，不匹配不是一个一个往前，而是有优化加速

​        // PrevLogIndex 长度合适，但是不匹配，因此往前寻找 矛盾的term的第一个元素

​        // 为什么该term的日志都是矛盾的呢？也不一定都是矛盾的，只是这么优化减少rpc而已

​        // ？什么时候term会矛盾呢？很多情况，比如leader接收了日志之后马上就崩溃等等

​        reply->set_updatenextindex(args->prevlogindex());

​        for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {

​            if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {

​                reply->set_updatenextindex(index + 1);

​                break;

​            }

​        }

​        reply->set_success(false);

​        reply->set_term(m_currentTerm);

​        return;

​    }

}









### 日志寻找匹配加速



这部分在`AppendEntries`  函数部分。



涉及代码：



// 不匹配，不匹配不是一个一个往前，而是有优化加速

// PrevLogIndex 长度合适，但是不匹配，因此往前寻找 矛盾的term的第一个元素

// 为什么该term的日志都是矛盾的呢？也不一定都是矛盾的，只是这么优化减少rpc而已

// ？什么时候term会矛盾呢？很多情况，比如leader接收了日志之后马上就崩溃等等

reply->set_updatenextindex(args->prevlogindex());

for (int index = args->prevlogindex(); index >= m_lastSnapshotIncludeIndex; --index) {

​    if (getLogTermFromLogIndex(index) != getLogTermFromLogIndex(args->prevlogindex())) {

​        reply->set_updatenextindex(index + 1);

​        break;

​    }

}

reply->set_success(false);

reply->set_term(m_currentTerm);

return;





前篇也说过，如果日志不匹配的话可以一个一个往前的倒退。但是这样的话可能会设计很多个rpc之后才能找到匹配的日志，那么就一次多倒退几个数。



倒退几个呢？这里认为如果某一个日志不匹配，那么这一个日志所在的term的所有日志大概率都不匹配，那么就倒退到 最后一个日志所在的term的最后那个命令。



### 其他



#### snapshot快照



##### 快照是什么？



当在Raft协议中的日志变得太大时，为了避免无限制地增长，系统可能会采取快照（snapshot）的方式来压缩日志。快照是系统状态的一种紧凑表示形式，包含在某个特定时间点的所有必要信息，以便在需要时能够还原整个系统状态。



如果你学习过redis，那么快照说白了就是rdb，而raft的日志可以看成是aof日志。rdb的目的只是为了崩溃恢复的加速，如果没有的话也不会影响系统的正确性，这也是为什么选择不详细讲解快照的原因，因为只是日志的压缩而已。



##### 何时创建快照？



 快照通常在日志达到一定大小时创建。这有助于限制日志的大小，防止无限制的增长。快照也可以在系统空闲时（没有新的日志条目被追加）创建。



##### 快照的传输



快照的传输主要涉及：kv数据库与raft节点之间；不同raft节点之间。



kv数据库与raft节点之间：因为快照是数据库的压缩表示，因此需要由数据库打包快照，并交给raft节点。当快照生成之后，快照内设计的操作会被raft节点从日志中删除（不删除就相当于有两份数据，冗余了）。



不同raft节点之间：当leader已经把某个日志及其之前的内容变成了快照，那么当涉及这部的同步时，就只能通过快照来发送。



#### 内容补充



##### 对 m_nextIndex 和 m_matchIndex作用的补充：



`m_nextIndex` 保存leader下一次应该从哪一个日志开始发送给follower；m_matchIndex表示follower在哪一个日志是已经匹配了的（由于日志安全性，某一个日志匹配，那么这个日志及其之前的日志都是匹配的）



一个比较容易弄错的问题是：`m_nextIndex` 与`m_matchIndex` 是否有冗余，即使用一个`m_nextIndex` 可以吗？



显然是不行的，`m_nextIndex` 的作用是用来寻找`m_matchIndex` ，不能直接取代。我们可以从这两个变量的变化看，在当选leader后，`m_nextIndex` 初始化为最新日志index，`m_matchIndex` 初始化为0，如果日志不匹配，那么`m_nextIndex` 就会不断的缩减，直到遇到匹配的日志，这时候`m_nextIndex` 应该一直为`m_matchIndex+1` 。



如果一直不发生故障，那么后期m_nextIndex就没有太大作用了，但是raft考虑需要考虑故障的情况，因此需要使用两个变量。



#### 你可以尝试思考的问题



1. 锁，能否在其中的某个地方提前放锁，或者使用多把锁来尝试提升性能？
2. 多线程发送，能不能直接在doHeartBeat或者doElection函数里面直接一个一个发送消息呢？



#### 可以有的优化空间：



1. 线程池，而不是每次rpc都不断地创建新线程
2. 日志
3. 从节点读取日志



#### 后期内容预告！



1. 剩余辅助函数的逻辑。
2. 持久化：raft哪些变量需要持久化
3. rpc：如何实现一个简单的rpc通信



哦吼！差点忘记了，大家都喜欢show me the code（文本阅读源码太无聊），俺们的代码仓库在[这里](https://github.com/youngyangyang04/KVstorageBaseRaft-cpp)，目前已经可以尝试一个rpc的简单运行了，运行方式在首页的README中。



走过路过不要忘记点一个star。



非常欢迎大家：对代码提出issue问题；尝试优化优化；添加新功能；捉虫。



下一章开始我们应该就可以尝试运行起来raft了，敬请期待。



由于近期比较忙，更新稍晚，抱歉。