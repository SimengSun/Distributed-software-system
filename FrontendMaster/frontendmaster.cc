#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>
#include <netdb.h>
#include <netinet/in.h>
#include <vector>
#include <signal.h>
#include <unordered_map>
#include <dirent.h>
#include <chrono>
#include <sstream>
#include <ctime>
#include <arpa/inet.h>
#include <iomanip>
#include <queue>

#define POOL_SIZE 100
#define BUFFER_SIZE 2048
#define QUEUE_SIZE 10
#define PING_INTV 12

using namespace std;

// global variables
bool DEBUGGING = false;
static volatile int running = 1;
static volatile int numReplica = 2;

enum Modes {GTKY, PING, RECO, UNKN};

class BackendServer{
public:
  struct sockaddr_in bindAddr;
  string addr;
  bool active = true;
  int serverIdx;
  int numClient = 0;
  chrono::milliseconds ms;
  bool isPrimary = false;
  bool requireLog = false;
	int primaryIdx;
	vector<int> secondaries;

  BackendServer(string _addr, int _idx){
    addr = _addr;
    if (_idx % numReplica == 0) {
      isPrimary = true;
    }
    primaryIdx = _idx - _idx % numReplica;
    for (int i = 1 ; i < numReplica; i++){
      if (i+primaryIdx != serverIdx){
        secondaries.push_back(i+primaryIdx);
      }
    }
    if (DEBUGGING) {
      cout << "Server:" << _idx << " isP: " << isPrimary << " Pidx:"<< primaryIdx <<endl;
      cout << "Second:";
      for (auto sec: secondaries){
        cout<< sec<<" ";
      }
      cout<<endl;
    }
    // addr = _addr;
    active = false;
    serverIdx = _idx;

    int bindIdx;
		string bindIp, bindPort;
    bindIdx = _addr.find(":");
    bindIp = _addr.substr(0, bindIdx);
    bindIdx++;
    bindPort = _addr.substr(bindIdx, _addr.length() - bindIdx);

    bzero(&bindAddr, sizeof(bindAddr));
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(atoi(bindPort.c_str()));
    inet_pton(AF_INET, bindIp.c_str(), &(bindAddr.sin_addr));
    auto now = chrono::system_clock::now();
    ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) /1000;
  }

  bool isActive(){
    auto now = chrono::system_clock::now();
    bool result = (chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) /1000 ).count() - ms.count() < PING_INTV ;
    // if (DEBUGGING) cout << ms.count() << endl;
    return result;
  }

  void sendMsgToServer(string s, int cur_fd){
    sendMsgToServer(s.c_str(), cur_fd);
  }

  void sendMsgToServer(const char* s, int cur_fd){
    write(cur_fd, s, strlen(s));
  }

	string readFromServer(int cur_fd){
		char tempBuffer[4];
		int end = read(cur_fd, tempBuffer, 3);
		tempBuffer[3] = '\0';
		return string(tempBuffer);
	}
};



// tablet for big table and backend server
class Tablet{
public:
  string start;
  string end;
  BackendServer* primary;
  vector<BackendServer*> secondary;

  Tablet(string _start){
    start = _start;
  }

  void setPrimary(BackendServer* p){
    primary = p;
  }

  void addSecondary(BackendServer* s){
    secondary.push_back(s);
  }
};

class Master{
private:
  // unordered_map<int, BackendServer*> serverMap;

  unordered_map<string, Tablet*> tabletMap;
  int totalServer = 0;
  vector<string> tablets;

public:
  unordered_map<int, BackendServer*> serverMap;
  chrono::milliseconds ms;
  BackendServer* backendMaster;

  BackendServer* getServer(int key){
    auto s = serverMap.find(key);
    if (s!= serverMap.end()){
      return s->second;
    }
    return NULL;
  }

  void init(const char* filePath){
    initServer(filePath);
    initTablets();
    assignServer();
    auto now = chrono::system_clock::now();
    ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) /1000;
  }

  void initTablets(){
    tablets = {"a", "o"};
  }

  void assignServer(){
    int i = 0;
    for (auto t: tablets){
      Tablet* newTablet = new Tablet(t);
      auto iter = serverMap.find(i++);
       if (iter != serverMap.end()){
         newTablet->setPrimary(iter->second);
         tabletMap.insert(pair<string, Tablet*>(t, newTablet));
       }
    }
  }

  int findServer(string queryStr){
    if (queryStr.compare("o") < 0){
      return 0;
    } else {
      return 1;
    }
  }

  void initServer(const char* filePath){
    ifstream inputfile(filePath);
    string str;
    int i = 0;

    // skip master node address
    getline(inputfile, str);
    string bindAddr = str;
    backendMaster = new BackendServer(bindAddr, -1);

    while (getline(inputfile, str)){
      if (str.length()==0) continue;
      bindAddr = str;
      addNewServer(bindAddr, i++);
    }
  }

  void addNewServer(string bindAddr, int idx){
    totalServer++;
    BackendServer* newServer = new BackendServer(bindAddr, idx);
    serverMap.insert(pair<int, BackendServer*>(idx, newServer));
  }

  void terminate(){
    for (auto& s: serverMap){
      delete(s.second);
    }
  }
};

Master master;

class ThreadWorker{
public:
  pthread_t thread;

  // add constructor for vector initialization
  ThreadWorker(){}

  bool isAvailable(){
    return available;
  }

  void markUnavailable() {
    available = false;
  }

  void markAvailable(){
    available = true;
  }

  void setSocket(int fd){
    thread_fd = fd;
  }

  // start to process commands until quit command
  void startHandling(){
    //send(thread_fd, greetMsg, strlen(greetMsg), 0);
    if (DEBUGGING) fprintf(stderr, "[%d] New connection.\r\n", thread_fd);
    while (running) handleSingleRequest();
    markAvailable();
  }

  // termicate the thread
  void terminate(){
    //send(thread_fd, termMsg, strlen(termMsg), 0);
    // sendMsg(msg421);
    if (DEBUGGING) fprintf(stderr, "[%d], Connection closed\r\n", thread_fd);
    close(thread_fd);
  }

private:
  bool addedToRecip = false;
  vector<string> data;
  vector<string> forwards;
  vector<string> args;
  string sender;
  bool available = true;
  Modes mode = UNKN;
  int thread_fd;
  char cmd[11];
  char client_msg[BUFFER_SIZE+1];
  char command[BUFFER_SIZE];
  char *cm_start = client_msg;
  char *cm_end = client_msg;
  char *c_start = command;
  char *c_end = command;
  // const char* gtkyCmd = "/key";
  const char* pingCmd = "/ping";
  // const char* recoCmd = "/reco";
  // const char* edatCmd = ".";

  const char* unknownCmdMsg = "/unknown command\r\n";
  const char* badCmdMsg = "/unknown command arguments\r\n";

  // read command from the client side
  void readCommand(){
    //receive message from client
    int read_size = 0;
    while (true){
      if (!getLine()) {
        read_size = recv(thread_fd, client_msg, sizeof(char)*BUFFER_SIZE, 0);
        resetCM(read_size);
      } else {
        return;
      }
    }
  }

  bool getLine(){
    char* i = cm_start;
    while (i != cm_end) {
      *c_end++ = *i;
      if (c_end - c_start == BUFFER_SIZE) closeConn();
      if (*(c_end-1)=='\n' && c_end -1 != c_start && *(c_end-2)=='\r'){
        *c_end = '\0';
        cm_start = i+1;
        return true;
      }
      i++;
    }
    return false;
  }

  // extract command from the message
  void extractCommand(){
    strncpy(cmd, c_start, 10);
    cmd[10] = '\0';
    if (DEBUGGING) cout<<"CURRENT COMMAND IS: " << cmd;
    if (strncasecmp(cmd, pingCmd, 5) == 0) {
      mode = PING;
    } else {
      mode = UNKN;
    }
  }

  // process different commands
  void processCommand(){

    if (!handleSpecialOP()){
      switch (mode) {

        case (PING): {
          // accept mail from command
          handlePing();
          closeConn();
          break;
        }
        case (UNKN): {
          // accept rcpt to command
          handleGetKey();
          closeConn();
          break;
        }
        default: {
          fprintf(stderr, "Error in server!\r\n");
          break;
        }
      }
    }
    // reset the command buffer for next command

    resetC();
    resetMode();
  }



  // handle get key request from front end server
  void handleGetKey(){
    BackendServer* curServer = getFrontendServer();
    string response = "HTTP/1.1 301 Redirect\r\n";
    response += "Location:" + curServer->addr + "\r\n";
    sendMsg(response.c_str());
  }

  BackendServer* getFrontendServer(){
    int idx = 0;
    int curNum = 100;
    for (auto& s: master.serverMap){
      //s.second->isActive() &&
      if ( s.second->isActive() && s.second->numClient < curNum) {
        curNum = s.second->numClient;
        idx = s.second->serverIdx;
      }
    }
    return master.getServer(idx);
  }

  void getArgs(vector<string> &args){
		if (DEBUGGING) cout<< "getting args"<<endl;
		string argStr(c_start);
		int start = argStr.find(",") + 1;
		int end = argStr.find("\r\n") ;
		argStr = argStr.substr(start, end-start);
		parseArgs(args, argStr);
	}

  void parseArgs(vector<string> &args, string argStr){
		if (DEBUGGING) cout<< "getting args"<<endl;
		string delimiter = ",";
		string token;
		int pos = 0;
		while ((pos = argStr.find(delimiter)) != string::npos){
			token = argStr.substr(0, pos);
			args.push_back(token);
			argStr.erase(0, pos + delimiter.length());
		}
		args.push_back(argStr);
	}

  // handles ping from backend server
  void handlePing(){
    getArgs(args);
    int key = stoi(args[0]), numC = stoi(args[1]);

    BackendServer* curServer = master.getServer(key);
    // curServer->active = true;
    if (curServer != NULL) {
      auto now = chrono::system_clock::now();
      curServer->ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()) / 1000;
      curServer->numClient = numC;
      if (DEBUGGING) cout <<"Cur Server:" << key << " " << curServer->numClient <<endl;
    }

    // forward to admin console
    int temp_fd = socket(PF_INET, SOCK_STREAM, 0);
    connect(temp_fd, (struct sockaddr*)&master.backendMaster->bindAddr, sizeof(master.backendMaster->bindAddr));
    string fing = "/fing," + to_string(key) + "\r\n";
    master.backendMaster->sendMsgToServer(fing, temp_fd);
    // string response = curBS->readFromServer(temp_fd);
    // if (DEBUGGING) cout<< response.compare("+OK")<<endl;
    if (DEBUGGING) cout<< "Sent ping to backend master"<<endl;
    close(temp_fd);

    printActive();
    return;
  }



  void printActive(){
    cout<<"Active workers:"<<endl;
    for (auto& s : master.serverMap){
      if (s.second->isActive()) cout<< s.second->serverIdx<<" "<<s.second->numClient<<endl;;
    }
    cout<<endl;
  }


  // handle specical commands such as quit, reset and un-recongnized.
  bool handleSpecialOP(){
    return false;
  }

  void resetData(){
    data.clear();
  }

  // sendMsg to client
  void sendMsg(const char* msg){
    send(thread_fd, msg, strlen(msg), 0);
  }

  // close this socket and quit
  // needs update
  void closeConn(){
    args.clear();
    close(thread_fd);
    thread_fd = 0;
    reset();
    //if (DEBUGGING) fprintf(stderr, "Marking available\r\n");
    markAvailable();
    pthread_detach(pthread_self());
    pthread_exit(NULL);
  }

  // reset all buffers
  void reset(){
    resetCM(0);
    resetC();
    resetMode();
    resetData();
    // resetSMTP();
  }

  // reset client message buffer
  void resetCM(int len){
    cm_start = client_msg;
    cm_end = cm_start + len;
  }

  // reset command buffer
  void resetC(){
    c_start = command;
    c_end = c_start;
  }

  // reset command mode
  void resetMode(){
    mode = UNKN;
  }

  // handle one single request
  void handleSingleRequest(){
    readCommand();
    //if (DEBUGGING) fprintf(stderr, "[%d] C: %s", thread_fd, c_start);
    extractCommand();
    processCommand();
  }
};


// thread function
void *threadRun(void *thread){
  ThreadWorker* myThread = (ThreadWorker*)thread;
  myThread->startHandling();
}

class ThreadPool{
public:

  // constructor that initiates the pool according to the pool size
  ThreadPool(): threadpool(POOL_SIZE, ThreadWorker()){}

  // iterate through the pool to find an available thread
  ThreadWorker* pop(){
    for (auto& threadworker: threadpool){
      if (threadworker.isAvailable()){
        threadworker.markUnavailable();
        return &threadworker;
      }
    }
    return NULL;
  }

  // print availability for debugging
  void printAva(){
    for (auto& threadworker: threadpool){
      cout<< threadworker.isAvailable()<<" ";
    }
    cout<<endl;
  }

  // terminate all threads
  void terminate(){
    for (auto& threadworker: threadpool){
      if (!threadworker.isAvailable()) threadworker.terminate();
    }
  }

private:
  vector<ThreadWorker> threadpool;
};

// set outside to be accessed from the signal handler
int socket_fd;
ThreadPool threadpool;


// catch ctrl + c and terminate the server
void intHandler(int sigInt){
  running = 0;
  threadpool.terminate();
  master.terminate();
  close(socket_fd);
  exit(0);
}

int server(const char *server_port, const char* congfigFile) {

  // fields
  int child_fd;
  socklen_t client_len;
  struct sockaddr_in  client_addr;
  struct addrinfo hints, *res;

  // first, load up address structs with getaddrinfo():
  memset(&hints, 0, sizeof hints);
  // use IPv4 or IPv6
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, server_port, &hints, &res);

  // make a socket:
  socket_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (socket_fd == -1){
    perror("Unable to create socket");
    exit(1);
  }

  // bind socket to the local address and port number
  if ( bind(socket_fd, res->ai_addr, res->ai_addrlen)<0){
    perror("Unable to bind");
    exit(1);
  }

  // define how many clients to listen
  if (listen(socket_fd, QUEUE_SIZE) < 0){
    perror("Error in listen");
    exit(1);
  }
  client_len= sizeof(struct sockaddr_in);
  master.init(congfigFile);

  // infinite loop to accept requests
  while (1){
    child_fd= accept(socket_fd, (struct sockaddr *) &client_addr, &client_len);
    // if unable to connect, accept next client
    if (child_fd < 0) {
      //if (DEBUGGING) fprintf(stderr, "Unable to connect!\r\nDiscarding this connection...\r\n");
      continue;
    }

    ThreadWorker* tw = threadpool.pop();

    // discard a socket if no more thread available
    if (tw == NULL) {
      fprintf(stderr, "Out of available threads.\r\nDiscarding this connection...\r\n");
      close(child_fd);
      continue;
    }
    tw->setSocket(child_fd);
    pthread_create(&tw->thread, NULL, threadRun, tw);
  }
  return 0;
}

int main(int argc, char *argv[])
{
  int c = 0;
  int default_port = 10000;
  const char* server_port = NULL;
  const char* congfigFile = NULL;
  signal(SIGINT, intHandler);

  while (( c = getopt(argc, argv, "p:av")) !=-1 ){
    switch(c){
      case 'p':
        server_port = optarg;
        break;
      case 'v':
        DEBUGGING = true;
        break;
      case 'a':
        fprintf(stderr, "*** Author: Yecheng Yang (yecheng).\r\n");
        exit(0);
      case '?':
        fprintf(stderr, "Invalid arguments!\n");
        exit(1);
    }
  }

  if (optind != argc-1) {
    fprintf(stderr, "Missing backend server file\n");
    exit(1);
  }
  congfigFile = argv[optind];

  // set server_port if no input is given
  if (server_port == NULL) server_port = to_string(default_port).c_str();
  server(server_port, congfigFile);

  return 0;
}
