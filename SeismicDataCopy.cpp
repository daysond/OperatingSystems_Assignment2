




// int main(int argc, char const *argv[])
// {

//    // MARK: SHARED MEMORY
//     // sem_t *sem_id1;
//     // key_t  ShmKey;
//     // int    ShmID;
//     // struct SeismicMemory *ShmPTR;
    


//     // MARK: SEMAPHORE
//     // TODO: 1 OR 1? 
//     // sem_id1 = sem_open(SEMNAME, O_CREAT, SEM_PERMS, 0);
//     // is_running = true;

//     //MARK: SIGNAL HANDLER
//     // struct sigaction action;
//     // action.sa_handler = signalHandler;
//     // sigemptyset(&action.sa_mask);
//     // action.sa_flags = 0;
//     // sigaction(SIGINT, &action, NULL);


//     // MARK: SERVER SOCKET SET UP
//     // const char LOCALHOST[] = "127.0.0.1";
//     // const int PORT = 1153;
//     // struct sockaddr_in sv_addr;
//     // int sv_sock = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);

//     // // TODO: ADDR SET UP 
//     // memset(&sv_addr, 0, sizeof(sv_addr));
//     // sv_addr.sin_family = AF_INET;
//     // sv_addr.sin_port = htons(PORT);
//     // int ret = inet_pton(AF_INET, LOCALHOST, &sv_addr.sin_addr);

//     // // TODO: BIND SERVER TO ADDR
//     // bind(sv_sock, (struct sockaddr*)&sv_addr, sizeof(sv_addr));


//     // // MARK: THREADS SET UP W/ MUTEX
//     // pthread_mutex_init(&lock_x, NULL);
//     // pthread_t rd_tid, wr_tid;
//     // // TODO: READ & WRITE THREAD
//     // //  TODO: ERROR CHECK
//     // pthread_create(&rd_tid, NULL, recv_func, &sv_sock);
//     // pthread_create(&wr_tid, NULL, send_func, &sv_sock);
    

//     // MARK: READING DATA
//     int dataIdx = 0;
//     while(is_running) {
//         if(ShmPTR->seismicData[dataIdx].status == WRITTEN) {
// //            DataPacket *dp = new DataPacket;
//             struct DataPacket packet;
//             sem_wait(sem_id1);
//             packet.data = string(ShmPTR->seismicData[dataIdx].data);
//             packet.packetLen = ShmPTR->seismicData[dataIdx].packetLen;
//             packet.packetNo = uint8_t(ShmPTR->packetNo);
//             // cout << "# " << ShmPTR->packetNo  << " or " << unsigned(packet.packetNo) << " len " << ShmPTR->seismicData[dataIdx].packetLen << " or " << packet.packetLen   << " data " <<ShmPTR->seismicData[dataIdx].data <<"\n\n" <<endl;
            
//             ShmPTR->seismicData[dataIdx].status = READ;
//             sem_post(sem_id1);
//             pthread_mutex_lock(&lock_x);
//             packetQueue.push(packet);
//             pthread_mutex_unlock(&lock_x);
//             ++dataIdx;
//             if(dataIdx>NUM_DATA) dataIdx=0;
//         }
//         // if (packetQueue.empty() == 0) {
            
//         //     cout << "# " << packetQueue.front().packetNo << " len " << packetQueue.front().packetLen << " data " << packetQueue.front().data <<"\n\n" <<endl;
//         //     packetQueue.pop();
//         //  }
//         // cout << "Packets: " << packetQueue.size()<< endl;
//         sleep(1);
//     }

//     // MARK: CLEAN UP
//     // TODO: JOIN THREADS
//     pthread_join(rd_tid, NULL);
//     pthread_join(wr_tid, NULL);
//     // TODO: CLOSE SOCKETS
//     close(sv_sock);
//     // TODO: CLOSE SEM
//     sem_close(sem_id1);
//     sem_unlink(SEMNAME);
//     // TODO: CLOSE SHM
//     shmdt((void *)ShmPTR);
//     shmctl(ShmID, IPC_RMID, NULL);

//     return 0;
// }



// void* recv_func(void *arg) {
//     // The data acquisition unit will have a read thread for authenticating data centers.
//     int sv_sock = *(int *)arg;
//     int ret = 0;
//     struct sockaddr_in cl_addr;  // client addr
   
//     socklen_t cl_addr_len = sizeof(cl_addr);
//     char buf[BUF_LEN];
//     memset(&buf, 0, BUF_LEN);
//     cout << "thread running" << endl;
//     while (is_running) {
//         memset(&cl_addr, 0, sizeof(cl_addr));
//         ret = recvfrom(sv_sock, buf, BUF_LEN, 0, (struct sockaddr*) &cl_addr, &cl_addr_len);

//        if (ret > 0){
//             //REVIEW: REPEATED CODE
//             pthread_mutex_lock(&lock_x);
//             char IP_addr[INET_ADDRSTRLEN];
//             inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);
//             int port = ntohs(cl_addr.sin_port);
//             string key = string(IP_addr) + ":" + to_string(port);


//             if (black_list.find(key) == black_list.end()) {
//                 // cout<< "\n\n" << "[GOT]"<< buf  << " from " << key << endl;
//                 authenticate(buf, &cl_addr, sv_sock); 
//             }
//             memset(&buf, 0, BUF_LEN);
//             pthread_mutex_unlock(&lock_x);
//         }
//     }

//     pthread_exit(NULL);
    

// }

// void authenticate(char auth_msg[BUF_LEN], struct sockaddr_in *cl_addr, int sv_sock) {
//     bool ret = false;

//     // TODO: PASS WORD
//     const char password[] = "Leaf";
//     const char action_sub[] = "Subscribe";
//     const char action_cnl[] = "Cancel";
//     const char reply[] = "Subscribed";
//     const int ACT_IDX = 0;
//     const int USRNAME_IDX = 1;
//     const int PSW_IDX = 2;
//     char IP_addr[INET_ADDRSTRLEN];
//     inet_ntop(AF_INET, &(cl_addr->sin_addr), IP_addr, INET_ADDRSTRLEN);
//     int port = ntohs(cl_addr->sin_port);
//     if(port == 0 || IP_addr == "0.0.0.0") return;
//     // cout << (IP_addr == nullptr) << endl;
//     // cout << IP_addr << endl;
//     // cout << port << endl;
    
//     const int MAX_MSG = 3;
//     char msg[MAX_MSG][BUF2_LEN]; // idx: 0 - action, 1 - username, 2 - password,  "Subscribe",<username>,"Leaf" or "Cancel",<username>
//     int idx = 0;
//     char * token = strtok(auth_msg, ",");
//     while(idx < MAX_MSG ) {
//         // printf(" %s\n", token ); //printing each token
//         memset(&msg[idx], 0, BUF2_LEN);

//         if(token == NULL) {
//             memcpy(&msg[idx], "", BUF2_LEN);
//         } else {
//             memcpy(&msg[idx], token, BUF2_LEN);
//         }
//         token = strtok(NULL, ",");
//         idx++;
//     }
//     // cout <<"[AUTH] " <<"username " << msg[USRNAME_IDX] << " action " << msg[ACT_IDX] << " password? " << msg[PSW_IDX] << endl;
    
//     string key = string(IP_addr) + ":" + to_string(port);

//     Subscriber sub;
//     memcpy(&sub.username, &msg[USRNAME_IDX], BUF2_LEN);
//     memcpy(&sub.IP_addr, &IP_addr, INET_ADDRSTRLEN);
//     sub.port = port;

//     if(strcmp(msg[ACT_IDX], action_cnl) == 0) {
//         // cancel subscription
//         subscribers.erase(key);
//         cout << "########### [" << key <<"] has unsubscribed." << endl;
//     }

//     if( (strcmp(msg[ACT_IDX], action_sub)==0) && (strcmp(msg[PSW_IDX], password) == 0)) {
//         // add to list of subscribers
//         if (grey_list.find(key) == grey_list.end()) {
//             subscribers[key] = sub;
//             cout << "[" << msg[USRNAME_IDX] <<"] has subscribed!" << endl;
//             //TODO: REPLY "Subscribed"
//             sendto(sv_sock, reply ,sizeof(reply), 0,(struct sockaddr *)cl_addr, sizeof(*cl_addr));
//         } else {
//             cout << "[" << msg[USRNAME_IDX] <<"] has already subscribed." << endl;
//         }
//     }

//     if((strcmp(msg[ACT_IDX], action_sub) != 0) && strcmp(msg[ACT_IDX], action_cnl) != 0) {
//         // invalid command
//         cout << "DataAcquisition: unknown command " << msg[ACT_IDX] << endl;
//         grey_list[key] = (grey_list.find(key) != grey_list.end()) ? grey_list[key]+1 : 1;
//         if(grey_list[key] >= 3){
//             cout << "Adding " << key << " to the rogue list for service denial attack" << endl;
//             black_list[key] = sub;
//             subscribers.erase(key);
//         }
//     } else if(((strcmp(msg[PSW_IDX], password) != 0) && (strcmp(msg[ACT_IDX], action_cnl) !=0))) {
//         // check if the data centers are guessing the password
//         grey_list[key] = (grey_list.find(key) != grey_list.end()) ? grey_list[key]+1 : 1;
//         if(grey_list[key] >= 3){
//             cout << "Adding " << key << " to the rogue list for trying to guess password" << endl;
//             black_list[key] = sub;
//             subscribers.erase(key);
//         }
//     }

// }

// void* send_func(void *arg) {
//     /*
//     The data acquisition unit will have a write thread to write seismic data to all subscribed data centers.
//     The data acquisition unit will use mutexing to synchronize the data received from the transducer
//      (push to a queue) with the data written to the data centers (front/pop from the queue).
//      The data center expects the following information: first byte: packet number, second byte: length of the data, remaining bytes: the data.
//     */

//    int sv_sock = *(int *)arg;
//    cout << "send func" << endl;

//     struct sockaddr_in cl_addr;
    
//     DataPacket *packet;
//     while(is_running) {
//         pthread_mutex_lock(&lock_x);
//         if (!packetQueue.empty()) {
//             cout << "DataPacket.size(): " << packetQueue.size() << " Num Clients: " << subscribers.size() <<  endl;
//             packet = &packetQueue.front();
//             for (auto it = subscribers.begin(); it != subscribers.end(); it++) {
//                 // std::cout << it->first << ": " << it->second.username << " " << it->second.IP_addr << ":" << it->second.port  << std::endl;
//                 // cout << "about to send " << endl;
//                 memset(&cl_addr, 0, sizeof(cl_addr));
//                 cl_addr.sin_family = AF_INET;
//                 cl_addr.sin_port = htons(it->second.port);
//                 int ret = inet_pton(AF_INET, it->second.IP_addr, &cl_addr.sin_addr);
//                 if(ret < 0) {
//                     cout << "send func inet_pton " << strerror(errno) << endl;
//                 }
//                 // TODO:------ verification --------
//                 char IP_addr[INET_ADDRSTRLEN];
//                 int port = ntohs(cl_addr.sin_port);
//                 inet_ntop(AF_INET, &(cl_addr.sin_addr), IP_addr, INET_ADDRSTRLEN);
//                 cout << "Sending Packet to: " << IP_addr << ":" << port << endl;
//                 //---------------------------
//                 char buf[BUF_LEN+3];
//                 buf[0] = packet->packetNo & 0xFF;
//                 buf[1] = (packet->packetLen >> 8) & 0XFF;
//                 buf[2] = packet->packetLen & 0xFF;
//                 memcpy(buf + 3, packet->data.c_str(), packet->packetLen);

//                 ret = sendto(sv_sock, (char *)buf, BUF_LEN+3, 0,(struct sockaddr *)&cl_addr, sizeof(cl_addr));
//                 if(ret < 0) {
//                     cout << "sendto " << strerror(errno) << endl;
//                 }
//             }
//             packetQueue.pop();
//         }
//         pthread_mutex_unlock(&lock_x);
//         sleep(1);
//     }

//     pthread_exit(NULL);
// }