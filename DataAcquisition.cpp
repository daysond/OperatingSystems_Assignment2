


#include "DataAcquisition.h"

using namespace std;

DataAcquisition* DataAcquisition::instance=nullptr;

static void signalHandler(int signum) {
    switch(signum) {
        case SIGINT:
            //TODO: SHUTDOWN
            cout << "DataAcquisition shutting down..." << endl;
            DataAcquisition::instance->shutdown();
            break;
    }
}

/*****************************
 ********** Public  **********
 *****************************/

DataAcquisition::DataAcquisition() {
    // lock_x = PTHREAD_MUTEX_INITIALIZER;
    is_running=false;
    ShmPTR=nullptr;
    DataAcquisition::instance = this;
}

void DataAcquisition::run() {

    setupSignalHandler();
    setupSharedMemory();
    setupSocket();
    createThreads();
    readMemory(); //NOTE: readMemory is blocking with a while loop

    // pthread_join(rd_tid, NULL);
    // pthread_join(wr_tid, NULL);

    shutdown();

    cout << "[DEBUG] Data Acquisition Unit exiting... " << endl; 

}

void DataAcquisition::shutdown() {
 
    is_running = false;
    // TODO: CLOSE SOCKETS
    close(sv_sock);
    // TODO: CLOSE SEM
    sem_close(sem_id1);
    sem_unlink(SEMNAME);
    // TODO: CLOSE SHM
    shmdt((void *)ShmPTR);
    shmctl(ShmID, IPC_RMID, NULL);

}


/*****************************
 ******* Private Setups ******
 *****************************/

void DataAcquisition::setupSharedMemory() {
    // MARK: SHARED MEMORY
    ShmKey = ftok(MEMNAME, 65);
    ShmID = shmget(ShmKey, sizeof(struct SeismicMemory), IPC_CREAT | 0666); //rw-rw-rw
    if(ShmID < 0) {
        cout<<"Seismic[ShmID] Error: "<<strerror(errno)<<endl;
        exit(-1);
    }

    ShmPTR = (struct SeismicMemory*) shmat(ShmID, NULL, 0);
    if (ShmPTR == (void *)-1) {
        cout<<"Seismic[ShmPTR] Error: "<<strerror(errno)<<endl;
        exit(-1);
    }

    // MARK: SEMAPHORE
    // TODO: 1 OR 1? 
    sem_id1 = sem_open(SEMNAME, O_CREAT, SEM_PERMS, 0);

    if(sem_id1 == SEM_FAILED) {
        cout<<"Seismic[ShmPTR] Error: "<<strerror(errno)<<endl;
        exit(-1);
    }

    is_running = true;

}

void DataAcquisition::setupSignalHandler() {

    struct sigaction action;
    action.sa_handler = signalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
}

void DataAcquisition::setupSocket() {
// MARK: SERVER SOCKET SET UP
    const char LOCALHOST[] = "127.0.0.1";
    const int PORT = 1153;
    struct sockaddr_in sv_addr;
    sv_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (sv_sock == -1) {
        cout<<"Scoket creation failed. Error: "<<strerror(errno)<<endl;
        exit(-1);
    }

    memset(&sv_addr, 0, sizeof(sv_addr));
    sv_addr.sin_family = AF_INET;
    sv_addr.sin_port = htons(PORT);
    check(inet_pton(AF_INET, LOCALHOST, &sv_addr.sin_addr));
 
    check(bind(sv_sock, (struct sockaddr*)&sv_addr, sizeof(sv_addr)));


}

void DataAcquisition::createThreads() {
    // MARK: THREADS SET UP W/ MUTEX
    pthread_mutex_init(&lock_x, NULL);
    
    check(pthread_create(&rd_tid, NULL, &DataAcquisition::recv_func, this));
    check(pthread_create(&wr_tid, NULL, &DataAcquisition::send_func, this));
    
}

void DataAcquisition::readMemory() {

    int dataIdx = 0;
    cout << "[DEBUG] Starting to read shared memory... " << endl; 
    while(is_running) {
        if(ShmPTR->seismicData[dataIdx].status == WRITTEN) {
//            DataPacket *dp = new DataPacket;
            struct DataPacket packet;
            sem_wait(sem_id1);
            packet.data = string(ShmPTR->seismicData[dataIdx].data);
            packet.packetLen = ShmPTR->seismicData[dataIdx].packetLen;
            packet.packetNo = uint8_t(ShmPTR->packetNo);
            
            ShmPTR->seismicData[dataIdx].status = READ;
            sem_post(sem_id1);

            pthread_mutex_lock(&lock_x);
            packetQueue.push(packet);
            pthread_mutex_unlock(&lock_x);

            ++dataIdx;
            if(dataIdx>NUM_DATA) dataIdx=0;
        }

        sleep(1);
    }
}


/*****************************
 ******* Thread related ******
 *****************************/

void* DataAcquisition::recv_func(void *arg) {
 
    cout << "[DEBUG] Receive thread started... " << endl; 
    DataAcquisition* instance = (DataAcquisition*)arg;
    int ret = 0;
    struct sockaddr_in cl_addr;  // client addr
    socklen_t cl_addr_len = sizeof(cl_addr);
    char IP_addr[INET_ADDRSTRLEN];
    int port;
    char buf[BUF_LEN];
    memset(buf, 0, BUF_LEN);

    while (instance->is_running) {

        memset(&cl_addr, 0, sizeof(cl_addr));
        ret = recvfrom(instance->sv_sock, buf, BUF_LEN, 0, (struct sockaddr*) &cl_addr, &cl_addr_len);
    
       if (ret > 0){
            memset(IP_addr, 0, INET_ADDRSTRLEN);
            inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);
            port = ntohs(cl_addr.sin_port);
            string key = string(IP_addr) + ":" + to_string(port);

            if (instance->black_list.find(key) == instance->black_list.end()) {
                instance->authenticate(buf, &cl_addr, instance->sv_sock); 
            }
            memset(&buf, 0, BUF_LEN);
  
        } 
    }
    cout << "[DEBUG] Receive thread exiting... " << endl; 
    pthread_exit(NULL);
    

}

void DataAcquisition::authenticate(char cl_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock) {
 
    const char password[] = "Leaf";
    const char action_sub[] = "Subscribe";
    const char action_cnl[] = "Cancel";
    const char reply[] = "Subscribed";

    const int CMD_IDX = 0;
    const int USRNAME_IDX = 1;
    const int PSW_IDX = 2;

    // Extrating client address and port 
    char IP_addr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(cl_addr->sin_addr), IP_addr, INET_ADDRSTRLEN);
    int port = ntohs(cl_addr->sin_port);
    if(port == 0 || IP_addr == "0.0.0.0") return;

    string key = string(IP_addr) + ":" + to_string(port);

    
    // -------------------- Extrating CSV Message into command, username and password --------------------
    const int MAX_MSG = 3;
    // idx: 0 - command, 1 - username, 2 - password,  "Subscribe",<username>,"Leaf" or "Cancel",<username>
    char extractedMsgs[MAX_MSG][BUF2_LEN]; 
    int idx = 0;
    char * token = strtok(cl_msg, ",");
    while(idx < MAX_MSG ) {

        memset(&extractedMsgs[idx], 0, BUF2_LEN);

        if(token == NULL) memcpy(&extractedMsgs[idx], "", BUF2_LEN);
        else memcpy(&extractedMsgs[idx], token, BUF2_LEN);
        
        token = strtok(NULL, ",");
        idx++;
    }
    // ----------------------------------------------------------------------------------------------------

    // initalizing Subcriber Object 
    Subscriber sub;
    memcpy(&sub.username, &extractedMsgs[USRNAME_IDX], BUF2_LEN);
    memcpy(&sub.IP_addr, &IP_addr, INET_ADDRSTRLEN);
    sub.port = port;
    
    // Subscriber wants to cancel subscription
    if(strcmp(extractedMsgs[CMD_IDX], action_cnl) == 0) {
        pthread_mutex_lock(&lock_x);
        subscribers.erase(key);
        pthread_mutex_unlock(&lock_x);
        cout << "***** [" << sub.username <<"] " << key << " has unsubscribed. *****" << endl;
    }

    // Subscriber wants to subscribe
    if( (strcmp(extractedMsgs[CMD_IDX], action_sub)==0) && (strcmp(extractedMsgs[PSW_IDX], password) == 0)) {

        if (grey_list.find(key) == grey_list.end()) {

            pthread_mutex_lock(&lock_x);
            subscribers[key] = sub;
            pthread_mutex_unlock(&lock_x);

            cout << "***** [" << extractedMsgs[USRNAME_IDX] <<"] has subscribed! *****" << endl;

            sendto(sv_sock, reply ,sizeof(reply), 0,(struct sockaddr *)cl_addr, sizeof(*cl_addr));

        } 
        else cout << "[" << extractedMsgs[USRNAME_IDX] <<"] has already subscribed." << endl;
        
    }

    // Rogue Data Center: denial service attack / try to guess password
    if((strcmp(extractedMsgs[CMD_IDX], action_sub) != 0) && strcmp(extractedMsgs[CMD_IDX], action_cnl) != 0) {
        // invalid command
        cout << "DataAcquisition: unknown command " << extractedMsgs[CMD_IDX] << endl;
        AddToGreyList(key, sub);

    } else if(((strcmp(extractedMsgs[PSW_IDX], password) != 0) && (strcmp(extractedMsgs[CMD_IDX], action_cnl) !=0))) 
        AddToGreyList(key, sub); // guess password
        
}

void DataAcquisition::AddToGreyList(string key, Subscriber &sub) {
    // addes data center to the grey list by key
    grey_list[key] = (grey_list.find(key) != grey_list.end()) ? grey_list[key]+1 : 1;

    if(grey_list[key] >= 3){
        black_list[key] = sub;
        cout << "***** [Blocked] " << sub.username << " " << key  << endl;
        pthread_mutex_lock(&lock_x);
        subscribers.erase(key);
        pthread_mutex_unlock(&lock_x);
    }
}

void* DataAcquisition::send_func(void *arg) {
    /*
    The data acquisition unit will have a write thread to write seismic data to all subscribed data centers.
    The data acquisition unit will use mutexing to synchronize the data received from the transducer
     (push to a queue) with the data written to the data centers (front/pop from the queue).
     The data center expects the following information: first byte: packet number, second byte: length of the data, remaining bytes: the data.
    */

    DataAcquisition* instance = (DataAcquisition*)arg;
    cout << "[DEBUG] Send thread started... " << endl; 
    struct sockaddr_in cl_addr;
    DataPacket *packet;
    // std::map<std::string, Subscriber> subscribers;
    while(instance->is_running) {
   
        if (!instance->packetQueue.empty()) {
            cout << "DataPacket.size(): " << instance->packetQueue.size() << " Num Clients: " << instance->subscribers.size() <<  endl;
            packet = &(instance->packetQueue.front());

            //NOTE: making a local copy of the subscribers since sendto could block (no mutexing)
            pthread_mutex_lock(&(instance->lock_x));
            subscribers =  instance->subscribers;
            pthread_mutex_unlock(&(instance->lock_x));

            for (auto it = instance->subscribers.begin(); it != instance->subscribers.end(); it++) {
  
                memset(&cl_addr, 0, sizeof(cl_addr));
                cl_addr.sin_family = AF_INET;
                cl_addr.sin_port = htons(it->second.port);
                instance->check(inet_pton(AF_INET, it->second.IP_addr, &cl_addr.sin_addr));
               
                // ------------------ Verifying IP and PORT --------------------
                char IP_addr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);

                cout << "Sending Packet to: " << IP_addr << ":" << ntohs(cl_addr.sin_port) << endl;
                //--------------------------------------------------------------

                char buf[BUF_LEN+3];
                buf[0] = packet->packetNo & 0xFF;
                buf[1] = (packet->packetLen >> 8) & 0XFF;
                buf[2] = packet->packetLen & 0xFF;
                memcpy(buf + 3, packet->data.c_str(), packet->packetLen);

                sendto(instance->sv_sock, (char *)buf, BUF_LEN+3, 0,(struct sockaddr *)&cl_addr, sizeof(cl_addr));
          
            }
       
            pthread_mutex_lock(&instance->lock_x);
            instance->packetQueue.pop();
            pthread_mutex_unlock(&instance->lock_x);
        }
        
        sleep(1);
    }

    cout << "[DEBUG] Send thread exiting... " << endl; 
    pthread_exit(NULL);
}

/*****************************
 ****** Helper Function ******
 *****************************/

void DataAcquisition::check(int ret){
    /*
     This function checks for error. It takes a returned status and a socket fd.
     If there's an error, it closes the socket and exits with returned status.
    */
    if (ret < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return;
        else{
            cerr << strerror(errno) << endl;
            shutdown();
            exit(ret);
        }    
    }
}