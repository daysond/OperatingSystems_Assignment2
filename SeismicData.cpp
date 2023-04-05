//SeismicData.cpp - implementation for seismic data acquisition unit
//
//
// 31-Mar-23  Y. Dong         Created.
//
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
#include <vector>
#include<algorithm>

using namespace std;

struct DataPacket {
    unsigned int packetNo;
    unsigned short packetLen;
    char data[BUF_LEN];
};

struct Subscriber {
    char username[BUF2_LEN];
    char IP_addr[INET_ADDRSTRLEN];
    int port;
};



void signalHandler(int signum);
void* recv_func(void *arg);
void* send_func(void *arg);
void authenticate(char auth_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock);

bool is_running;
queue<DataPacket> packetQueue;
map<string, Subscriber> subscribers; // <string username, struct Subscriber>
map<string, int> grey_list;
vector<string> black_list;

int main(int argc, char const *argv[])
{

    /*
    // TODO: The shared memory:
    Before the data acquisition unit reads from shared memory, it has to ensure that the status of the packet in shared memory is set to WRITTEN.
    When the data acquisition unit reads the data, the status byte is set to READ.
    .
    The synchronization mechanism between the transducer and the data acquisition unit is through the status byte and semaphores.
    The data acquisition unit will take each packet of data from shared memory and push it on to a queue of a structure that contains the length of the packet, the packet number, and the seismic data. Call this structure DataPacket.
    After reading from shared memory, the data acquisition unit should sleep for 1 second.
    */
   // MARK: SHARED MEMORY
    sem_t *sem_id1;
    key_t  ShmKey;
    int    ShmID;
    struct SeismicMemory *ShmPTR;
    
    ShmKey = ftok(MEMNAME, 65);
    ShmID = shmget(ShmKey, sizeof(struct SeismicMemory), IPC_CREAT | 0666); //rw-rw-rw
    if(ShmID < 0) {
        cout<<"Seismic[ShmID] Error: "<<strerror(errno)<<endl;
        return -1;
    }

    ShmPTR = (struct SeismicMemory*) shmat(ShmID, NULL, 0);
    if (ShmPTR == (void *)-1) {
        cout<<"Seismic[ShmPTR] Error: "<<strerror(errno)<<endl;
        return -1;
    }

    // MARK: SEMAPHORE
    // TODO: 1 OR 1? 
    sem_id1 = sem_open(SEMNAME, O_CREAT, SEM_PERMS, 0);
    is_running = true;

    //MARK: SIGNAL HANDLER
    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);


    // MARK: SERVER SOCKET SET UP
    const char LOCALHOST[] = "127.0.0.1";
    const int PORT = 1153;
    struct sockaddr_in sv_addr;
    int sv_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

    // TODO: ADDR SET UP 
    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(PORT);
    int ret = inet_pton(AF_INET, LOCALHOST, &sv_addr.sin_addr);

    // TODO: BIND SERVER TO ADDR
    bind(sv_sock, (struct sockaddr*)&sv_addr, sizeof(sv_addr));


    // MARK: THREADS SET UP W/ MUTEX
    pthread_mutex_t lock_x = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_init(&lock_x, NULL);
    pthread_t rd_tid, wr_tid;
    // TODO: READ & WRITE THREAD
    //  TODO: ERROR CHECK
    pthread_create(&rd_tid, NULL, recv_func, &sv_sock);
    pthread_create(&wr_tid, NULL, send_func, &sv_sock);
    
    
    
    // MARK: CLEAN UP
    // TODO: JOIN THREADS
    pthread_join(rd_tid, NULL);
    pthread_join(wr_tid, NULL);

    // MARK: READING DATA
    int dataIdx = 0;
    while(is_running) {
        if(ShmPTR->seismicData[dataIdx].status == WRITTEN) {
//            DataPacket *dp = new DataPacket;
            struct DataPacket packet;
            sem_wait(sem_id1);
            memset(packet.data, 0, BUF_LEN);
            memcpy(packet.data, ShmPTR->seismicData[dataIdx].data, BUF_LEN);
            packet.packetLen = ShmPTR->seismicData[dataIdx].packetLen;
            packet.packetNo = ShmPTR->packetNo;
            ShmPTR->seismicData[dataIdx].status = READ;
            sem_post(sem_id1);
            packetQueue.push(packet);
            ++dataIdx;
            if(dataIdx>NUM_DATA) dataIdx=0;
        }
        sleep(1);
    }

    while (packetQueue.empty() == 0) {
        cout << packetQueue.front().data <<"\n\n\n" <<endl;
        packetQueue.pop();
    }
    // TODO: CLOSE SOCKETS

    // TODO: CLOSE SEM

    // TODO: CLOSE SHM

    
    
    return 0;
}

void signalHandler(int signum) {
    switch(signum) {
        case SIGINT:
            //TODO: SHUTDOWN
            cout << " shutting down" << endl;
            is_running = false;
            break;
    }
}

void* recv_func(void *arg) {
    // The data acquisition unit will have a read thread for authenticating data centers.
    int sv_sock = *(int *)arg;
    int ret = 0;
    struct sockaddr_in cl_addr;  // client addr
    memset(&cl_addr, 0, sizeof(cl_addr));
    socklen_t cl_addr_len = sizeof(cl_addr);
    char buf[BUF_LEN];
    memset(&buf, 0, BUF_LEN);
    cout << "thread running" << endl;
    while (is_running) {
        ret = recvfrom(sv_sock, buf, BUF_LEN, 0, (struct sockaddr*) &cl_addr, &cl_addr_len);
        if (ret < 0) {
            sleep(1);
        } else {
            //REVIEW: REPEATED CODE
            char IP_addr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);
            int port = ntohs(cl_addr.sin_port);
            string key = string(IP_addr) + ":" + to_string(port);
            // -------- repeated code -----------
            cout<< "\n\n" << "[GOT]"<< buf  << endl;
            if( find(black_list.begin(), black_list.end(), key) == black_list.end()){
                // cout<< "\n\n" << "[GOT]"<< buf  << endl;
                authenticate(buf, &cl_addr, sv_sock); 
            } 
            // else {
                // cout<< "dc " << key << " blocked." << endl;
            // }
            memset(&buf, 0, BUF_LEN);


        }
    }

    pthread_exit(NULL);
    
    //  For the read thread, for authentication the data center will send a CSV stream of data with the following format: "Subscribe",<username>,"Leaf" The password is "Leaf".
    //  On successful subscription, the data acquisition unit replies "Subscribed" and prints out a message that the data center has subscribed.
    //For the read thread, when a data center wishes to unsubscribe, it will send the following CSV stream of data: "Cancel",<username>
    //On unsubscribe, the data acquisition unit will remove the data center from the list of data centers and print out a message that the data center has unsubscribed.
    //The data acquisition unit therefore has to keep a list of subscribers. The data structure for a subscriber should have the subscriber's username, IP address and port.
    //For the read thread, the data acquisition unit has to handle denial of service attacts. This occurs when a rogue data center sends massive amounts of data to the data acquisition unit hoping to crash the unit.
    //For the read thread, the data acquisition unit has to handle a brute force attack by a rogue data center that wishes to guess the password.
    //Detecting a rogue data center can be made easy by tracking the IP address of the last three data centers to send data to the data acquisition unit. If the IP addresses are all the same, chances are a rogue data center is trying to guess the password or is trying to overwhelm the data acquisition unit.
    //The data acquisition unit should therefore keep a list of the IP addresses of all rogue data centers. The data structure for a rogue data center would be the same as for a valid data center.
    //Whenever data from a rogue center arrives, just throw it away.

}

void authenticate(char auth_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock) {
    bool ret = false;

    // TODO: PASS WORD
    const char password[] = "Leaf";
    const char action_sub[] = "Subscribe";
    const char action_cnl[] = "Cancel";
    const char reply[] = "Subscribed";
    const int ACT_IDX = 0;
    const int USRNAME_IDX = 1;
    const int PSW_IDX = 2;
    char IP_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(cl_addr->sin_addr), IP_addr, INET_ADDRSTRLEN);
    int port = ntohs(cl_addr->sin_port);
    if(port == 0 || IP_addr == "0.0.0.0") return;
    // cout << (IP_addr == nullptr) << endl;
    cout << IP_addr << endl;
    cout << port << endl;
    
    const int MAX_MSG = 3;
    char msg[MAX_MSG][BUF2_LEN]; // idx: 0 - action, 1 - username, 2 - password,  "Subscribe",<username>,"Leaf" or "Cancel",<username>
    int idx = 0;
    char * token = strtok(auth_msg, ",");
    while( token != NULL && idx < MAX_MSG ) {
        // printf(" %s\n", token ); //printing each token
        memset(&msg[idx], 0, BUF2_LEN);
        memcpy(&msg[idx], token, BUF2_LEN);
        token = strtok(NULL, ",");
        // cout << "token null? " << (token != NULL) << " and " << (idx < MAX_MSG) << " idx " << idx << endl;
        idx++;
    }
    cout << "username " << msg[USRNAME_IDX] << " action " << msg[ACT_IDX] << " password? " << msg[PSW_IDX] << endl;
    
    string key = string(IP_addr) + ":" + to_string(port);

    if(strcmp(msg[ACT_IDX], action_cnl) == 0) {
        // cancel subscription
        subscribers.erase(key);
        cout << "[" << key <<"] has unsubscribed." << endl;
    }

    if( (strcmp(msg[ACT_IDX], action_sub)==0) && (strcmp(msg[PSW_IDX], password) == 0)) {
        // add to list of subscribers
        Subscriber sub;
        strncpy(sub.username, msg[USRNAME_IDX], BUF2_LEN);
        strncpy(sub.IP_addr, IP_addr, INET_ADDRSTRLEN);
        sub.port = port;

        subscribers[key] = sub;
        cout << "[" << key <<"] has subscribed." << endl;
        //TODO: REPLY "Subscribed"
        sendto(sv_sock, reply ,sizeof(reply), 0,(struct sockaddr *)cl_addr, sizeof(*cl_addr));
        
    }

    if( ((strcmp(msg[ACT_IDX], action_sub) != 0 ) && strcmp(msg[ACT_IDX], action_cnl) != 0 )|| (strcmp(msg[PSW_IDX], password) != 0)) {
        // check if the data centers are guessing the password
        grey_list[key] = (grey_list.find(key) != grey_list.end()) ? grey_list[key]+1 : 1;

        cout << "[" << key <<"] has been added to greylist. num attemped: " << grey_list[key] <<endl;
        if(grey_list[key] > 3){
            black_list.push_back(key);
            cout << "[" << key <<"] has been blocked." << endl;
        } 
        
    }

}

void* send_func(void *arg) {
    /*
    The data acquisition unit will have a write thread to write seismic data to all subscribed data centers.
    The data acquisition unit will use mutexing to synchronize the data received from the transducer
     (push to a queue) with the data written to the data centers (front/pop from the queue).
     The data center expects the following information: first byte: packet number, second byte: length of the data, remaining bytes: the data.
    */
    pthread_exit(NULL);
}