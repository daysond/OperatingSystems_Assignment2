//
//
//
//

#ifndef DATAACQUISITION_H_
#define DATAACQUISITION_H_

#include "SeismicData.h"
#include <sys/shm.h>
#include <semaphore.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <string.h>
#include <queue>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/ip.h>     
#include <arpa/inet.h>  
#include <pthread.h>
#include <map>
#include <set>
#include <algorithm>

static void signalHandler(int signum);

class DataAcquisition {

    struct DataPacket {
        uint8_t packetNo;
        uint16_t packetLen;
        std::string data;
    };

    struct Subscriber {
        char username[BUF2_LEN];
        char IP_addr[INET_ADDRSTRLEN];
        int port;
    };


    bool is_running;
    pthread_mutex_t lock_x;
    std::queue<DataPacket> packetQueue;
    std::map<std::string, Subscriber> subscribers; // <string username, struct Subscriber>
    std::map<std::string, int> grey_list;
    std::map<std::string, Subscriber> black_list; 
    sem_t *sem_id1;
    key_t  ShmKey;
    int    ShmID, sv_sock;
    struct SeismicMemory *ShmPTR;
    pthread_t rd_tid, wr_tid;

   
    void authenticate(char cl_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock);
    void check(int);
    void AddToGreyList(std::string key, Subscriber &sub);
    void setupSharedMemory();
    void setupSignalHandler();
    void setupSocket();
    void createThreads();
    void readMemory();
    

    public:
     DataAcquisition();
    static DataAcquisition* instance;
    static void* recv_func(void *arg);
    static void* send_func(void *arg);
    void run();
    void shutdown();
};


#endif