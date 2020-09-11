#include <atomic>
#include <thread>
#include <random>
#include <fstream>
#include <unistd.h>
#include <sstream>
#include <assert.h>
#include <iostream>
#include <string.h>
#include <exception>
#include <arpa/inet.h>
#include <bits/stdc++.h>
#define DIFF_PORTS 10000
#include <ctime>
using namespace std;
typedef chrono::time_point<chrono::system_clock> Time;
static Time start; /*Time is typedef reference here*/

/*
    Following are some usefull global variable which are used in our program. Some of below variables are
    atomic because they can be used to synchronize memory accesses among different threads.
*/
atomic<int> noOfMessagesExchanged = ATOMIC_VAR_INIT(0);       /*Used for calculating total no.of messages exchanged*/
atomic<long long int> responseTime = ATOMIC_VAR_INIT(0);      /*Used for calculating total responsive time*/
int process_done = 0, n, n_processes;                         /*No.of processes*/
pthread_cond_t conditioned_thread = PTHREAD_COND_INITIALIZER; /*Related to threads*/
pthread_mutex_t locker = PTHREAD_MUTEX_INITIALIZER;           /*Related to mutex*/
atomic<int> noOfRequestMessages = ATOMIC_VAR_INIT(0);       /*Used for calculating total no.of messages requestMessages*/

atomic<int> noOfTokenMessages = ATOMIC_VAR_INIT(0);
/*
    alpha- Time spend outside the critical section is exponentiallly distributed with alpha
    beta - Time spend inside the critical section is exponentially distributed with beta
*/
float alpha = 0.65, beta = 0.15;
int noOfTimesME_CS = 1; /*No.of times a process can access the Critical Section*/

#include "Algorithm.cpp"

/*
    This function will take two arguments. one is string and other one is character. This function will
    split the string based on the character(delimiter) and returns the vector of strings.
*/
vector<string> split(string str, char delimiter)
{
    vector<string> internal;
    stringstream ss(str); // Turn the string into a stream.
    string tok;

    while (getline(ss, tok, delimiter)) /*Extracting the required part from the string by eliminating delimiter*/
    {
        internal.push_back(tok);
    }

    return internal; /*Vector of strings*/
}

/*
    This functions takes lamda as argument and calculated the expression mentioned in the function.
    and returns the double value. This value is used for sleep() in the program.
*/
double exp_rand(double lamda)
{
    double reqSleepTime;
    reqSleepTime = rand() / (double)RAND_MAX;
    reqSleepTime = -log(1 - reqSleepTime) / lamda;
    return reqSleepTime;
}

/*
  The below snippet of line shows the barrier synchronization of the process. We use this synchronization 
  because process after creating the socket, it has to wait untill all other processes to create their sockets.
  So that they can communicate with each other. If we don't use this type synchronization, then process will 
  send/receive messages to the processes which haven't created their sockets(No means of communication then). 
  So, we use this barrier synchronizations.
*/
void process_Synchronization_Barrier()
{
    pthread_mutex_lock(&locker); /*all the processes should bind to socket before they start communicating each other*/
    process_done++;              /*Each process increments it if they bind the socket*/

    if (process_done == n) /*If all the processes are done with binding of the sockets then we release the lock and wake up the threads*/
    {
        for (int i = 1; i <= process_done; i++)       /*Here waking up the threads*/
            pthread_cond_signal(&conditioned_thread); /*This concept is used from operating systems subject for synchronization*/
    }

    else /*If not all processes completed the binding then wait untill they do */
    {
        pthread_cond_wait(&conditioned_thread, &locker); /*Waiting */
    }

    pthread_mutex_unlock(&locker); /*Do this after all processes completes the binding of socekts*/
    process_done = 0;
}

/*
    This functions is important component of our program. We use this function to start our computation of 
    the algorithm. This algorithm takes 9 arugments as input to the function and starts the computation.
*/

void startingProcess(const int process_pid, int &inCS, int &hasToken, LLONG &logicalClock, map<int, RequestArrayNode> &reqArray,
                     vector<int> &neighbors, Token **sharedTokenPtrPtr, atomic<int> &finishedProcesses, mutex *lock)
{

    Time requestCSTime; //Time is typedef reference
    char *stringTime;   //Used to store corrent time repesentation hh:mm:ss

    /*
        The following snippet of code is responsible for making request for CS (critical section) , executing
        the CS and exit from the CS. This snippet of code loops the procedure for noOfTimesME_CS i.e 
        No.of times a process can access the Critical Section
    */
    for (int i = 1; i <= noOfTimesME_CS; i++)
    {
        double cs_outTime = exp_rand(alpha); /*Getting outside CS time*/
        double cs_inTime = exp_rand(beta);   /*Getting inside CS time*/
        stringTime = timeTaken();            /*Reading the corrent system time using timeTaken() function*/
        printf("Process %d: Local computation at %s\n", process_pid + 1, stringTime);
        usleep(cs_outTime * 10000); /*Sleeping with cs_outTime before we make request to CS*/

        stringTime = timeTaken(); /*Reading the corrent system time using timeTaken() function*/
        printf("Process %d requests to enter CS for the %d time at %s\n", process_pid + 1, i, stringTime);

        requestCSTime = chrono::system_clock::now(); /*Used for calculating response time*/
        
        /*Making request for CS using requestCS function*/
        requestCS(process_pid, inCS, hasToken, logicalClock, neighbors, lock);
        /*Calculating responseTime here*/
        responseTime += chrono::duration_cast<chrono::microseconds>(chrono::system_clock::now() - requestCSTime).count();

        stringTime = timeTaken();
        printf("Process %d enters its CS for the %d time at %s\n", process_pid + 1, i, stringTime);
        usleep(cs_inTime * 10000);
        printf("Process %d exits its CS for %d time at %s\n", process_pid + 1, i, stringTime);
        /*Process exits from its CS*/
        exitCS(process_pid, inCS, hasToken, logicalClock, reqArray, sharedTokenPtrPtr, lock);
    }

    stringTime = timeTaken();
    printf("Process %d completed all %d times accessing its CS at %s\n", process_pid + 1, noOfTimesME_CS, stringTime);

    /*
        For any process once it completes the CS access required no.of times, then we need to terminate the process
        and we need to transmit the token after completing CS. Once process finish all the CS accesses
        then it needs to send terminate message to other processes.  
    */
    TerminateMessage terminateMessage;       /*Structure of terminate message is defined in Algorithm.cpp*/
    terminateMessage.senderID = process_pid; /*Mentioning which process is sending terminate message*/
    terminateMessage.type = TERMINATE;       /*Message type*/

    /*Sending Terminate Message to other processes */
    for (int i = 0; i < n_processes; i++)
    {
        if (i != process_pid)
        {
            stringTime = timeTaken(); /*Reading the corrent system time using timeTaken() function*/
            printf("Process %d sends TERMINATE MESSAGE to Process-%d at %s\n", process_pid + 1, i + 1, stringTime);
            bool status = sendMessage(process_pid, i, terminateMessage); /*sent message*/
            if (!status)
            {
                printf("Process %d is unable to send TERMINATE MESSAGE to Process-%d\n", process_pid + 1, i + 1);
                exit(EXIT_FAILURE);
            }
        }
    }

    finishedProcesses++; /*After sending terminate message we increase this variable by one */

    /*
        If process is finished we need to wait until all processes completes its execution.

    */

    while (finishedProcesses < n_processes)
    {
        lock->lock();
        if (hasToken == TRUE) /*If the process has token then need to transmit it using exitCS() function*/
        {
            lock->unlock();
            exitCS(process_pid, inCS, hasToken, logicalClock, reqArray, sharedTokenPtrPtr, lock);
        }
        lock->unlock();
    }
}

/*
    The receive() function is used to receive the messages that were sent by anyprocess. Each process 
    maintains a thread to receive() function. This function takes 10 arguments as input.
    After receiving the message from socket , this function will distinguish the message type
    (REQUEST,TOKEN,TERMINATE) and perform actions based on the message type. 
*/
void receive(const int process_pid, const int port, int &inCS, int &hasToken, LLONG &logicalClock, map<int, RequestArrayNode> &reqArray, atomic<int> &finishedProcesses, vector<int> &neighbors, Token **sharedTokenPtrPtr, mutex *lock)
{
    /*
        Below code snippet is about socket connections and receiving message from socket
    */
    struct sockaddr_in client;                                         /*socket addresses for  client*/
    int portNumber = port + DIFF_PORTS;                                /*Port number*/
    socklen_t len = sizeof(struct sockaddr_in);                        /*Storing the size of client in len*/
    int maxRecvBufferLen = max(sizeof(RequestMessage), sizeof(Token)); /*Making max Buffere length*/
    char recvBuffer[maxRecvBufferLen];                                 /*Buffer declaration */
    unsigned int length;

    Token token;
    int clientSockfd;

    struct sockaddr_in server;                       /*socket addresses for  server*/
    server.sin_family = AF_INET;                     /*Using IPV4*/
    server.sin_addr.s_addr = inet_addr("127.0.0.1"); /*Local host address*/
    server.sin_port = htons(portNumber);             /*Port number */
    bzero(&server.sin_zero, 0);                      /*The bzero function places nbyte null bytes*/

    int sockfd;
    try
    {
        sockfd = socket(AF_INET, SOCK_STREAM, 0); /*socket() function to create socket and the returned discriptor is stored in the*/
        /*We use bind() function to bind the address and port number*/
        int s = bind(sockfd, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
        /*The listen() function marks a connection-mode socket */
        int r = listen(sockfd, MAX_NODES);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        /*The setsockopt function sets a socket option*/
        int c = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (sockfd < 0 || s < 0 || r < 0 || c < 0)
        {
            throw exception();
            exit(0);
        }
    }
    catch (exception e)
    {
        printf("Process %d failed due to  %s\n", process_pid + 1, strerror(errno));
        exit(EXIT_FAILURE);
    }
    while (true) /*Receiving message.....*/
    {
        if (finishedProcesses >= n_processes)
            break;

        if ((clientSockfd = accept(sockfd, (struct sockaddr *)&client, &len)) >= 0)
        {
            /*Receiving the message from client using recv function */
            int data_len = recv(clientSockfd, recvBuffer, maxRecvBufferLen, 0);

            if (data_len > 0) //Checking for data length received from client
            {
                /*Extracting message type from received message*/
                RequestMessage *requestMessage = reinterpret_cast<RequestMessage *>(recvBuffer);
                Token *tokenMessage = reinterpret_cast<Token *>(recvBuffer);
                TerminateMessage *terminateMessage = reinterpret_cast<TerminateMessage *>(recvBuffer);

                int message_type = tokenMessage->type;
                char *stringTime = timeTaken();

                switch (message_type) /*Checking message type*/
                {
                case TOKEN: /*If the message is token type*/
                    printf("Process %d receives TOKEN message from process %d at %s \n", process_pid + 1, tokenMessage->senderID, stringTime);
                    noOfMessagesExchanged++; /*no.of messages exchanged is incremented by 1*/
                    token = *tokenMessage;
                    noOfTokenMessages+=1;
                    lock->lock();
                    token.senderID = process_pid; /*Making sender Id to process id*/
                    *sharedTokenPtrPtr = &token;  /*to share token with sender thread*/

                    /*Received token so calling receiveTOken function to handle TOKEN messages*/
                    receiveToken(process_pid, inCS, hasToken, reqArray, sharedTokenPtrPtr);
                    lock->unlock();
                    break;

                case REQUEST: /*If the message is REQUEST type*/
                    printf("Process %d receives REQUEST message from process %d at %s \n", process_pid + 1, tokenMessage->senderID, stringTime);
                    noOfMessagesExchanged++; /*no.of messages exchanged is incremented by 1*/
                    noOfRequestMessages+=1;
                    lock->lock();
                    /*We call receiveRequest() function to handle REQUEST messages*/
                    receiveRequest(process_pid, inCS, hasToken, logicalClock, reqArray, requestMessage, neighbors, sharedTokenPtrPtr);
                    lock->unlock();
                    break;

                case TERMINATE: /*If the message type is TOKEN*/
                    printf("Process %d receives TERMINATE message from process %d at %s\n", process_pid + 1, tokenMessage->senderID, stringTime);
                    finishedProcesses++; /*We increse finishedProcesses by 1 in this process*/
                    break;

                default:
                    printf(" Process %d gets invalid message\n", process_pid + 1);
                }
            }
            close(clientSockfd);
        }
    }
}

/*
    We run our program by giving a file path to main program.
    From the file we read the input and store them in appropriate variables.

*/
int main(int argc, char *argv[])
{

    /*Required variables declaration*/
    fstream newfile;
    string tp;
    vector<string> sep;
    std::vector<int> neighbours;
    char filepath[100];
    strcpy(filepath, argv[1]);

    srand(time(NULL));
    int initialTokenNode;
    noOfTimesME_CS = 2; /*Can be changed*/
    /*
        Below peice of code will read the text file and gets information needed for the computation.
        such as no.of processes, Adjacency information etc.
    */
    newfile.open(filepath, ios::in);
    getline(newfile, tp);
    sep = split(tp, ' ');

    n_processes = stoi(sep[0]);
    initialTokenNode = n = stoi(sep[1]);

    /*2d-vector used to store the topology information of the processes */
    vector<vector<int>> topology(n_processes, vector<int>(0));
    int i = 0, l = 0, newline = 0;
    while (getline(newfile, tp))
    {
        sep = split(tp, ' ');
        int nodeID = stoi(sep[0]);
        for (int j = 1; j < sep.size(); j++)
            topology[nodeID].push_back(stoi(sep[j]));
    }

    double start_time,end_time;
    start_time=clock();
    start = chrono::system_clock::now(); /*Starting the time to calculate time taken by the algorithm*/

    /*
        We are declaring the datastructures that are needed for our compuation in this algorithm.
        1.startProcessThread is used to store thread for each process, which is going to be start point 
          of our computation
        2. receiveThreads - used to handle the receiving of messages for process
        3. inCS- maintains boolean value which shows that a process is in CS or not
        4. hasTOken-tells us whether the process has token or not
        5.logicalClock - represents the logical clock of a process used in request message (for time)
        6.reqArray - each process maintains the array which stores the requests for token. For each process
            there will be list for maintaining the requests from the processes.
        7.sharedTokenPtrPtr-
        8.sharedTOkenPtr-
        9.finishedProcesses- tells us about no.of processes finished till now
        10. locks- these are mutex used for synchronize the accesses and to execute the atomic instructions
        
    */
    vector<thread> startProcessThread(n_processes);
    vector<thread> receiveThreads(n_processes);
    vector<int> inCS(n_processes, FALSE);
    vector<int> hasToken(n_processes, FALSE);
    vector<LLONG> logicalClock(n_processes, 0);
    vector<map<int, RequestArrayNode>> reqArray(n_processes); //RequestArrayNode is list of RequestId structures
    vector<Token **> sharedTokenPtrPtr(n_processes, NULL);
    vector<Token *> sharedTokenPtr(n_processes, NULL);
    vector<atomic<int>> finishedProcesses(n_processes); //atomic- can be used to synchronize memory accesses among different threads
    vector<mutex> locks(n_processes);
    hasToken[initialTokenNode] = TRUE; /*Initial node has token*/

    /*Initializing the variables appropriately*/
    for (int i = 0; i < n_processes; i++)
    {
        sharedTokenPtrPtr[i] = &sharedTokenPtr[i];
        finishedProcesses[i] = ATOMIC_VAR_INIT(0);

        for (int &nbr : topology[i])
        {
            reqArray[i][nbr] = list<RequestID>(0); //Request ID structure for neighbor at a process i
        }
    }

    Token first_token_holder;                       /*Initial token holder process details*/
    first_token_holder.senderID = initialTokenNode; /*Inital token node*/
    first_token_holder.type = messageType::TOKEN;
    first_token_holder.elecID = initialTokenNode;

    for (int i = 0; i < MAX_NODES; i++)
    {
        first_token_holder.lud[i] = -1;
    }

    sharedTokenPtr[initialTokenNode] = &first_token_holder;

    printf("Creating receiver threads for processes ...................\n");

    /*Creating receiver thread for process i*/
    for (int i = 0; i < n_processes; i++)
    {
        receiveThreads[i] = thread(receive, i, i,
                                   ref(inCS[i]), ref(hasToken[i]), ref(logicalClock[i]), ref(reqArray[i]),
                                   ref(finishedProcesses[i]), ref(topology[i]), sharedTokenPtrPtr[i],
                                   &locks[i]);
        printf("Receive thread is created for process %d\n", i + 1);
        // process_done++;
        // process_Synchronization_Barrier();
    }
    printf("Process %d holds the token initially\n",initialTokenNode);
    /*Creating thread for internal computation for process i*/
    for (int i = 0; i < n_processes; i++)
    {
        startProcessThread[i] = thread(startingProcess, i, ref(inCS[i]), ref(hasToken[i]), ref(logicalClock[i]), ref(reqArray[i]), ref(topology[i]), sharedTokenPtrPtr[i], ref(finishedProcesses[i]), &locks[i]);
    }

    /* Joining the threads so that main will wait until all processes complete their execution*/
    for (int i = 0; i < n_processes; i++)
    {
        startProcessThread[i].join();
        receiveThreads[i].join();
    }
    end_time=clock();/*End clock time */
    double total=(end_time-start_time)/double(CLOCKS_PER_SEC);
    
 
    
    cout<<endl;
    printf("\n******************************************************************************\n");
    cout << "\n\tTotal Messages Exchanged:  " << noOfMessagesExchanged << endl;
    cout<<"\tTotal time taken by the algorithm to complete(in seconds): "<<setprecision(4)<<total<<endl;

    // cout<<total<<endl;
    printf("\n******************************************************************************\n");
    return 0;
}