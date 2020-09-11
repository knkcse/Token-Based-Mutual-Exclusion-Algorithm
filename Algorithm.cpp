#include <bits/stdc++.h>
#define MAX_NODES 60
#define MAX_LENGTH 500
#define FALSE 0
#define TRUE 1
using namespace std;

typedef long long int LLONG;

/* The Timeread() method is used to read the system time 
*/
time_t Timeread() /*This function is not taking any arguments. This function just reads the time*/
{
    struct timespec starts;                                 /* Variable to store the time value*/
    clock_gettime(CLOCK_MONOTONIC, &starts);                /* For obtaining time with much higher precision(nanoseconds) */
    return ((starts.tv_sec * 1e9) + starts.tv_nsec) * 1e-9; /* Time is read in the form of seconds and nanoseconds. So we are converting nano seconds into seconds and adding error_factor*driftFactor and we are returning this value*/
}

/*
	This function will read the time from the local system and then truncate the taken time to get
    required representation of the time(hh:mm:ss).This function will return stringTime

*/
char *timeTaken()
{
    // localclock local_clock;
    struct tm *time_information; /* Structure containing a calendar date and time broken down into its components*/
    time_t system_time;          /* time type variable, which stores the local_clock time from the tableValues*/
    time_information;            /*Converts to local time*/
    char *stringTime;            /*Convert tm structure to string*/
    system_time = Timeread();
    time(&system_time);
    time_information = localtime(&system_time);
    stringTime = asctime(time_information);
    stringTime[19] = '\0';
    stringTime = &stringTime[11];
    return stringTime;
}

/*
    The following few typedefs , struct and enums are the data structures that will be helpfull
    in our algorithm. 
*/
/*
    Enumerating messageType as TOKEN,REQUEST and TERMINATE. They will be used to identify the
    message type.
*/
enum messageType
{
    TOKEN = 1,
    REQUEST = 2,
    TERMINATE = 3
};

/*
    We need to REQUEST message has some feilds inside it so that our computation will be simpler.
*/
typedef struct RequestMessage
{
    enum messageType type; /*Message type*/
    int senderID;          /*Process which sent the request*/
    int reqOriginId;       /*Process id which sent original request */

    LLONG reqTime;                /*Lamport's logical clock value of the requesting time*/
    char alreadySeen[MAX_LENGTH]; /*The set of processes which have seen or about to see the request*/

} RequestMessage;

/*
    This typedef represents TOKEN message structure.
*/
typedef struct Token
{
    enum messageType type; /*Message type*/
    int senderID;          /*Process which sent the token*/

    int elecID; /*Process id the token's final destination process*/

    LLONG lud[MAX_NODES]; /* Array whose ith index stores the value that Process_i logical-clock had when
                             Process_i gave the token to another process.*/
} Token;

/*
    This typedef represents the TERMINATE MESSAGE structure
*/
typedef struct TerminateMessage
{
    enum messageType type; /*Message type*/
    int senderID;          /*Process id which sends the termination message*/
} TerminateMessage;

/*
    This typedef represents the request-id structure, which shows that the request origin ID and time
    at which the request is originated
 */
typedef struct RequestID
{
    int reqOriginId; /*Process id by which the request is created*/
    LLONG reqTime;   /*Lamport's time at which the request is done*/
} RequestID;

typedef list<RequestID> RequestArrayNode;

struct usageOfDatatypes
{
    LLONG minRequestTime = 0;
    int minRequestID = -1;
    int minNbrVal;
};

template <typename T>

/*
    The sendMessage() function takes 3 arguments as the inputs which is process id, destination process id
    and the message. The task of this function is to send the message to the destination process.
*/

bool sendMessage(const int process_pid, const int dstID, const T &message)
{

    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0); /*socket() function to create socket and the returned discriptor is stored in the*/
    if (sockfd < 0)
    {
        printf("Process %d gets error in creating send socket :: Reason - %s\n", process_pid + 1, strerror(errno));
        close(sockfd);
        return false;
    }

    struct sockaddr_in server;                       /*socket addresses for  recipient*/
    server.sin_family = AF_INET;                     /*Using IPV4*/
    server.sin_addr.s_addr = inet_addr("127.0.0.1"); /*Local host address*/
    server.sin_port = htons(DIFF_PORTS + dstID);     /*Port number */
    bzero(&server.sin_zero, 8);                      /*The bzero function places nbyte null bytes*/

    /*Connecting to recipient*/
    if (connect(sockfd, (struct sockaddr *)(&server), sizeof(struct sockaddr_in)) < 0)
    {
        printf("Process %d gets error in connecting to process %d.Reason - %s\n", process_pid + 1, dstID + 1, strerror(errno));
        close(sockfd);
        return false;
    }

    if (send(sockfd, (char *)(&message), sizeof(message), 0) < 0) /*Sending message*/
    {
        printf("Process %d gets error in sending message-type %s to process %d.Reason - %s\n", process_pid + 1, ((message.type == TOKEN) ? "TOKEN" : ((message.type == REQUEST) ? "REQUEST" : "TERMINATE")), dstID + 1, strerror(errno));
        close(sockfd);
        return false;
    }
    close(sockfd);
    return true;
}

/*
    This function takes 3 arguments as input and finds the neighbour process ID from request Array list
    and returns the neighbour id. It checks with all the requests in the reqArray and if any request with originated
    process id equals to elecId then it send that particular process id. If no such processes present
    then it returns -1 which means no such processes exists.
*/
int getNeighborIDfromReqArray(const int process_pid, const int elecID, map<int, RequestArrayNode> &reqArray)
{
    for (auto &nbr : reqArray) /*Loop through reqArray*/
    {
        for (auto it = nbr.second.begin(); it != nbr.second.end(); it++) /*Loop through all requests for reqArray element*/
        {
            if (it->reqOriginId == elecID)
            {
                return nbr.first;
            }
        }
    }

    printf("For process %d elecID not found in request array\n", process_pid + 1);
    return -1;
}

/*
    This function takes one character array as argument. This array contains proceses ids with '|' in between.
    So, this function will extract processes IDs from this array and make a vector with processes ids.
    Finally returns the vector containing processes ids, which are already seen or about to see the request msg
*/
vector<int> getAlreadySeenProcesses(char Q[])
{
    // puts(Q);
    vector<int> processes;
    char *sss = strtok(Q, "|"); /*Because we are adding this | to our alreadyseen process in construction of already seen function*/
    while (sss != NULL)
    {
        // puts(sss);
        processes.push_back(*sss - '0'); /*Getting int process id from character*/
        sss = strtok(NULL, "|");
    }

    return processes; /*Returning already seen processes Id vector*/
}

/*
    This function takes two arguments as input. One id process id and other input is vector of alreadyseen
    processes. If the process id 'n' present in the vector of already seen 'v' then this process returns true
    otherwise it returns false.
*/
bool isAlreadyInSeenList(int n, vector<int> v)
{
    for (int i = 0; i < v.size(); i++)
        if (n == v[i])
            return true;
    return false;
}

/*
This function takes two vectors of processes ids as input. One is neighboring processes and another one is
alreadySeen processes vector. The task of this function is to union these two vectors and return the resultant
processes set. This function uses isalreadyInSeenList() function to determine whether the process in 
neighbours vector is present in the alreadySeen vector. If it doesn't present then we add that process id to
alreadySeen list. 
*/
string unionSeenNeighbours(vector<int> alreadySeen, vector<int> &neighbors)
{

    vector<int> set;          /*To store already seen processes ids*/
    for (int i : alreadySeen) /*Simply pushing all processes in alreadySeen vector */
        set.push_back(i);

    for (int i : neighbors) /*For processes in neighbors vector*/
    {
        if (!isAlreadyInSeenList(i, set)) /*Checking whether the process is in alreadySeen list or not*/
            set.push_back(i);             /*Pushing the process id to set*/
    }

    /*
        Below snippet of code will make vector into string in which each process id is concated with '|'
        we return the resultant string 
    */
    string res = "";
    for (int nbr : set)
    {
        res += to_string(nbr) + "|"; /*Adding '|' after process id*/
    }
    return (res.size() > 0) ? res.substr(0, res.size() - 1) : res; /*remove the trailing '|' */
}

/*
    This function takes two arguments as input.1.Process id and 2. vector of processes which are neighbor to 
    process. This function will add '|' between processe ids and return the string containing list.
    This function is used in requestCS() function, while requesting for CS access.
*/

string constructAlreadySeenString(const int process_pid, vector<int> &neighbors)
{
    string s = "";
    for (const int i : neighbors)
    {
        s += to_string(i) + "|"; /*Inserting '|' between processes. This is for our convenience*/
    }
    s += to_string(process_pid);
    return s;
}

/*
    The transmitToken() functions takes 5 arguments as input which will be helpful to send/transmit the token
    to other processes. The variable sharedTokenPtrPtr is used to check whther the process has token to transmit
    or not.

*/

void transmitToken(const int process_pid, int &tokenHere, LLONG &logicalClock, map<int, RequestArrayNode> &reqArray,
                   Token **sharedTokenPtrPtr)
{
    if (*sharedTokenPtrPtr == NULL)
    {
        printf("Process %d has no token to transmit.\n", process_pid + 1);
        return;
    }

    /*
        If the process has token then the process has to extract the minimum/old request from request 
        array and send the token 
    */
    LLONG minRequestTime = 0;
    int minRequestID = -1;
    int minNbrVal;

    list<RequestID>::iterator minRequestNodeIterator;

    for (auto &i : reqArray) /*Loop through reqArray */
    {
        for (auto it = i.second.begin(); it != i.second.end(); it++) /**Loop through all requests for reqArray element*/
        {
            /*finding minimum request from totally ordered reqArray*/
            if ((*sharedTokenPtrPtr)->lud[it->reqOriginId] < 0 || (*sharedTokenPtrPtr)->lud[it->reqOriginId] < it->reqTime)
            {
                if (minRequestID == -1)
                {
                    minRequestTime = it->reqTime;
                    minRequestID = it->reqOriginId;
                    minNbrVal = i.first;
                    minRequestNodeIterator = it;
                }
                else
                {
                    /* As mentioned in the paper-- minimum :: (y,z) -> (y < y') || (y == y' && z < z')
                        y=request time
                        z=process id
                    */
                    if (it->reqTime < minRequestTime) /*It is based on request time i.e strictly lessthan*/
                    {
                        minRequestTime = it->reqTime;
                        minRequestID = it->reqOriginId;
                        minNbrVal = i.first;
                        minRequestNodeIterator = it;
                    }
                    else if (it->reqTime == minRequestTime) /*If request time is equal then we check for the process id*/
                    {
                        if (it->reqOriginId < minRequestID)
                        {
                            minRequestTime = it->reqTime;
                            minRequestID = it->reqOriginId;
                            minNbrVal = i.first;
                            minRequestNodeIterator = it;
                        }
                    }
                }
            }
        }
    }

    if (minRequestID != -1)
    {
        /*Remove the request from the reqArray*/
        reqArray[minNbrVal].erase(minRequestNodeIterator);

        /*Update the TOKEN feilds such as messa type , sender id, destination id and logical clock*/
        (*sharedTokenPtrPtr)->type = messageType::TOKEN;
        (*sharedTokenPtrPtr)->senderID = process_pid;
        (*sharedTokenPtrPtr)->elecID = minRequestID;
        (*sharedTokenPtrPtr)->lud[process_pid] = logicalClock;

        logicalClock++;
        tokenHere = false;

        char *stringTime = timeTaken();
        stringTime = timeTaken();
        printf("Process %d sends TOKEN to neighbor process %d at %s \n", process_pid + 1, minNbrVal + 1, stringTime);
        bool status = sendMessage(process_pid, minNbrVal, **sharedTokenPtrPtr); /*Token send*/
        if (status == false)
        {
            printf("Process %d is unable to send TOKEN to process %d\n", process_pid + 1, minNbrVal + 1);
        }

        // removing the token
        *sharedTokenPtrPtr = NULL;
    }
}

/*
    The receiveToken() function takes 5 arguements as input. This function task is that , if the destination
    address of the token is current process then it keeps the token. If not , then it finds the neighbour process
    id through which it can send the TOKEN to the destination process. 

    This function will be invoked in receive() function when the received message type is TOKEN, 
    which will be handled by receiving threads

*/
void receiveToken(const int process_pid, int &inCS, int &tokenHere, map<int, RequestArrayNode> &reqArray,
                  Token **sharedTokenPtrPtr)
{

    tokenHere = TRUE; /*Token is at the current process so makeing tokenHere to TRUE*/

    if ((*sharedTokenPtrPtr)->elecID == process_pid) /*The destination is current process */
    {
        inCS = TRUE; /*Current process isCS flag is TRUE now*/
    }
    else /*If TOKEN's destination address is not current process at which token arrived*/
    {

        /*Getting neighbour process id through which TOKEN has to be transmitted to desitnation process*/
        int nbrVal = getNeighborIDfromReqArray(process_pid, (*sharedTokenPtrPtr)->elecID, reqArray);
        if (nbrVal == -1)
        {
            return;
        }
        /*Update the TOKEN details */
        (*sharedTokenPtrPtr)->type = messageType::TOKEN;
        (*sharedTokenPtrPtr)->senderID = process_pid; /*Current process is sending the token to nbrVal process*/
        tokenHere = FALSE;                            /*Because current process is sending token to other process*/

        char *stringTime = timeTaken();
        printf("Process %d is sends TOKEN for process-%d to neighbor %d at %s\n", process_pid + 1, (*sharedTokenPtrPtr)->elecID + 1, nbrVal + 1, stringTime);
        if (sendMessage(process_pid, nbrVal, **sharedTokenPtrPtr) == false) /*Sending token*/
        {
            printf("Process %d is  unable to send TOKEN to its neighbor process %d\n", process_pid + 1, nbrVal + 1);
        }

        /*Removing the token from currentprocess sharedTokenPtrPtr*/
        *sharedTokenPtrPtr = NULL;
    }
}

/*
    The receiveRequest() function takes 8 arguments as input. This function is being invoked in receive()
    function when the received message type is REQUEST.

*/

void receiveRequest(const int process_pid, int &inCS, int &tokenHere, LLONG &logicalClock, map<int, RequestArrayNode> &reqArray, RequestMessage *request,
                    vector<int> &neighbors, Token **sharedTokenPtrPtr)
{
    bool isRequestAlreadyPresent = false;
    /*
        We need to check whether the reqArray contains old request or not. If present we delete that record.
        If the request is not present then we add it into the request array
    */
    for (auto &nbr : reqArray)
    {
        list<RequestID>::iterator it = nbr.second.begin();
        while (it != nbr.second.end())
        {
            if (it->reqOriginId == request->reqOriginId)
            {
                if (it->reqTime < request->reqTime)
                {
                    nbr.second.erase(it++); //Delete an old request
                    continue;
                }
                else
                {
                    isRequestAlreadyPresent = true;
                }
            }
            it++;
        }
    }

    if (!isRequestAlreadyPresent)
    {
        /* The request just received is a new one and is the youngest that the process ever
           received from requesting process
         */
        logicalClock = max(logicalClock, request->reqTime) + 1; /*Taking maximum as logical clock*/

        /*Inserting the new record into the array*/
        reqArray[request->senderID].push_back(RequestID{request->reqOriginId, request->reqTime});

        /*We need to get the list of processes which already have seen this request*/
        vector<int> alreadySeen = getAlreadySeenProcesses(request->alreadySeen);

        request->senderID = process_pid; /*Updating request sender id*/
        /*
            We need to find the REQUEST unseen neighbours of current process and we need to update the already
            seen set with neighbours of the process. The unionSeenNeighbours() function will make union of them
        */
        strcpy(request->alreadySeen, unionSeenNeighbours(alreadySeen, neighbors).c_str());

        /*
            Now, we need to send the request message to process's neighbours which havent seen the
            request message yet
        */
        for (const int nbr : neighbors) /*Loop through neighbour processes*/
        {
            /*Checking the neighbour process whether it has seen the REQUEST message of not.*/
            if (find(alreadySeen.begin(), alreadySeen.end(), nbr) == alreadySeen.end())
            {
                char *stringTime = timeTaken();
                printf("Process %d sends REQUEST message to process %d at %s\n", process_pid + 1, nbr + 1, stringTime);

                if (sendMessage(process_pid, nbr, *request) == false)
                {
                    printf("Process %d is unable to send REQUEST to process %d\n", process_pid, nbr);
                }
            }
        }

        /*
            If the current process has the token and it is not in the CS then we can transmit the token
            to other processes using transmitToken() function
        */
        if (tokenHere == TRUE && inCS == FALSE)
        {
            transmitToken(process_pid, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr);
        }
    }
}

/*
    This function is used to request for CS. This function takes 6 arguments as input and performs
    appropriate computations to make request for CS. This function creates REQUEST message and computes already
    seen set and sends the REQUEST message to other processes.

*/

void requestCS(const int process_pid, int &inCS, int &tokenHere, LLONG &logicalClock, vector<int> &neighbors, mutex *lock)
{
    lock->lock();

    if (tokenHere == TRUE) /*If process has token then it can access to its CS.*/
    {
        inCS = TRUE;
        lock->unlock();
    }
    else /*If process doesn't have token, then  REQUEST for TOKEN*/
    {
        RequestMessage message;
        message.reqTime = logicalClock; /*Taking logical time */
        lock->unlock();                 /*unlock early to avoid unnecessary blocking of receiver thread*/

        /*Making REQUEST message here */
        message.type = messageType::REQUEST;
        message.senderID = process_pid; /*Sender and originated id is same because both are same process only*/
        message.reqOriginId = process_pid;

        /*constructAlreadySeenString() function will give us alreadySeen processes for this request*/
        strcpy(message.alreadySeen, constructAlreadySeenString(process_pid, neighbors).c_str());

        /*Send the request to neighbour processes */
        for (const int nbr : neighbors)
        {

            char *stringTIme = timeTaken();
            printf("Process %d sends REQUEST MESSAGE to its neighbour process %d at %s\n", process_pid + 1, nbr + 1, stringTIme);
            if (sendMessage(process_pid, nbr, message) == false)
            {
                printf("Process %d is unable to send REQUEST to process %d\n", process_pid + 1, nbr + 1);
            }
        }
    }

    /*After sending the request message process has to wait for the token to enter the CS*/
    while (true)
    {
        lock->lock();
        if (inCS == TRUE)
        {
            lock->unlock();
            break;
        }
        lock->unlock();
    }
}

/*
    Process will exit the CS by calling exitCS() function. This function takes 6 arguments as input.
    This function will transmit the TOKEN to other process using transmitToken() function.
*/
void exitCS(const int process_pid, int &inCS, int &tokenHere, LLONG &logicalClock, map<int, RequestArrayNode> &reqArray,
            Token **sharedTokenPtrPtr, mutex *lock)
{
    lock->lock();
    inCS = false; /*After executing CS we need to make this false*/
    transmitToken(process_pid, tokenHere, logicalClock, reqArray, sharedTokenPtrPtr);
    lock->unlock();
}
