
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <dirent.h>
#include <ftw.h>



#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
//#define SEM_NAME "/Time"

void *createCardGameRoom(void *roomNumber);
void *waitingTimeOut();
void signalDetection(int sig);
static int start=0;
static int gameStart=0;
static int totalWaitingPlayer=0;


//static sem_t * sem;


int loginCheck(char read[] ,int bytes_received);
int findDirectory(char username[]);

void createDirectory(char username[]);
int createAccount(char username[],char password[]);
void deleteDirectory(int clientCode);
//int checkIdData(char username[]);
int checkLoginFile(char username[],char password[]);
//int checkUserInfo(char *receviedData ,int dataLength);
int saveUserInfo(char *receviedData ,int dataLength);

void storeClientInRoom(int roomNumber);
void initializePlayerSet();
void playerSetStore(int players[],int newcomer);
void playerSetDelete(int players[], int goodbye);



int checkClientPlayingCard(int client);
int clientIdxFinder(int clientCode);
void requestSolver(int clientCode,char message[]);
void readExtracter(char read[],int bytes_received);
int checkClientRequest(int clientCode);
//void storeRequestingClient(int specialRequestClient[],int client);
void storeUserinfo(char username[]);
void deleteUserinfo(int userNumber);
void storeUserPurpose(int userNumber,int purpose);


void storeWaitingQueue(int client);
int deleteWaitingQueue(int client);


static int connectedClient=0;

const static int MAX_CLIENT_HISTORY_DATA =10;
const static int MAX_CARD=15;
const static int playerlimit=4;
const static int userlimit=10;


static int waitingLine[10]; //sameas userlimit changed cuz of compile error
static int currentPlayers[10]={0};//default all 0
static int playerSets[4]={-1,-1,-1,-1}; // max player is 4 // -1 means no player

static int clientCode=0;
fd_set master;
fd_set masterCard;// this is for thread

typedef struct{
    int userNumber;
    char *username;
    int currnetRequest;
    char *title;
    //int playingStatus; // forgot about this and made currnetPlayers.. whatever..
    int room;
}userData;
static userData *currentUsers; 

/*
typedef struct{
    int playerList[4]; // receive playerSets[4]
    int roomNumber;
}cardgamePlayers;
static cardgamePlayers *playerSet;
*/
static int roomCount=0;

typedef struct{
    char cardType; // R (red) G (green) B (blue) Y (yellow)
    char cardStyle;// 1~9 j (jump) p (plus) d (direction) a (attk2) s (attk3) q (ultimate attk)
    int attack;// .0 .2 .3
    int ultimate; //.0 .1
    int used; // .-1 .0 .1 .2 // -1 means "this space does not have card" /2 means "card returned to cardDeck"
}card;


typedef struct{
    int userNumber;
    char *username;
    int cardCount;
    card *cardDeck;
    int warning; // if 3 >>  forced out
    int disquealified;
}userInfo;

typedef struct{
    int timeOut; // 0: waiting  / 1: time over  
    int roomNumber; //room number
    int clientNumber; // client number
}timeLimit_thread_data;

//void sendDeckTurn(char information[],card currentDeck , int turn);
void playerScreenMaker(char **playerScreen,userInfo *player,int playerCount,char commonScreen[]);
void currnetCardInDeck(char outputScreen[],card currentDeckCard);
void blindCard(char outputScreen[],userInfo playerInfo);
void printCard(char outputScreen[],userInfo playerInfo);
void cardToScreen(char outputScreen[],card currentCard);
void cardsLeftInDeck(char commonScreen[],char cardLeft[]);
int cardDeckCounter(card *cardDeck);


void readGameHistory(char *filename,int clientNumber);
void storeGameHistoryServer(int fileNumber,char *cardgameOneTurnData);
int createHistoryFileInServer();
int countHistoryFile(char *defaultPath);
void copyHistoryFromServerToClient(char *username,int serverDataNumber);
void screenMakerForServer(char *cardgameOneTurnData,userInfo *playerInfo,int playerCount);

void deleteCurrnetPlayer(int clientNumber);
void addSpecialOccastionMessage(char *playerScreen,int styleIdx,char *inputClient,int attkPoint);
int surviverCheck(userInfo *playerInfo,int playerCount);
void playerEraser(userInfo *playerInfo, int clientIdx,int playerCount,card *cardDeck);
int playerIdxFinder(userInfo *playerInfo, int client,int playerCount);
void initializeSpecialOccastionList(int *specialOccasion, int sizeofList);
void turnTableReverse(userInfo *playerInfo,int playerCount);
int changeTurn(userInfo *playerInfo,int currentTurn,int playerCount);
void takeCardFromClient(int cardPosition, userInfo *playerinfo, card *cardDeck );
int checkClientCardValid(char inputCard[], card currentDeck,int attkPoint);
int checkClientCardExist(char inputCard[], card *playerDeck );
void sendCardToClient(int cardCount,userInfo *playerInfo, card *cardDeck);
int checkCardCount(int cardCount, userInfo playerinfo);
void storePlayerRoomInfo(int players[], userInfo *playerinfo);
void shuffleCard(card* cardDeck,int reshuffle); // 1>> reshuffle 0>> just shuffle
static card *totalCard;
const static int totalCardType=4;
const static int totalCardStyle=15;

static int clientInputChecker[5];// same with max card but wrote 5 cuz compile issue


int main(int argc, char** argv) {

	if(argc != 2) {
		fprintf(stderr,"Usage: %s Port",argv[0]);
		return 0;  
	}	
    char username[]="";
    char password[]="";

    currentUsers = malloc((sizeof(*currentUsers))*userlimit);
    for(int i=0;i<userlimit;i++){
        currentUsers[i].userNumber=-1; // default -1 means no client
        currentUsers[i].currnetRequest=-1;
        //currentUsers[i].playingStatus=0;
        currentUsers[i].room=-1;
        waitingLine[i]=-1;
        //printf("[%d] num is : %d\n",i,currentUsers[i].userNumber); test code
    }
     


    
    const int maxRoom=5;
    

    for(int i=0;i<maxRoom;i++){
        clientInputChecker[i]=0;
    }

    pthread_t cardRoom[maxRoom];

    //int specialRequestClient[userlimit];
    int players[4]={-1,-1,-1,-1};
    int turn=-1;
    int purpose=-1;
    char notEnoughPlayer[]="not enough player please wait ... \n";
    char notYourTurn[]="it's not your turn please wait for your turn... \n";
    char askPurpose[]="?purpose?\n";
    
    //initialize rooms
    for(int i=0;i<maxRoom;i++){
        cardRoom[i]=-1;
    }
    //create card deck
    totalCard = malloc((sizeof(*totalCard))*(totalCardStyle*totalCardType)); // 4*15 =60
    char totalCardTypeList[]={'R','G','B','Y'};
    char totalCardStyleList[]={'1','2','3','4','5','6','7','8','9','p','j','d','a','s','q'};
    int cardNumber=0;
    for(int i=0;i<totalCardType;i++){
        for(int j=0;j<totalCardStyle;j++){
            totalCard[cardNumber].cardType=totalCardTypeList[i];
            totalCard[cardNumber].cardStyle=totalCardStyleList[j];
            
            if(totalCardStyleList[j]=='a')
                totalCard[cardNumber].attack=2;
            else if(totalCardStyleList[j]=='s')
                totalCard[cardNumber].attack=3;
            else
                totalCard[cardNumber].attack=0;
            
            if(totalCardStyleList[j]=='q')
                totalCard[cardNumber].ultimate=1;
            else
                totalCard[cardNumber].ultimate=0;

            totalCard[cardNumber].used=0; // if given too user >> 1
            cardNumber++;
        }
    }

    
    //sem = sem_open(SEM_NAME, O_RDWR | O_CREAT, 0777, 1);
    /*
    if(sem == SEM_FAILED)
	{
		fprintf(stderr, "Sem_Open Error \n");
		exit(1);
	}
    */
    printf("Configuring local address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(0, argv[1], &hints, &bind_address);


    printf("Creating socket...\n");
    SOCKET socket_listen;
    socket_listen = socket(bind_address->ai_family,
            bind_address->ai_socktype, bind_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_listen)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Binding socket to local address...\n");
    if (bind(socket_listen,
                bind_address->ai_addr, bind_address->ai_addrlen)) {
        fprintf(stderr, "bind() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    freeaddrinfo(bind_address);


    printf("Listening...\n");
    if (listen(socket_listen, 10) < 0) {
        fprintf(stderr, "listen() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }

    //master is global
    FD_ZERO(&master);
    FD_SET(socket_listen, &master);
    SOCKET max_socket = socket_listen;

    printf("Waiting for connections...\n");
    printf("Player Limit is %d\n",playerlimit);


    while(1) {
        fd_set reads;
        reads = master;
        
                    
        if(gameStart==1){
            gameStart=0;
            for(int i=0; i<maxRoom;i++){
                if(cardRoom[i]==-1){
                    roomCount++;
                    //playerSet = malloc((sizeof(*playerSet)));
                    //strcpy(playerSet->playerList, playerSets);
                    //playerSet->roomNumber=roomCount;
                    int *roomNumber =&roomCount;
                    storeClientInRoom(*roomNumber);
                    int statusRoom;
                    printf("card room NO. %d created\n",*roomNumber);
                    statusRoom=pthread_create(&cardRoom[i], NULL, createCardGameRoom, (void *)roomNumber);
                    if(statusRoom==-1){
                    	fprintf(stderr,"PThread  Creation Error \n");
                    	exit(0);
                    }
                    pthread_detach(cardRoom[i]);
                    //function to update user info and make thread
                    break;
                }
            }
            continue;
        }

        if (select(max_socket+1, &reads, 0, 0, 0) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

        SOCKET i;
        SOCKET socket_client;
        for(i = 1; i <= max_socket; ++i) {
            if (FD_ISSET(i, &reads)) {                
                if (i == socket_listen ) {
                    struct sockaddr_storage client_address;
                    socklen_t client_len = sizeof(client_address);
                    socket_client = accept(socket_listen,
                            (struct sockaddr*) &client_address,
                            &client_len);
                    if (!ISVALIDSOCKET(socket_client)) {
                        fprintf(stderr, "accept() failed. (%d)\n",
                                GETSOCKETERRNO());
                        return 1;
                    }

                    char address_buffer[100];
                    getnameinfo((struct sockaddr*)&client_address,
                            client_len,
                            address_buffer, sizeof(address_buffer), 0, 0,
                            NI_NUMERICHOST);
                    printf("New connection request from %s\n", address_buffer);
                    
                    if(connectedClient==userlimit){
                        char notEnoughSpace[]="#Message from server : Server full connection refused#\n";
                        send(socket_client,notEnoughSpace,strlen(notEnoughSpace),0);
                        CLOSESOCKET(socket_client);
                        printf("connection refused.. \n");
                        continue;    
                    } 
                    
                    FD_SET(socket_client, &master);

                    if (socket_client > max_socket)
                        max_socket = socket_client;
                    printf("connected.. \n");

                    connectedClient++;
                    printf("current number of connected client : %d \n",connectedClient);
                    //ask purpose of connection
                    send(socket_client, askPurpose,strlen(askPurpose), 0);

                } 
                else {

                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    int request=0;


                    //client_exit
                    if (bytes_received < 1) { 
                        printf("exit detected.. \n");
                        FD_CLR(i, &master);

                        int deletedWaitingQueue=0; 
                        deleteUserinfo(i);
                        deletedWaitingQueue=deleteWaitingQueue(i);
                        if(deletedWaitingQueue){
                            totalWaitingPlayer--;
                        }
                        connectedClient--;
                        printf("player count : %d \n",connectedClient);
                        if(connectedClient==0){
                            
                            start=0;
                        }
                        CLOSESOCKET(i);                          
                        continue;
                    }


                    clientCode=i;

                    if(checkClientPlayingCard(i)){
                        printf("message from client %d >> %.*s \n",i ,bytes_received, read);
                        printf("client %d is playing card.. message ignored..\n",i);
                        continue; //ignore client playing card
                    }
                    else if(read[0]=='<' && read[bytes_received-2]=='>'){
                            if((read[1]=='=' && read[bytes_received-3]=='=')){
                                char pingReceived[]="ping Recevied \n";
                                send(clientCode,pingReceived, strlen(pingReceived), 0);
                                continue;
                        }
                    }
                    else{
                        printf("[ recevied <%d> >> %.*s form client %d ]\n",bytes_received,bytes_received, read,i);
                    }
                    
                    
                    /*
                    // client from card game came back
                    if(read[0]=='$' && read[bytes_received-2]=='$'){
                        printf("client %d waiting in menu \n",clientCode);
                        char serverReady[]="$ Ready $\n";
                        send(clientCode,serverReady, strlen(serverReady), 0);
                        continue;
                    }
                    */


                    //menu request detector client by client
                    request=checkClientRequest(clientCode);
                    if(request>0){
                        readExtracter(read,bytes_received);
                        //char *requestFromClient=malloc(sizeof(char)*(bytes_received+1));
                        //strcpy(requestFromClient,readExtracter(read,bytes_received));
                        requestSolver(clientCode,read);
                        // all request client by client
                        //free(requestFromClient);
                        continue;
                    }

                    //detect purpose
                    if(read[0]=='!' && read[bytes_received-2]=='!'){
                        purpose=0;// ! ~~ ! something for login for account
                    }
                    if(read[0]=='=' && read[bytes_received-2]=='='){
                        char request[3];
                        for(int idx=0; idx<3;idx++){
                            request[idx]=read[idx+1];
                        }
                        purpose= atoi(request);
                        printf("client %d : request from menu <%d> received..\n",i,purpose);
                    }

                    if(purpose>100){
                        //storeRequestingClient(specialRequestClient,i);
                        if(purpose==101){
                            char searchingForOther[]= "<searching for other players>\n";
                            storeUserPurpose(clientCode,purpose);
                            send(i,searchingForOther, strlen(searchingForOther), 0);
                            
                            storeWaitingQueue(clientCode);
                            purpose=-1; //situation disabled
                            continue;
                        }

                    }
                    
                    //sub menu request my info
                    if(purpose >200 ){
                        //storeRequestingClient(specialRequestClient,i);
                        //new password request
                        if(purpose==201){
                            char pwRequest[]= "=*New password request*=\n";
                            storeUserPurpose(clientCode,purpose);
                            send(i,pwRequest, strlen(pwRequest), 0);
                            purpose=-1; //situation disabled
                            continue;
                        }
                        //account delete request
                        if(purpose==202){
                            
                            char recheck[]= "=? Are you sure? y/n ?=\n";
                            storeUserPurpose(clientCode,purpose);
                            send(i,recheck, strlen(recheck), 0);
                            purpose=-1; //situation disabled
                            continue;
                        }

                        if(purpose==203){
                            //match history request
                        }
                    }
                    // login or account
                    if(purpose== 0){
                        // !@ ~~ @! command : connecting for card game
                        if((read[1]=='@' && read[bytes_received-3]=='@')){
                            printf("client %d is connecting for card game \n",i);
                            purpose=-1;//situation disabled
                            continue;
                        }
                        // !^ ~~ ^! command : connecting for creating account
                        if((read[1]=='^' && read[bytes_received-3]=='^')){
                            if(read[2]=='!' && read[bytes_received-4]=='!'){
                                //client sent ID and password
                                int resultOfReceivedData=0;
                                //start=1;//countdown end
                                resultOfReceivedData=saveUserInfo(read ,bytes_received);
                                if(resultOfReceivedData){
                                    char saveComplete[]="##Message from server : your account is created##";
                                    send(i,saveComplete, strlen(saveComplete), 0);
                                }
                                else{
                                    char saveError[]="##Message from server : identical username exists##";
                                    send(i,saveError, strlen(saveError), 0);
                                }
                                purpose=-1;//situation disabled
                                continue;
                            }
                            printf("client %d is connecting for creating account \n",i);
                            //start=1;//countdown end
                            //printf("CountDown disabled...\n");
                            purpose=-1; //situation disabled
                            char requireData[]="?^send me ID and PW^?\n"; 
                            send(i,requireData, strlen(requireData), 0);
                            continue;
                        }
                        // !* ~~ *! command : connecting for login
                        if((read[1]=='*' && read[bytes_received-3]=='*')){
                            if(read[2]=='!' && read[bytes_received-4]=='!'){
                                //client sent ID and password
                                int loginResult=0;
                                //start=1;//countdown end
                                
                                loginResult=loginCheck(read ,bytes_received);
                                if(loginResult==1){
                                    char loginComplete[]="#!Message from server : login success!! welcome!!#\n";
                                    send(i,loginComplete, strlen(loginComplete), 0);
                                }
                                else if(loginResult==0){
                                    char loginError[]="##Message from server : wrong ID or PW ##";
                                    send(i,loginError, strlen(loginError), 0);
                                }
                                else{
                                    char loginExist[]="##Message from server : multi login refused##";
                                    send(i,loginExist, strlen(loginExist), 0);
                                }
                                purpose=-1;//situation disabled
                                continue;
                            }
                            printf("client %d is connecting for login\n",i);
                            //start=1;//countdown end
                            //printf("CountDown disabled...\n");
                            purpose=-1; //situation disabled
                            char requireData[]="?^send me ID and PW^?\n"; 
                            send(i,requireData, strlen(requireData), 0);
                            continue;
                        }
                    } // if purpose 0 in else

                    /*    
                    // nomral
                    if(purpose== -1){
                        
                        if(totalWaitingPlayer<2){
                            send(i,notEnoughPlayer,strlen(notEnoughPlayer),0);
                            continue;
                        }

                        if(i!=turn){
                            send(i,notYourTurn,strlen(notYourTurn),0);
                            continue;
                        }

                        if(i==turn){
                            turn=changeTurn(players,turn);
                        }
                    }
                    */

                    SOCKET j; // send message to others
                    for (j = 1; j <= max_socket; ++j) {
                        if (FD_ISSET(j, &master)) {

                            if (j == socket_listen || i==j)
                                continue;
                            else{
                                send(j, read, bytes_received, 0);                              
                            }
                        }
                    }
                    continue;
                }  // else
            }//if FD_ISSET
        }  //for i to max_socket
    }//while(1)s



    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    printf("Finished.\n");

    return 0;
}

/*
int checkUserInfo(char *receviedData ,int dataLength){
    int bytes_received=dataLength;
    int divisionPos=0;
    char word[2]="\0";//for using strcmp and strcpy
    printf("Extracting information from receivedData .. \n");
    for(int i=3;i<bytes_received-4;i++){
        if(receviedData[i]=='+'){
            divisionPos=i;
            break;
        }
    }
    char username[divisionPos-3+1];//!^!erase + null
    word[0]=receviedData[3];
    strcpy(username,word);
    for(int i=4;i<divisionPos;i++){
        word[0]=receviedData[i];
        strcat(username,word);
    }
    char password[bytes_received-(divisionPos+1)-4+1];//erase ~ divpos !^!\\n + null 
    word[0]=receviedData[divisionPos+1];
    strcpy(password,word);
    for(int i=divisionPos+2;i<bytes_received-4;i++){
        word[0]=receviedData[i];
        strcat(password,word);
    }
    int storeResult=0;
    printf("Extraction complete..\n");
    printf("ID : %s \nPW: : %s \n",username,password);
    

clientInputChecker
}
*/

int checkClientPlayingCard(int client){
    for (int i=0;i<userlimit;i++){
        if(client == currentPlayers[i]){
            return 1; //client is playing Card
        }
    }
    return 0;
}
void storeClientInRoom(int roomNumber){
    for(int i=0;i<userlimit;i++){
        if(currentUsers[i].userNumber==-1) //ignore empty 
            continue;
        for(int j=0;j<playerlimit;j++){
            if(currentUsers[i].userNumber==playerSets[j]){
                //currentUsers[i].playingStatus=1;
                currentUsers[i].room=roomNumber;
            }
        }
    }
}

void readExtracter(char read[],int bytes_received){

    char tmp[bytes_received+1];
    for(int i=0;i<bytes_received;i++){
        tmp[i]=read[i];
    }
    strcpy(read,tmp);

    return ;
}
int clientIdxFinder(int clientCode){
    for(int i=0;i<userlimit;i++){
        if(currentUsers[i].userNumber==clientCode)
            return i;
    }
}

void requestSolver(int clientCode, char message[]){

    int clientIdx = clientIdxFinder(clientCode);
    int request = currentUsers[clientIdx].currnetRequest;
    
    //currentUsers[clientCode].currnetRequest=-1;// request solved
    if(request>100){
        if(request==101){
            totalWaitingPlayer++;
            pthread_t thread_id;
            if(totalWaitingPlayer==1){
                printf("game request form client : %d \n",clientCode);
                int status;
                printf("thread created countdown start\n");
                
            	status=pthread_create(&thread_id, NULL, waitingTimeOut,NULL);
                if(status==-1){
            		fprintf(stderr,"PThread  Creation Error \n");
            		exit(0);
            	}
                
                pthread_detach(thread_id);
            }
            else if(totalWaitingPlayer>1){
                printf("addtional game request from client %d \n",clientCode);
                char newcomer[]="$ newcomer $\n";
                send(clientCode,newcomer, strlen(newcomer), 0);
                //printf("sending  signal to pthread.. \n");
                //signal(SIGTERM,signalDetection);
                //pthread_kill(thread_id,SIGTERM);
                printf("\n");
            }
            currentUsers[clientIdx].currnetRequest=-1;// request solved
            return;
        }
    }
    
    
    if(request>200){
        //change password
        if(request==201){
            printf("password change new login file process active \n");
            if(createAccount(currentUsers[clientIdx].username,message)){
                printf("new login file creation success \n");
            }
            else{
                printf("rewrite failed\n");
            }
            currentUsers[clientIdx].currnetRequest=-1;// request solved
            return;
        }
        //delete account
        if(request==202){
            if(message[0]=='y'||message[0]=='Y'){
                printf("%s account >>delete process proceed \n",currentUsers[clientIdx].username);
                deleteDirectory(clientCode);
                char exit[]="#goodbye#\n";
                send(clientCode,exit, strlen(exit), 0);
                deleteUserinfo(clientCode);
                return;
            }
            else{
                printf("request <%d> from client :%d canceled\n",request,currentUsers[clientIdx].userNumber);
                currentUsers[clientIdx].currnetRequest= -1;
                return;
            }
        }


    }
}
int checkClientRequest(int clientCode){

    for(int i=0;i<userlimit;i++){
        if((currentUsers[i].userNumber==clientCode)&&(currentUsers[i].currnetRequest!=-1)){
            printf("client : %d >> current request : %d \n",clientCode,currentUsers[i].currnetRequest);
            return currentUsers[i].currnetRequest;
        }
    }
    return 0;

}
/*

void storeRequestingClient(int specialRequestClient[],int client){
    for(int i=0;i<userlimit;i++){
        if(specialRequestClient[i]==-1){
            specialRequestClient[i]=client;
            return ;
        }
    }
}
*/
void initializePlayerSet(){
    for (int i=0;i<playerlimit;i++){
        playerSets[i]=-1;
    }
}
void playerSetStore(int *players ,int newcomer){

    for(int i=0;i<playerlimit;i++){
        if(players[i]<1){
            players[i] = newcomer;
            printf("In playerSet or currentPlayers client: %d >> %d stored.. \n",i,newcomer);
            break;
        }        
    } 
    
}
void playerSetDelete(int *players, int goodbye){

    for(int i=0;i<playerlimit;i++){
        if(players[i]==goodbye){
            players[i] = -1;
            printf("In playerSet %d >> %d deleted.. \n",i,goodbye);        
            break;
        }
    } 
    
}

int deleteWaitingQueue(int client){
    for(int i=0;i<userlimit;i++){
        if(waitingLine[i]==client){
            printf("client : %d deleted in waiting queue No. %d\n",client,i);
            waitingLine[i]=-1;
            return 1; //deleted something
        }
    }
    return 0;
}

void storeWaitingQueue(int client){

    for(int i=0;i<userlimit;i++){
        if(waitingLine[i]==-1){
            printf("client : %d stored in waiting queue No. %d\n",client,i);
            waitingLine[i]=client;
            return;
        }
    }
}
/*
void *ping(){

    char pingMessage[]="<self ping>\n";

    while(1){

        sleep(2);
        send(0,pingMessage, strlen(pingMessage), 0);
    }
}
*/

void *countdown(void *arg){
    
    timeLimit_thread_data *thread_data = (timeLimit_thread_data*)arg;
    int roomNumber = thread_data->roomNumber;
    int clientNumber = thread_data->clientNumber;
    //clientInputChecker[roomNumber-1]=0;
    int timeLimit = 20; // default 20s
    sleep(1);
    for(int i=0; i < timeLimit; i++){
        printf("client %d >> counting %d  \n",clientNumber,timeLimit - i);
        //printf("client %d >> input checker inside thread :%d\n",clientNumber,clientInputChecker[roomNumber-1]);
        
        if(clientInputChecker[roomNumber-1]==1){ // -1 cuz romm Num starts from 1 but idx starts from 0 
            printf("countdown terminated for client %d in room No. %d \n",clientNumber,roomNumber);
            thread_data->timeOut=0;
            clientInputChecker[roomNumber-1]=0;
            pthread_exit(NULL);
        }
        sleep(1);
        


    }

    printf("client %d in room No. %d time out \n",clientNumber,roomNumber);
    char timeOutMessage[]="! Time Out !\n";
    send(clientNumber,timeOutMessage, strlen(timeOutMessage), 0);
    thread_data->timeOut=1;
    clientInputChecker[roomNumber-1]=0;
    pthread_exit(NULL);

}

void *waitingTimeOut(){
    int waitingTime=8;
    int addTime=2;
    int addTimecount=0;
    char failMessage[]="{# Message from server : match failed #}\n";
    char successMessage[]="{ match success game will start soon... }\n";
	for(int i=0; i < waitingTime; i++){
        if(totalWaitingPlayer==4){
            printf("client count >> %d / Waitng Queue full\n",totalWaitingPlayer);
            break;
        }
        printf("current waitng client >> %d \n",totalWaitingPlayer);
        printf("%d seconds before closing waiting room.. \n",waitingTime-i);
        sleep(1);
        if(/*(start==1) &&*/(totalWaitingPlayer==2)&&(addTimecount==0)){
            addTimecount++;
            printf("additional client entrance detected\n");
            printf("current totalWaitingPlayer : %d >> add %d sec\n",totalWaitingPlayer,addTime);
            waitingTime = waitingTime+addTime;
        }
        if(totalWaitingPlayer==0){
            printf("All client out from waitingLine >> Close Room\n");
            start=0;
            
            return NULL;
        }

	}
    
    if(totalWaitingPlayer==1){
        for(int i=0;i<userlimit;i++){
            if(waitingLine[i]!=-1){
                printf("not enough players matched for game sending message to client : %d\n",waitingLine[i]);
                send(waitingLine[i], failMessage,strlen(failMessage), 0);
                deleteWaitingQueue(waitingLine[i]);
                
            }
        }
        
    }
    if(totalWaitingPlayer>1){
        printf("card game start command activated.. \n");
        gameStart=1; // command to make additional thread for game
        for(int i=0;i<userlimit;i++){
            if(waitingLine[i]!=-1){
                FD_CLR(waitingLine[i],&master); // stop listening card game client from main thead
                printf("sending match success message to client : %d\n",waitingLine[i]);
                send(waitingLine[i], successMessage,strlen(successMessage), 0);
                playerSetStore(playerSets,waitingLine[i]);
                playerSetStore(currentPlayers,waitingLine[i]);
                deleteWaitingQueue(waitingLine[i]);
            }
        }
    }

    
    totalWaitingPlayer=0;
    
    start=0;


	return NULL;
}

void *createCardGameRoom(void *roomNumber){
    int players[4]; // needed for storng client info 
    int *room=(int *)roomNumber;
    int roomNum = *room; // room number
    int playerCount=0;  // total client strated card game
    int survivers=0;    // playerCount - dropout count
    int turn =-1;   //turn for card game
    int initialCard=6; // select many cards to start with
    int gameover=0; // 1 >> game over 2>> game end cuz of connection lost
    int dropoutIdx=-1; // loser idx
    int ultimateActivate=0; // 1: means ultimate

    //jump /plus /direction /weak attk(a) /strong attk(s) /r g b y ultimate
    int specialOccasion[]={0,0,0,0,0,0,0,0,0,0}; // needed for screenmaker
    int changeTurnCount=1;//default turn changes once / 0 :extra turn(p) 2 : jump
    int turnReverse=0;
    int attkPoint=0;
    int attackComplete=0; // 1 >> attkPoint to 0
    int winnerIdx=-1;
    int timeLimitActive =0; // 1>> active every time send new screen to players
    //int timeOutActive=0; // 1>> chagnege turn code active only works per time out
    //int lazyClientNumber=-1; // client time out 

    SOCKET max_socket=-1;
    
    card currentDeck;
    card clientInput;

    card *cardDeck = malloc((sizeof(card))*(totalCardStyle*totalCardType));
    memcpy(cardDeck,totalCard,(sizeof(*totalCard))*(totalCardStyle*totalCardType));
    
    card *usedCardDeck= malloc((sizeof(card))*(totalCardStyle*totalCardType));
    
    shuffleCard(cardDeck,0);

    printf("1st deck created [%c%c] in room No. %d \n",cardDeck[0].cardType,cardDeck[0].cardStyle,roomNum);
    currentDeck = cardDeck[0];
    cardDeck[0].used =1;

    //shuffle card
    /* test code print shuffled card
    for (int i = 0; i < 60; i++)
    {
        printf("%c%c ",cardDeck[i].cardType,cardDeck[i].cardStyle);
    }
    */

   //master card is global
    FD_ZERO(&masterCard);


    for(int i=0;i<playerlimit;i++){
        players[i] = playerSets[i];
        if(players[i]!=-1){
            FD_SET(players[i],&masterCard);
            if(turn ==-1){
                turn = players[i];
                printf("first turn is client : %d in room %d \n",turn,roomNum);
            }
            if(max_socket<players[i]){
                max_socket=players[i];
            }
            printf("client %d entered in room %d \n",players[i],roomNum);
            playerCount++;
        }
    }
    //FD_SET(0,&masterCard);

    survivers = playerCount; // survivers are remain players while playerCount only care how many players started this game

    userInfo *playerInfo = malloc((sizeof(userInfo))*(playerCount));
    storePlayerRoomInfo(players,playerInfo); // client default set 

    card **playerDeck = (card **)malloc((sizeof(card*))*(playerCount));
    for(int i=0;i<playerCount;i++){
        playerDeck[i] = (card *)malloc((sizeof(card))*(MAX_CARD));
        playerInfo[i].cardDeck = playerDeck[i];//card deck address connected
        for(int j = 0; j < MAX_CARD; j++){
            playerDeck[i][j].used = -1; // default no cards 
        }
        // 1st time always possible
        //if(checkCardCount(initialCard,playerInfo[i])){
        sendCardToClient(initialCard,&playerInfo[i],cardDeck);
        //}
        //else{
        //    printf("client : %d >> name :%s card overloaded \n",playerInfo[i].userNumber,playerInfo[i].username);
        //}
    }
    
    initializePlayerSet(); //playerSet is global needed to make other rooms
    printf("playerSet initialized.. ready to make new room \n");
    char startMessage[]="@ game start @\n";
    send(turn,startMessage, strlen(startMessage), 0);

    int historyFileNumber = createHistoryFileInServer();
    printf("HISTORY file : %d created \n",historyFileNumber);
    /*
    char client[200];
    strcpy(client,">>\n");
    for(int i=0;i<playerCount;i++){

        printf("print user info >> %d\n",i);
        printf("usernumber : %d \n",playerInfo[i].userNumber);
        printf("username : %s \n",playerInfo[i].username);
        printf("user card count : %d \n",playerInfo[i].cardCount);
        printCard(client,playerInfo[i]);
        printf("card %s\n",client);
    
    }
    */
    // auto self ping
    int status;
    /*
    pthread_t ping_thread;
    printf("ping thread created in room No. %d\n",roomNum);
    status=pthread_create(&ping_thread, NULL, ping, NULL);
    if(status==-1){
    	fprintf(stderr,"PThread  Creation Error \n");
    	exit(0);
    }
    pthread_detach(ping_thread);
*/
    // thread for limiting input time
    pthread_t timeLimit_thread;
    
    timeLimit_thread_data thread_data;
    thread_data.roomNumber=roomNum;
    thread_data.clientNumber=-1;
    thread_data.timeOut=0; 
    

    int readyActive=0;
    while(1){
        fd_set readInThread;
        readInThread = masterCard;



        //masterCard is global
        if (select(max_socket+1, &readInThread, 0, 0, 0) < 0) {
            fprintf(stderr, "In thread select() failed. (%d)\n", GETSOCKETERRNO());
            exit(1);
        }


        for(int i=1;i<=max_socket;i++){
            if (FD_ISSET(i, &readInThread)) {
                
                char read[1024];
                int bytes_received = recv(i, read, 1024, 0);
                //printf("recevied <%d> : %.*s by client %d\n",bytes_received,bytes_received, read,i);
                //printf("screenActive == %d\n",screenActive);
                
                if(thread_data.timeOut==1 && (read[0]=='!' && read[bytes_received-2]=='!')){
                    //timeOutActive=1;
                    int warnPlayerIdx;
                    //lazyClientNumber=turn;
                    thread_data.timeOut=0;
                    //clientInputChecker[roomNum-1]=0;
                    printf("waring to client %d in room No. %d \n",turn,roomNum);
                    warnPlayerIdx= playerIdxFinder(playerInfo, turn ,playerCount);
                    playerInfo[warnPlayerIdx].warning++;
                    readyActive=-1; // skip card check
                    //give warning and one card with force
                    if((playerInfo[warnPlayerIdx].warning==3)||(playerInfo[warnPlayerIdx].cardCount==15||playerInfo[warnPlayerIdx].cardCount+attkPoint>15)){
                        survivers--;
                        char forcedExit[]="#* GET OUT *#\n";
                        send(playerInfo[warnPlayerIdx].userNumber,forcedExit,strlen(forcedExit),0);

                        printf("Erasing player informaion ] client %d in room No . %d \n",playerInfo[warnPlayerIdx].userNumber,roomNum);
                        playerEraser(playerInfo, warnPlayerIdx,playerCount,cardDeck);
                        printf("reconnect client : %d in room No. %d to main thread \n",playerInfo[warnPlayerIdx].userNumber,roomNum);
                        FD_SET(playerInfo[warnPlayerIdx].userNumber,&master);// reconnect with main thread
                        deleteCurrnetPlayer(playerInfo[warnPlayerIdx].userNumber);
                        playerInfo[warnPlayerIdx].disquealified=1;
                        if(survivers==1){
                            gameover=2;
                        }
                    }
                    else if(attkPoint>0){
                        printf("card damage : %d to client :%d in room No. %d\n",attkPoint,i,roomNum);
                        sendCardToClient(attkPoint,&playerInfo[warnPlayerIdx],cardDeck);
                        attackComplete=1;
                        specialOccasion[9]=1;
                    }
                    else{
                        sendCardToClient(1,&playerInfo[warnPlayerIdx],cardDeck);
                    }
                    printf("client %d time out turn changed\n",turn);
                    turn = changeTurn(playerInfo,turn,playerCount);
                }
/*
                if((i==lazyClientNumber)&&(timeOutActive==1)){
                    timeOutActive=0; //to defualt
                    lazyClientNumber=-1;
                    turn = changeTurn(playerInfo,turn,playerCount);
                    
                }
*/
                /*
                if(i==0){
                    printf("\nself auto ping >> %.*s \n",bytes_received, read);
                    continue;
                }
                */
                
                if(bytes_received<1){// client terminated connection
                    printf("client %d exited from card game room No. %d \n",i,roomNum);
                    survivers--;
                    dropoutIdx = playerIdxFinder(playerInfo, i ,playerCount);
                    printf("Erasing player informaion ] client %d in room No . %d \n",playerInfo[dropoutIdx].userNumber,roomNum);
                    playerEraser(playerInfo, dropoutIdx,playerCount,cardDeck);
                    printf("reconnect client : %d in room No. %d to main thread \n",playerInfo[dropoutIdx].userNumber,roomNum);
                    FD_SET(playerInfo[dropoutIdx].userNumber,&master);// reconnect with main thread
                    deleteCurrnetPlayer(playerInfo[dropoutIdx].userNumber);
                    playerInfo[dropoutIdx].disquealified=1;
                    if(survivers==1){
                        
                        gameover=2;
                        //break;
                    }
                    turn = changeTurn(playerInfo,turn,playerCount);
                    readyActive=-1;// player terminated connection skip card check
                }
                

                
                // this code activates only once
                if(read[0]=='@' && read[bytes_received-2]=='@'){
                    readyActive=1;
                    printf("%.*s from client %d \n",bytes_received,read,i);
                    //do not continue needed to make game screen
                }
                /*
                else if((read[0]=='@' && read[bytes_received-2]=='@')&&screenActive==1){
                    printf("%.*s from client %d \n",bytes_received,read,i);
                    printf("already sent\n");
                    continue;
                }
                */
                
                /*
                else if((read[0]=='?' && read[bytes_received-2]=='?')){
                    char information[12];
                    sendDeckTurn(information,currentDeck ,turn);
                    printf("client %d card deck and turn request send >> [%s] \n",i,information);
                    send(i,information, strlen(information), 0);
                    continue;
                }
                */
                if(readyActive==0){ // code for checking card input from client
                    if(turn!=i){ //ignore client if not turn
                        printf("current turn client>> %d ignore client %d \n",turn,i);
                        continue;
                    }
                    

                    //function of cards
                    int cardPosition=-1;
                    int user;
                    user = playerIdxFinder(playerInfo,i,playerCount);
                    
                    if(bytes_received > 2){
                        read[0]=toupper(read[0]); // type is always upper alpha
                        read[1]=tolower(read[1]);  // style is always lower alpha or number
                        //compare input card with currentDeck card 1: possible 0: invalid
                        if(checkClientCardValid(read,currentDeck,attkPoint)){ 
                            printf("input compare : possible >> card Exist check active \n");
                            //card exist check
                            cardPosition = checkClientCardExist(read, playerInfo[user].cardDeck );
                            if(cardPosition==-1){
                                printf("nonexist input from client %d in room No. %d>> ignored\n",i,roomNum);
                                continue;
                            }
                            else{
                            printf("take [%.*s] from client %d in room No. %d\n",2,read,i,roomNum);
                            printf("change current Deck to [%.*s]\n",2,read);
                            currentDeck = playerInfo[user].cardDeck[cardPosition];
                            takeCardFromClient(cardPosition, &playerInfo[user],cardDeck);
                            if(playerInfo[user].cardCount==0){ // card all used
                                winnerIdx=user;// store winner idx
                                gameover=1;
                            }
                            //break;
                            }
                        }
                        else{
                            printf("invalid input from client %d in room No. %d>> ignored\n",i,roomNum);
                            continue;
                        }
                    }
                    else if((bytes_received==2)&&(read[0]=='0')&&(read[1]=='\n')){  // 0\n is request card
                        if(attkPoint==0){ //normal card send
                            if(playerInfo[user].cardCount == MAX_CARD){
                                printf("client %d in room No. %d has maximum card >> request refused \n",i,roomNum);
                                continue;
                            }
                            printf("card request from client %d in room No. %d\n",i,roomNum);
                            sendCardToClient(1,&playerInfo[user],cardDeck);// client requested a card
                            //break;
                        }
                        else{ //attacked by other player
                            attackComplete=1;
                            if(attkPoint+playerInfo[user].cardCount<16){
                                printf("card damage : %d to client :%d in room No. %d\n",attkPoint,i,roomNum);
                                sendCardToClient(attkPoint,&playerInfo[user],cardDeck);// damaged
                                specialOccasion[9]=1;
                            //break;
                            }
                            else{
                                printf("client : %d in room No. %d card overflow >> RIP \n",i,roomNum);
                                survivers--;
                                // send message to dead user and ersase informaion
                                dropoutIdx = playerIdxFinder(playerInfo, i ,playerCount);
                                char cardOverflowMessage[]="#* you died *#\n";
                                send(playerInfo[dropoutIdx].userNumber, cardOverflowMessage, strlen(cardOverflowMessage), 0);
                                printf("Erasing player informaion ] client %d in room No . %d \n",playerInfo[dropoutIdx].userNumber,roomNum);
                                playerEraser(playerInfo, dropoutIdx,playerCount,cardDeck);
                                printf("reconnect client : %d to main thread \n",playerInfo[dropoutIdx].userNumber);
                                FD_SET(playerInfo[dropoutIdx].userNumber,&master);// reconnect with main thread
                                deleteCurrnetPlayer(playerInfo[dropoutIdx].userNumber);
                                if(survivers==1){
                                    gameover=2;
                                }
                                //break;
                            }
                        }
                    }
                    else{// input less than 2 is invalid
                        printf("invalid input from client %d in room No. %d>> ignored\n",i,roomNum);
                        continue;
                    }
                        
                    
                    
                    //countdown stop 
                    printf("<valid data confirmed countdown stop> \n");
                    //printf("client input : %.*s",bytes_received, read);
                    clientInputChecker[roomNum-1]=1;
                    //printf("client input checker outside thread :%d\n",clientInputChecker[roomNum-1]);


                    // check if client cardCount is over 15 or reached 0

                     //here

                    // activate 100% after valid input from client then take input card from client
                    // special card detector 
                    //    jump /plus /direction /weak attk(a) /strong attk(s) /r g b y ultimate 
                    // idx: 0    1      2             3               4        5 6 7 8   
                    switch(read[1]){
                        case 'j': 
                            changeTurnCount=2;
                            specialOccasion[0]=1;
                            break;                         
                        case 'p': 
                            changeTurnCount=0;
                            specialOccasion[1]=1;
                            break;    
                        case 'd': 
                            turnReverse=1;
                            specialOccasion[2]=1;
                            break; 
                        case 'a': 
                            attkPoint+=2;
                            specialOccasion[3]=1;
                            break;
                        case 's': 
                            attkPoint+=3;
                            specialOccasion[4]=1;
                            break;
                        case 'q': 
                            ultimateActivate=1;
                    }
                    if(ultimateActivate){
                        ultimateActivate=0;
                        switch(read[0]){
                            case 'R':
                                attkPoint+=5;
                                specialOccasion[5]=1;
                            break;
                            case 'G':
                                turnReverse=1;
                                specialOccasion[6]=1;
                            break;                        
                            case 'B':
                                attkPoint=0;
                                specialOccasion[7]=1;
                            break;                        
                            case 'Y':
                                specialOccasion[8]=1;
                            break;                        
                        }
                    }

                    if(turnReverse==1){
                        turnTableReverse(playerInfo,playerCount);
                    }


                    //change turn 
                    if(changeTurnCount>0){ // 0:extra turn / 1: normal / 2: jump
                        for(int turnCount=0;turnCount<changeTurnCount;turnCount++){
                            turn = changeTurn(playerInfo,turn,playerCount);
                        }
                        
                    }


                    changeTurnCount=1;//back to default
                    turnReverse=0;
                }// if (readyActive ==0)



                if(readyActive==1){ // this code activate only once
                    printf("client screen auto active \n");
                    
                    // 1st countdown
                    printf("thread created countdown start\n");
                    thread_data.clientNumber=turn;
            	    status=pthread_create(&timeLimit_thread, NULL, countdown,(void *)&thread_data);
                    if(status==-1){
            		    fprintf(stderr,"PThread  Creation Error \n");
            		    exit(0);
            	    }
                    pthread_detach(timeLimit_thread);                
                    //timeLimitActive=1;

                    readyActive=0;
                }
                
                if(readyActive==-1){//this code activates only when player terminates connectiton
                    readyActive=0;  // useage : skip card check 
                }

                // observe all client in server
                /*
                printf("===================\n");
                for(int idx=0;idx<playerCount;idx++){
                    printf("%s : ",playerInfo[idx].username);
                    if(playerInfo[idx].cardCount != -1){
                        for(int j=0; j< MAX_CARD; j++){
                            if(playerInfo[idx].cardDeck[j].used !=-1){
                            printf(" [%c%c] ",playerInfo[idx].cardDeck[j].cardType,playerInfo[idx].cardDeck[j].cardStyle);
                            }
                        }
                    }
                    else{
                        printf(" R.I.P :( \n");
                    }
                    printf("\ncard count : %d \n",playerInfo[idx].cardCount);
                }
                printf("currnet deck card [%.*s] \n",2,read);
                printf("\ncurrent turn : %d\n",turn);
                printf("===================\n");
                */

                //file store code right here@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
                char commonScreen[100];
                char currentDeckCard[20];
                char cardgameOneTurnData[1024];
                int nullCount=0;
                int cardDeckLeft=0;
                char cardLeft[3];
                nullCount = cardDeckCounter(cardDeck);
                cardDeckLeft = totalCardStyle*totalCardType -nullCount;
                sprintf(cardLeft,"%d",cardDeckLeft);                
                cardsLeftInDeck(commonScreen,cardLeft);
                currnetCardInDeck(commonScreen,currentDeck);
                strcpy(cardgameOneTurnData,commonScreen);
                screenMakerForServer(cardgameOneTurnData,playerInfo,playerCount);
                strcat(cardgameOneTurnData,"\n");

                // special card message create / detect special card info
                int inputClientIdx=-1;
                inputClientIdx =playerIdxFinder(playerInfo, i ,playerCount);
                
                char *specialOccasionClientName= playerInfo[inputClientIdx].username;

                int specialOccasionCount=(int)(sizeof(specialOccasion)/sizeof(specialOccasion[0]));
                int styleIdx=-1;
                
                for(int idx=0;idx<specialOccasionCount;idx++){
                    if(specialOccasion[idx]==1){
                        styleIdx=idx;
                        break;
                    }
                }
                if(styleIdx !=-1){
                    addSpecialOccastionMessage(cardgameOneTurnData,styleIdx,specialOccasionClientName,attkPoint);
                }
                //observe all in server
                printf("%s \n",cardgameOneTurnData);

                //card deck test code
                int zeros=0;
                int ones =0;
                int twos=0;
                for(int card=0;card<60;card++){
                    printf("%d ",cardDeck[card].used);
                    if(cardDeck[card].used==0)
                        zeros++;
                    if(cardDeck[card].used==1)
                        ones++;                        
                    if(cardDeck[card].used==2)
                        twos++;                
                }
                printf("\nkeeping cards : %d \n",zeros);
                printf("clients card : %d\n",ones);
                printf("returned card %d\n",twos);


                //here store in file
                storeGameHistoryServer(historyFileNumber,cardgameOneTurnData);

                //active when gameover
                if(gameover>0){
                    for(int user=0; user<playerCount;user++){
                        if(playerInfo[user].disquealified != 1){ // someone who exited or ignored turn won't get history
                            copyHistoryFromServerToClient(playerInfo[user].username,historyFileNumber);
                        }
                    }
                    
                }

                if(gameover ==1){ // game over some one used all cards
                    for(int idx=0;idx<playerCount;idx++){
                        if(playerInfo[idx].cardCount!=-1){
                            if(idx == winnerIdx){
                                char winnermessage[]="#! you won !#\n";
                                send(playerInfo[winnerIdx].userNumber,winnermessage,strlen(winnermessage),0);
                            }
                            else{
                                char losermessage[]="#~ you lose ~#\n";
                                send(playerInfo[idx].userNumber,losermessage,strlen(losermessage),0);
                            }
                            printf("Erasing player informaion ] client %d in room No . %d \n",playerInfo[idx].userNumber,roomNum);
                            playerEraser(playerInfo, dropoutIdx,playerCount,cardDeck);
                            printf("reconnect client : %d in room No. %d to main thread \n",playerInfo[idx].userNumber,roomNum);
                            FD_SET(playerInfo[idx].userNumber,&master);
                            deleteCurrnetPlayer(playerInfo[idx].userNumber);
                        }
                    }
                    break;
                }
                
                if(gameover ==2){// gameover cuz only one left
                    clientInputChecker[roomNum-1]=1;
                    winnerIdx=surviverCheck(playerInfo,playerCount);
                    printf("only one player left >> game terminated \n");
                    printf("winner : client %d in room No. %d sending message..\n",playerInfo[winnerIdx].userNumber,roomNum);
                    char surrenderVictory[]="## other player eliminated ##\n";
                    send(playerInfo[winnerIdx].userNumber,surrenderVictory,strlen(surrenderVictory),0);
                    
                    printf("Erasing player informaion ] client %d in room No . %d \n",playerInfo[winnerIdx].userNumber,roomNum);
                    playerEraser(playerInfo, winnerIdx,playerCount,cardDeck);
                    printf("reconnect client : %d in room No. %d to main thread \n",playerInfo[winnerIdx].userNumber,roomNum);
                    FD_SET(playerInfo[winnerIdx].userNumber,&master);// reconnect with main thread    
                    deleteCurrnetPlayer(playerInfo[winnerIdx].userNumber);
                    break;
                }
                

                char **playerScreen=(char**)malloc((sizeof(char*))*playerCount);
                for(int m=0;m<playerCount;m++){
                    playerScreen[m]=(char*)malloc(sizeof(char)*1024);
                }
                
                

                //screen maker for client materials same with screen maker for server

                playerScreenMaker(playerScreen,playerInfo,playerCount,commonScreen);

                SOCKET j; // send message to others
                
                for (int j = 0; j <playerCount; j++) {
                    if(playerInfo[j].cardCount!=-1){
                        if(styleIdx != -1){ // add special message if something happened
                            addSpecialOccastionMessage(playerScreen[j],styleIdx,specialOccasionClientName,attkPoint);
                        }

                        if(dropoutIdx!= -1){ // dead user 
                            strcat(playerScreen[j],playerInfo[dropoutIdx].username);
                            strcat(playerScreen[j]," dropout from game..\n");
                        }
                        
                        if(playerInfo[j].userNumber==turn){
                            char yourTurn[]="\nYour turn >>";
                            strcat(playerScreen[j],yourTurn);
                        }
                        else{
                            char notYourTurn[]="\n[Not your turn. command ignored]";
                            strcat(playerScreen[j],notYourTurn);
                        }


                        send(playerInfo[j].userNumber,playerScreen[j], strlen(playerScreen[j]), 0);
                    }
                    
                }
                //timeLimitActive=1;
                dropoutIdx=-1;// default
                if(attackComplete==1){
                    attackComplete=0;

                    attkPoint =0; //after attack atk point 0
                }
                initializeSpecialOccastionList(specialOccasion,(int)(sizeof(specialOccasion)/sizeof(specialOccasion[0])));
                free(playerScreen);  

                // new count down start
                if(timeLimitActive==0){ // 1st skip 
                    timeLimitActive=1;
                }
                else if(timeLimitActive==1){ //activate from 2nd client input permanently
                    //reset to default and pass out turn
                    thread_data.clientNumber = turn;

                    printf("thread created countdown start\n");
                    sleep(2);
                    if(survivers >1){
            	    status=pthread_create(&timeLimit_thread, NULL, countdown,(void *)&thread_data);
                    if(status==-1){
            		    fprintf(stderr,"PThread  Creation Error \n");
            		    exit(0);
            	    }
                    pthread_detach(timeLimit_thread);
                    int res;
                    pthread_join(timeLimit_thread,(void**) &res); 
                    }
                    else{
                        printf("only one surviver left countdown disabled \n");
                    }
                }


            
                continue;
            }//if FD_ISSET

                
        }//for loop

        if(gameover>0){
            printf(" game over : %d active >>break while loop  \n",gameover);
            break;
        }

    }//while
    printf("while loop in room No. %d breaked \n",roomNum);
    printf("GAME OVER in room No. %d \n",roomNum);
    printf("memory free active\n");
    sleep(1.1);
    clientInputChecker[roomNum-1]=0;
    free(cardDeck);
    free(usedCardDeck);
    free(playerInfo);
    free(playerDeck);
    pthread_exit(NULL);

}// func create thread

/*
void sendDeckTurn(char information[],card currentDeck , int turn){
    char cardInfo[]="[";
    char turnInfo[4];
    strcpy(information,cardInfo);
    cardInfo[0]=currentDeck.cardType;
    strcat(information,cardInfo);
    cardInfo[0]=currentDeck.cardStyle;
    strcat(information,cardInfo);
    
    //cardInfo[0]='+';
    //strcat(information,cardInfo);
    //sprintf(turnInfo,"%d",turn);
    //strcat(information,turnInfo);
    
    strcat(information,"]\n"); //ex) [card]\n
    return;
}
*/

void unproperPlayer(int unproperPlayerIdx,userInfo *playerInfo){ //disquealified of reading history data 
    playerInfo[unproperPlayerIdx].disquealified=1;
}

void deleteCurrnetPlayer(int clientNumber){

    for(int i=0;i<userlimit;i++){
        if(currentPlayers[i]== clientNumber){
            currentPlayers[i]=0; //disable
            break; 
        }
    }
}

void addSpecialOccastionMessage(char *playerScreen,int styleIdx,char *inputClient,int attkPoint){

    /*
    if(styleIdx==-1){ // no special style 
        return;
    }
    */
    char ATKPoint[4];
    sprintf(ATKPoint,"%d",attkPoint);

    char specialOcassionMessage[200];
    switch (styleIdx){
        case 0:
            strcpy(specialOcassionMessage,"Jump from player : ");
            strcat(specialOcassionMessage, inputClient);
            break;

        case 1:
            strcpy(specialOcassionMessage,"extra turn for player : ");
            strcat(specialOcassionMessage, inputClient);
            break;            
        case 2:    
            strcpy(specialOcassionMessage,"turn table reverse from player : ");
            strcat(specialOcassionMessage, inputClient);
            break;
        case 3:            
            strcpy(specialOcassionMessage,"Attack +2 from player : ");
            strcat(specialOcassionMessage, inputClient);
            strcat(specialOcassionMessage," (total ATK point : ");
            strcat(specialOcassionMessage,ATKPoint);
            strcat(specialOcassionMessage," )");
            break;
        case 4:            
            strcpy(specialOcassionMessage,"Attack +3 from player : ");
            strcat(specialOcassionMessage, inputClient);
            strcat(specialOcassionMessage," (total ATK point : ");
            strcat(specialOcassionMessage,ATKPoint);
            strcat(specialOcassionMessage," )");
            break;            
        case 9:
            strcpy(specialOcassionMessage,"player : ");
            strcat(specialOcassionMessage, inputClient);
            strcat(specialOcassionMessage," damaged ");
            strcat(specialOcassionMessage,ATKPoint);
            strcat(specialOcassionMessage," points");
            break;
        default:
            //printf("in switch special occastion >> Idx %d (5~8 is ultimate)\n ",styleIdx);
            break;

    }

    if(styleIdx>4 && styleIdx<9){
        if(styleIdx==5){
            strcpy(specialOcassionMessage,"[Red Ultimate] Attack +5 from player : ");
            strcat(specialOcassionMessage, inputClient);
            strcat(specialOcassionMessage," (total ATK point : ");
            strcat(specialOcassionMessage,ATKPoint);
            strcat(specialOcassionMessage," )");
        }
        else if(styleIdx==6){   
            strcpy(specialOcassionMessage,"[Green Ultimate] Reflection from player : ");
            strcat(specialOcassionMessage, inputClient);
            if(attkPoint>0){
                strcat(specialOcassionMessage," (total ATK point : ");
                strcat(specialOcassionMessage,ATKPoint);
                strcat(specialOcassionMessage," )");                
            }            
        }
        else if(styleIdx==7){    
            strcpy(specialOcassionMessage,"[Blue Ultimate] Block from player : ");
            strcat(specialOcassionMessage, inputClient);
            strcat(specialOcassionMessage, " ATK point disabled ");
        }
        else if(styleIdx==8){    
            strcpy(specialOcassionMessage,"[Yellow Ultimate] Dodge from player : ");
            strcat(specialOcassionMessage, inputClient);
            if(attkPoint>0){
                strcat(specialOcassionMessage," (total ATK point : ");
                strcat(specialOcassionMessage,ATKPoint);
                strcat(specialOcassionMessage," )");                
            }
        }
    }


    strcat(specialOcassionMessage,"\n");
    strcat(playerScreen,specialOcassionMessage);
}

int surviverCheck(userInfo *playerInfo,int playerCount){
    for(int i=0;i<playerCount;i++){
        if(playerInfo[i].cardCount !=-1){
            return i; //return surviver idx
        }   
    }
}

int playerIdxFinder(userInfo *playerInfo, int client,int playerCount){
    for(int i=0;i<playerCount;i++){
        if(playerInfo[i].userNumber==client){
            return i; //return client userInfo idx
        }   
    }
}

void playerEraser(userInfo *playerInfo, int clientIdx,int playerCount,card *cardDeck){
    int playerIdx=clientIdx;
    //playerInfo[playerIdx].userNumber = -1; //needed for screen maker

    int cardNeedToRetrive=playerInfo[playerIdx].cardCount;
    int retreivedCard=0;
    int cardIdx=0;
    while(retreivedCard<cardNeedToRetrive){//retrieve all card from client
        if(playerInfo[playerIdx].cardDeck[cardIdx].used==0){
            takeCardFromClient(cardIdx,&playerInfo[playerIdx],cardDeck);
            retreivedCard++;
        }
        else{
            cardIdx++;
        }
    }
    playerInfo[playerIdx].cardCount=-1;// needed to print dead from screen maker
    FD_CLR(playerInfo[playerIdx].userNumber,&masterCard);
    //CLOSESOCKET(client);

}

void initializeSpecialOccastionList(int *specialOccasion,int sizeofList){
    int specialOccastionTypeCount =sizeofList; 
    for(int i=0;i<specialOccastionTypeCount;i++){
        specialOccasion[i]=0;
    }
}

void turnTableReverse(userInfo *playerInfo,int playerCount){
    int reverseCount = (int)(playerCount/2);
    userInfo tmp;
    for(int i=0;i<reverseCount;i++){
        tmp =playerInfo[i];
        playerInfo[i]=playerInfo[(playerCount-1)-i];
        playerInfo[(playerCount-1)-i]=tmp;       
    }
}

void playerScreenMaker(char **playerScreen,userInfo *playerInfo,int playerCount,char commonScreen[]){

    for(int i=0; i<playerCount;i++){
        if(playerInfo[i].cardCount!=-1){
            strcpy(playerScreen[i],commonScreen);
            printCard(playerScreen[i],playerInfo[i]);
            for(int j=0;j<playerCount;j++){
                if(i!=j){

                    blindCard(playerScreen[i],playerInfo[j]);
                }
            }
        }
    }
}

void cardsLeftInDeck(char commonScreen[],char cardLeft[]){
                strcpy(commonScreen,"<Stranger Cards>\n\n");
                strcat(commonScreen,"Cards left in deck : ");
                strcat(commonScreen,cardLeft);
                strcat(commonScreen,"\n\n");    
}
void currnetCardInDeck(char outputScreen[],card currentDeckCard){

    strcat(outputScreen,"current deck card is :");
    cardToScreen(outputScreen,currentDeckCard);
    strcat(outputScreen,"\n");
}

void cardToScreen(char outputScreen[],card currentCard){

    char type[2]="\0";
    char style[2]="\0";
    type[0]=currentCard.cardType;
    style[0]=currentCard.cardStyle;

    strcat(outputScreen," [ ");
    strcat(outputScreen,type);
    strcat(outputScreen,style);
    strcat(outputScreen," ] "); //_[ ?? ]_
}
void printCard(char outputScreen[],userInfo playerInfo){
    
    strcat(outputScreen,"=================\n");
    strcat(outputScreen,playerInfo.username);
    strcat(outputScreen," : ");    

    for(int i=0;i<MAX_CARD;i++){
        //if card exists print
        if(playerInfo.cardDeck[i].used !=(-1)){
            cardToScreen(outputScreen,playerInfo.cardDeck[i]);
        }
    }
    strcat(outputScreen,"\n");

}
void blindCard(char outputScreen[],userInfo playerInfo){

    strcat(outputScreen,"=================\n");
    strcat(outputScreen,playerInfo.username);
    strcat(outputScreen," : ");
    
    if(playerInfo.cardCount!=-1){
        for(int i=0;i<playerInfo.cardCount;i++){
            strcat(outputScreen," [ ?? ] ");
        }
        strcat(outputScreen,"\n");
    }
    else{
        strcat(outputScreen," R.I.P :( \n");
    }
}

void takeCardFromClient(int cardPosition, userInfo *playerInfo, card *cardDeck){
    printf("receving %c%c from player \n",playerInfo->cardDeck[cardPosition].cardType,playerInfo->cardDeck[cardPosition].cardStyle);
    
    for(int i=0;i<totalCardType*totalCardStyle;i++){
        if((cardDeck[i].cardType==playerInfo->cardDeck[cardPosition].cardType)&&(cardDeck[i].cardStyle==playerInfo->cardDeck[cardPosition].cardStyle)){
            cardDeck[i].used =2;// returned
        }
    }
    
    playerInfo->cardDeck[cardPosition].cardType='x';// doesn't mean anything //just in case
    playerInfo->cardDeck[cardPosition].cardStyle='x';
    playerInfo->cardDeck[cardPosition].used= -1; //does not exist
    playerInfo->cardCount--;
}

int changeTurn(userInfo *playerInfo,int currentTurn,int playerCount){
    printf("Prev turn : %d \n",currentTurn);
    for(int i=0;i<playerlimit;i++){
        if (playerInfo[i].userNumber==currentTurn)
        {   
            while(1){
                i++;
                if(i==playerCount)
                    i=0;
                if(playerInfo[i].cardCount == -1) // user does not exist
                    continue;

                printf("Turn changed >> %d \n",playerInfo[i].userNumber);
                return playerInfo[i].userNumber;
                
            }
        }
        
    }
}

int checkClientCardExist(char inputCard[], card *playerDeck ){

    for (int i=0;i<MAX_CARD;i++){
        if(playerDeck[i].used ==0){ // this space has card
            if((inputCard[0]==playerDeck[i].cardType)&&(inputCard[1]==playerDeck[i].cardStyle))
                return i; //exist return position of selected card
            else
                continue;
        }
    }
    return -1; //card not found
}


int checkClientCardValid(char inputCard[], card currentDeck,int attkPoint){

    if(inputCard[0]=='Y' && inputCard[1]=='q'){
        return 1; //dodge always possible
    }

    if(attkPoint==0){
        if((inputCard[0]==currentDeck.cardType)||(inputCard[1]==currentDeck.cardStyle)){
            return 1;
        }
        else
            return 0;
    }
    else if(attkPoint>0){ // attk active
        if((inputCard[1]=='q')&&(currentDeck.cardStyle =='a'||currentDeck.cardStyle=='s')){
            return 1; // q> a s  attk active
        }
        else if((inputCard[1]=='s')&&(currentDeck.cardStyle=='a')){
            return 1; // s> a   attk active
        }

        if((inputCard[1]=='a')&&(currentDeck.cardStyle=='s')){
            return 0;
        }       // a cannot beat s 

        if((inputCard[1]=='a' ||(inputCard[1]=='s'))&&(currentDeck.cardStyle=='q')){
            if(currentDeck.cardType == 'R')
                return 0; // rq is invincible
            else if(inputCard[0]==currentDeck.cardType){
                return 1; // other bq gq yq attk still active possible
            }
            else{
                return 0; // no match
            }
        }        

        if(isdigit(inputCard[1])){ // 1~9 is not valid in attk 
            return 0;
        }
        else if(inputCard[1]!='q' && inputCard[1] != 's' && inputCard[1]!='a'){ // if not ulti or attk card
            return 0;
        }
    }
}

    
        
    

int cardDeckCounter(card *cardDeck){
    int nullCount=0;
    while(1){


        if(cardDeck[nullCount].used >0){ // 1 or 2
            nullCount++;
        }
        else{
            break;
        }
        /*
        if(nullCount==totalCardStyle*totalCardType){
            shuffleCard(cardDeck,1);
            break;    
        }
        */
    }
    return nullCount;
}

void sendCardToClient(int cardCount, userInfo *playerInfo, card *cardDeck){
    //cardcount: ammount of card to send // playerinfo :target //cardDeck : deck 
    int nullCount=0;

    nullCount = cardDeckCounter(cardDeck);

    int checkpoint=0;
    for(int i=0;i<cardCount;i++){ // how many cards to sendZ
        for(int j=checkpoint;j<MAX_CARD;j++){
            if(playerInfo->cardDeck[j].used== -1){
                playerInfo->cardDeck[j]=cardDeck[nullCount];
                cardDeck[nullCount].used =1;//gave card to client >>used
                playerInfo->cardCount=(playerInfo->cardCount)+1;
                nullCount++;
                if(nullCount>=totalCardStyle*totalCardType){
                    printf("cardDeck empty reshuffle active\n");
                    shuffleCard(cardDeck,1);
                    //checkpoint=0;
                    nullCount=cardDeckCounter(cardDeck); // deck initialized
                }
                else{
                checkpoint=j+1;// not to check from the start
                }
                break;
            }
        }
    }
}

int checkCardCount(int cardCount, userInfo playerinfo){
    if((playerinfo.cardCount)+cardCount>MAX_CARD)
        return 0;//DEAD
    else{
        playerinfo.cardCount=(playerinfo.cardCount)+cardCount;
        return 1; // possible
    }
    
}

void storePlayerRoomInfo(int players[], userInfo *playerinfo){
    int playerCount =0;
    for(int i=0;i<playerlimit;i++){
        
        if(players[i]!= -1)
            playerCount++;
    }

    for(int i=0;i<playerCount;i++){
        for(int j=0;j<userlimit;j++){
            if(currentUsers[j].userNumber==players[i]){
                playerinfo[i].userNumber=currentUsers[j].userNumber;
                playerinfo[i].username=malloc(sizeof(char)*(strlen(currentUsers[j].username)+1));
                strcpy(playerinfo[i].username,currentUsers[j].username);
                playerinfo[i].cardCount=0;  //default 0 cards
                playerinfo[i].warning=0;//default no warning
                playerinfo[i].disquealified=0;
                break;
            }
        }
    }
    return;
}

void shuffleCard(card* cardDeck,int reshuffle){
    srand(time(NULL));

    card tmp;
    int shuffleCount = 100;
    int pickNum;
    int pickNum2;
    int unreturnedCard=totalCardStyle*totalCardType;
    
    
    
    for(int i=0;i<(totalCardStyle*totalCardType);i++){
        if(cardDeck[i].used==2){//card returned from client
            cardDeck[i].used = 0;
            unreturnedCard--;
        }
    }
   

    for(int i=0;i<shuffleCount;i++){
        pickNum=random()%(totalCardStyle*totalCardType);
        //printf("pickNum : %d \n",pickNum);
        while(1){
            pickNum2=random()%(totalCardStyle*totalCardType);
            if(pickNum2==pickNum)
                continue;
            else
                break;
        }
        //printf("pickNum2 : %d \n",pickNum2);
        tmp = cardDeck[pickNum];
        cardDeck[pickNum]=cardDeck[pickNum2];
        cardDeck[pickNum2]=tmp;

    }


    if(reshuffle){
        //organizing 1 and 0  111111111100000000 is what i want
        //oreder in decending order
        for(int i=0; i<totalCardStyle*totalCardType;i++){
            for(int j=i+1;j<totalCardStyle*totalCardType;j++){
                if(cardDeck[i].used<cardDeck[j].used){
                    tmp = cardDeck[i];
                    cardDeck[i]=cardDeck[j];
                    cardDeck[j]=tmp;
                }
            }
        }

    /*
        for(int i=totalCardType*totalCardStyle-1;i>-1;i--){ // from end to start
            if(cardDeck[i].used==2){
                cardDeck[i].used =0;
                continue;// skip 2 
            }
            else{ //100% 1
                for(int j=searchPoint;j<i;j++){ //from start to end
                    if(cardDeck[j].used==1){
                        searchPoint++;
                        continue;
                    }
                    else{//100% 0
                        tmp=cardDeck[i];
                        cardDeck[i]=cardDeck[j];
                        cardDeck[j]=tmp;
                        searchPoint++;
                        resortCount++;
                        if(resortCount==unreturnedCard){
                            printf("card reshuffle complete >> cardDeck ready\n");
                            return;
                        }
                    } //exchange back 1 and front 2
                }
            }
        }
    */
    }

    printf("card shuffle complete >> cardDeck ready\n");
}
/*
void signalDetection(int sig){
    if(sig ==SIGTERM){
    //printf("signal received.. termainating countdown..");
    printf("signal received.. \n");
    start=1;
    return;
    }   
}
*/
void screenMakerForServer(char *cardgameOneTurnData,userInfo *playerInfo,int playerCount){
    // after cardsLeftInDeck();
    // after currentDeck();
    for(int i=0; i<playerCount;i++){
        if(playerInfo[i].cardCount!=-1){
            printCard(cardgameOneTurnData,playerInfo[i]);
        }
        else{
            strcat(cardgameOneTurnData,"=================\n");
            strcat(cardgameOneTurnData,playerInfo[i].username);
            strcat(cardgameOneTurnData," : ");
            strcat(cardgameOneTurnData," R.I.P :( \n");
        }
    }
    //before addspecialOccastionMessage();
}

void copyHistoryFromServerToClient(char *username,int serverDataNumber){
    FILE *fptr;
    char serverPath[30];
    char clientPath[50];
    char numberToString[4];
    
    //server side
    strcpy(serverPath,"./CARDGAME_DATA/");   // ./CARDGAME_DATA/filenumber
    sprintf(numberToString,"%d",serverDataNumber);
    strcat(serverPath,numberToString);  

    //client side
    

    strcpy(clientPath,"./USER_DATA/"); // ./USER_DATA/username/history/filename
    strcat(clientPath,username);
    strcat(clientPath,"/history");
    int clientHistoryFileCount = countHistoryFile(clientPath) ; 

    if(clientHistoryFileCount == MAX_CLIENT_HISTORY_DATA){
        printf("client storage full \n");
        return ;
    }
    strcat(clientPath,"/");
    sprintf(numberToString,"%d",clientHistoryFileCount);
    strcat(clientPath,numberToString);

    char copyCommand[40];
    strcpy(copyCommand,"cp "); // cp serverData clientData(renamed)
    strcat(copyCommand,serverPath);
    strcat(copyCommand," ");
    strcat(copyCommand,clientPath);
    system(copyCommand);

    printf("copy server data to client dir complete \n");
    return ;    
}

int countHistoryFile(char *defaultPath){

    DIR *directory;
    struct dirent *dir;
    int fileNum=0;
    directory = opendir((const char *)defaultPath);// CARDGAME_DATA or USER_DATA

    if(directory){
        while((dir = readdir(directory))!= NULL){
            fileNum++;
        }
    }
    else{
        printf("FATAL ERROR DIR NOT FOUND\n");
        exit(1);
    }
    return fileNum -1; // always start with 2
    /* old code
    FILE *fptr;
    char path[30];
    int fileNum=0;
    char numberToString[4];
    while(1){
        strcpy(path,defaultPath);
        sprintf(numberToString,"%d",fileNum);
        strcat(path,numberToString);
        
        if(access((const char*)path, R_OK|W_OK)){
            //file exist
            fileNum++;
            continue;
        }
        else{
            fclose(fptr);
            return fileNum;
        }
    } 
    */   
}

int createHistoryFileInServer(){
    FILE *fptr;
    int fileNum;
    char path[30];
    char numberToString[4];

    fileNum=countHistoryFile("./CARDGAME_DATA");
    strcpy(path,"./CARDGAME_DATA/");   // defaultPath/filenumber
    sprintf(numberToString,"%d",fileNum);
    strcat(path,numberToString);   

    
    fptr = fopen(path,"w");
    if(fptr==NULL){
        printf("file >> %s creation error \n",path);
        exit(1);
    }
    printf("history file >> %s is created..\n",path);
    char fileMode[]="0744";
    int mode ;
    mode =strtol(fileMode,0,8);
    if(chmod(path,mode)<0){
        printf("FATAL ERROR in changing mode\n");
        printf("mode change failed\n");
        exit(1);
    }
    fclose(fptr);
    return fileNum;            
        
    
}

void storeGameHistoryServer(int fileNumber,char *cardgameOneTurnData){
    FILE *fptr;
    char path[30];
    char numberToString[4];
    strcpy(path,"./CARDGAME_DATA/");
    sprintf(numberToString,"%d",fileNumber);
    strcat(path,numberToString);    

    fptr = fopen(path,"a");
    if(fptr==NULL){
        printf("file >> %s open error \n",path);
        return ;
    }


    fputs(cardgameOneTurnData,fptr);
    fputs("\n#\n",fptr); // # is mark of one turn
    
    fclose(fptr);
    printf("Save complete! \n");
    return ;

}
void readGameHistory(char *filename,int clientNumber){
    FILE *fptr;
    int clientIdx = clientIdxFinder(clientNumber);
    char path[50];
    strcpy(path,"./USER_DATA/"); // ./USER_DATA/username/history/filename
    strcat(path,currentUsers[clientIdx].username);
    strcat(path,"/history/");
    strcat(path,filename);

    fptr = fopen(path,"r");
    if(fptr==NULL){
        printf("file >> %s open error \n",path);
        return ;
    }
    char word[2]="\0";
    char turnData[1024];
    char singleWord;
    int wordCount=0;

    while(singleWord!=EOF){
        singleWord=fgetc(fptr);
        word[0]=singleWord;
        
        if(word[0]=='#'){
            word[0]='\0'; // cut
            strcat(turnData,word);

            printf("send %d bytes to client %d\n",wordCount,clientNumber);
            send(clientNumber,turnData, strlen(turnData), 0);
            wordCount=0;
            sleep(3.3);
            continue;
        }
        else{
            if(wordCount==0){
                strcpy(turnData,word);
                wordCount++;
                continue;
            }

            strcat(turnData,word);
            wordCount++;
        }
    }    
    fclose(fptr);
    char readComplete[]="(Read Complete)\n";
    send(clientNumber,readComplete, strlen(readComplete), 0);

    printf("read complete! \n");
    return ;

}

int findDirectory(char username[]){

    DIR* dir;
    char userData[]="./USER_DATA";
    char path[12+strlen(username)+1];//./USER_DATA/+ strlen(username)+null
    
    printf("searching path... \n");
    strcpy(path,userData);
    strcat(path,"/");
    strcat(path,username);
    printf("searching dir : %s \n",path);

    dir= opendir(path);
    if(dir){
        printf("dir : %s found\n",path);
        return 1;
    }
    else{
        printf("dir : %s does not exist\n",path);
        return 0;
    }

}
void createDirectory(char username[]){

    char userData[]="./USER_DATA";
    char historyDir[]="/history";
    char path[12+strlen(username)+8+1];//./USER_DATA/+ strlen(username)+/history+null
    strcpy(path,userData);
    strcat(path,"/");
    strcat(path,username);
    
    int dirCreate=0;
    dirCreate=mkdir(path,0744);
    if(dirCreate==0){
        printf("DIR_PATH >> %s created\n",path);
        strcat(path,historyDir);
        dirCreate=mkdir(path,0744);
        if(dirCreate==0){
            
            printf("HISORY DIR_PATH >> %s created\n",path);
            return ;
        }
        else{
            printf("HISORY DIR_PATH >> %s creation error..\n",path);
        return ;
    }
    }
    else{
        printf("DIR_PATH >> %s creation error..\n",path);
        return ;
    }

}
void deleteDirectory(int clientCode){

    int idx=clientIdxFinder(clientCode);
    char username[strlen(currentUsers[idx].username)+1];
    strcpy(username,currentUsers[idx].username);

    char userData[]="./USER_DATA";
    char path[12+strlen(username)+1];//./USER_DATA/+ strlen(username)+null
    strcpy(path,userData);
    strcat(path,"/");
    strcat(path,username);
    
    char deleteCommand[7+strlen(path)+1];//"rm -rf "+ path + null
    strcpy(deleteCommand,"rm -rf ");
    strcat(deleteCommand,path);
    system(deleteCommand);
    printf("%s >> file deleted \n",path);
    return ;

}
int createAccount(char username[],char password[]){
    FILE *fptr;
    char userData[]="./USER_DATA";
    char loginFile[]="";
    char path[12+strlen(username)+1+5+1];//./USER_DATA/+ strlen(username)+/+login+null
    strcpy(path,userData);
    strcat(path,"/");
    strcat(path,username);
    strcat(path,"/");
    strcat(path,"login");

    fptr = fopen(path,"w");
    if(fptr==NULL){
        printf("file >> %s creation error \n",path);
        return 0;
    }
    printf("file >> %s is created..\n",path);
    printf("store data in file %s \n",path);

    char fileMode[]="0744";
    int mode ;
    mode =strtol(fileMode,0,8);
    if(chmod(path,mode)<0){
        printf("FATAL ERROR in changing mode\n");
        printf("mode change failed\n");
        exit(1);
    }
    

    fputs(username,fptr);
    fputs(",",fptr);
    fputs(password,fptr);
    fputs("\n",fptr);
    fclose(fptr);
    
    printf("Save complete! \n");
    
    return 1;

}
int saveUserInfo(char *receviedData ,int dataLength){
    int bytes_received=dataLength;
    int divisionPos=0;
    char word[2]="\0";//for using strcmp and strcpy
    printf("Extracting information from receivedData .. \n");
    for(int i=3;i<bytes_received-4;i++){
        if(receviedData[i]=='+'){
            divisionPos=i;
            break;
        }
    }
    char username[divisionPos-3+1];//!^!erase + null
    word[0]=receviedData[3];
    strcpy(username,word);
    for(int i=4;i<divisionPos;i++){
        word[0]=receviedData[i];
        strcat(username,word);
    }
    char password[bytes_received-(divisionPos+1)-4+1];//erase ~ divpos !^!\\n + null 
    word[0]=receviedData[divisionPos+1];
    strcpy(password,word);
    for(int i=divisionPos+2;i<bytes_received-4;i++){
        word[0]=receviedData[i];
        strcat(password,word);
    }
    int findDir=0;
    printf("Extraction complete..\n");
    printf("ID : %s \nPW: : %s \n",username,password);
    findDir = findDirectory(username);// if found 1 /not 0
    

    if(findDir==0){
        printf("%s >> directory creating process activated.. \n",username);
        createDirectory(username);
        createAccount(username,password);
        return 1;
    }
    else{
        printf("Client name : %s already exists.. \n",username);
        return 0;
    }


}
int loginCheck(char receviedData[] ,int dataLength){

    int bytes_received=dataLength;
    int divisionPos=0;
    char word[2]="\0";//for using strcmp and strcpy
    printf("Extracting information from receivedData .. \n");
    for(int i=3;i<bytes_received-4;i++){
        if(receviedData[i]=='+'){
            divisionPos=i;
            break;
        }
    }
    char username[divisionPos-3+1];//!^!erase + null
    word[0]=receviedData[3];
    strcpy(username,word);
    for(int i=4;i<divisionPos;i++){
        word[0]=receviedData[i];
        strcat(username,word);
    }
    char password[bytes_received-(divisionPos+1)-4+1];//erase ~ divpos !^!\\n + null 
    word[0]=receviedData[divisionPos+1];
    strcpy(password,word);
    for(int i=divisionPos+2;i<bytes_received-4;i++){
        word[0]=receviedData[i];
        strcat(password,word);
    }
    int findDir=0;
    printf("Extraction complete..\n");
    printf("ID : %s \nPW: : %s \n",username,password);

    int dirFind=0;
    int loginConfirm=0;
    dirFind = findDirectory(username);
    if (dirFind==0)
        return 0; //error dir not found
    else{
        loginConfirm = checkLoginFile(username,password);//login :1 error :0 multiLogin :-1
        
        if(loginConfirm==1){
            storeUserinfo(username);
            printf("[%s] login sucess\n",username);
        }
        return loginConfirm;
    }
    


}
void storeUserinfo(char username[]){
    
    for(int i=0;i<userlimit;i++){
        if(currentUsers[i].userNumber==-1){
            currentUsers[i].username=malloc(sizeof(char)*(strlen(username)+1));
            strcpy(currentUsers[i].username,username);
            currentUsers[i].userNumber=clientCode;// static int >> get i(client code) in for loop
            printf("%s stored in struct idx >> %d, usernumber : %d \n",currentUsers[i].username,i,currentUsers[i].userNumber);
            return;
        }
    }

}
void deleteUserinfo(int userNumber){
    for(int i=0;i<userlimit;i++){
        if(userNumber==currentUsers[i].userNumber){
            printf("%s data in struct idx >> %d, usernumber : %d  deleting..\n",currentUsers[i].username,i,currentUsers[i].userNumber);
            free(currentUsers[i].username);
            currentUsers[i].userNumber=-1;
            currentUsers[i].currnetRequest=-1;
            printf("deleted.. \n");
            return;
        }
    }
}
void storeUserPurpose(int userNumber,int purpose){
        for(int i=0;i<userlimit;i++){
        if(userNumber==currentUsers[i].userNumber){
            currentUsers[i].currnetRequest=purpose;
            return;
        }
    }
}
int checkLoginFile(char username[],char password[]){

    char newUser[strlen(username)+1];
    strcpy(newUser,username);
    for (int i=0; i<userlimit;i++){
        if(currentUsers[i].userNumber != -1){
            if(strcmp(currentUsers[i].username,username)==0){
                printf("username %s already logined \n",username);
                return -1;
            }
            
        }
        FILE *fptr;
        char userData[]="./USER_DATA";
        char loginFile[]="";
        char path[12+strlen(username)+1+5+1];//./USER_DATA/+ strlen(username)+/+login+null
        strcpy(path,userData);
        strcat(path,"/");
        strcat(path,username);
        strcat(path,"/");
        strcat(path,"login");

        fptr = fopen(path,"r");
        if(fptr==NULL){
            printf("FATAL ERROR \n");
            printf("file >> %s read error \n",path);
            return 0; // no file in dir 
        }

        char word[2]="\0";
        char fileData[20];
        char singleWord='a';
        int wordCount=0;
        int idMatch=0;

        while(singleWord!=EOF){
            singleWord=fgetc(fptr);
            word[0]=singleWord;

            if(word[0]=='\n' || word[0]==','){
                word[0]='\0';
                strcat(fileData,word); //add null
                char compareBox[strlen(username)+1];
                strcpy(compareBox,fileData);


                if(idMatch==0 &&strcmp(username,compareBox)==0){ //ID yet found 
                    printf("username : <%s> , fileID: <%s> match\n",username,fileData);
                    idMatch=1;
                    wordCount=0;
                    continue;
                }
                else if(idMatch==1 &&strcmp(password,compareBox)==0){// only activate when ID found
                    fclose(fptr);
                    printf("password match login process activate..\n");
                    return 1; //match ID and PW found
                }
                else{
                    fclose(fptr);
                    printf("Wrong PW from client\n");
                    return 0; //wring pw

                }
                wordCount=0;
                continue;
            }
            else{
                if(wordCount==0){
                    strcpy(fileData,word);
                    wordCount++;
                    continue;
                }
                strcat(fileData,word);
                wordCount++;
            } 

        }
        fclose(fptr);
        printf("FATAL ERROR in login flie \n");
        return 0; // if this activates whatever it is its an error

    }
}

//==================past====================//

/*
int loginCheck(char username[],char password[]){
    FILE *fptr;
    
    printf("opening file.. \n");
    fptr= fopen("./maybeTest","r");
    if(fptr==NULL){
        printf("FILE READ ERROR.. \n");
        exit(1);
    }

    int alreadyExist = 0;
    char word[2]="\0";
    char fileData[20];
    char secret[20];
    char singleWord='a';
    int wordCount=0;
    int idFound=0;
    int thisIsId=0; //check ID or PW

    while(singleWord!=EOF){
        singleWord=fgetc(fptr);
        word[0]=singleWord;

        if(word[0]=='\n' || word[0]==','){
            thisIsId++;
            if(((thisIsId%2)==0)||(idFound==1)){ 
            // if pw >>Do not check, but if ID is found >> check
                continue;
            
            }
            word[0]='\0';
            strcat(fileData,word); //add null
            char compareBox[strlen(username)+1];
            strcpy(compareBox,fileData);
            printf("user : <%s>, fileData : <%s> \n",username,compareBox);
            
            if(idFound==0 &&strcmp(username,compareBox)==0){ //ID yet found 
                printf("identical fileData found %s \n",fileData);
                idFound=1;
                wordCount=0;
                continue;
            }
            else if(idFound==1 &&strcmp(password,compareBox)){// only activate when ID found
                fclose(fptr)
                printf("client account found login process activate..\n")
                return 1; //match ID and PW found
            }
            else{
                fclose(fptr)
                printf("Wrong PW from client\n")
                return 0; //wring pw

            }
            wordCount=0;
            continue;
        }
        else{
            if(wordCount==0){
                strcpy(fileData,word);
                wordCount++;
                continue;
            }
            strcat(fileData,word);
            wordCount++;
        } 
        
    }
    printf("Wrong ID or PW from client\n");
    fclose(fPtr);
    return 0;
}
*/

/*  // storing data as txt is not convenient 
int saveData(char username[],char password[]){
    FILE *fptr;
    FILE *fptr_ID;
    printf("opening file.. \n");
    fptr= fopen("./maybeTest","a");
    if(fptr==NULL){
        printf("FILE READ ERROR.. \n");
        exit(1);
    }

    int alreadyExist = 0;
    alreadyExist = checkIdData(username);

    if(alreadyExist  == 1){
        fclose(fptr);
        return 0; // not stored
    }
    // store 
    printf("ID PW store process activate.. \n");

    fputs(username,fptr);
    fputs(",",fptr);
    fputs(password,fptr);
    fputs("\n",fptr);

    printf("store process completed.. \n\n");
    
    printf("Noticing ID_DATA.. \n");
    fptr_ID=fopen("./ID_DATA","a");
    fputs(username,fptr_ID);
    fputs("\n",fptr_ID);
    printf("ID_DATA rewrite complete\n");

    //fptr=freopen("./maybeTest","r",fptr);
    //printf("reopening file... \n");

    fclose(fptr);
    fclose(fptr_ID);
    return 1; //stored
}
*/
/*
int checkIdData(char username[]){
    FILE *fPtr;
    printf("Checking data in ID_DATA.. \n");
    fPtr= fopen("./ID_DATA","r");
    if(fPtr==NULL){
        printf("FILE READ ERROR.. \n");
        exit(1);
    }
    char word[2]="\0";
    int wordCount=0;
    char name[20];
    char singleWord='a';

    while(singleWord!=EOF){
        singleWord=fgetc(fPtr);
        word[0]=singleWord;
        //printf("%c",word[0]);
        if(word[0]=='\n'){
            word[0]='\0';
            strcat(name,word); //add null

            char compareBox[strlen(username)+1];
            strcpy(compareBox,name);
            printf("user : <%s>, name : <%s> \n",username,compareBox);
            
            if(strcmp(username,compareBox)==0){
                printf("identical name found %s \n",name);
                fclose(fPtr);
                return 1;
            }
            else{
                wordCount=0;
                continue;
            }
        }
        else{
            if(wordCount==0){
                strcpy(name,word);
                wordCount++;
                continue;
            }
            strcat(name,word);
            wordCount++;
        } 
        
    }
    printf("identical ID data not found.. \n");
    fclose(fPtr);
    return 0;
}

*/


