/*
 * MIT License
 *
 * Copyright (c) 2018 Lewis Van Winkle
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
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
#define SEM_NAME "/Time"

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
int changeTurn(int players[],int currentTurn);


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
const int MAX_CARD=15;
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
    int playingStatus;
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
    char cardStyle;// 1~9 p (plus) j (jump) d (direction) a (attk2) s (attk3) q (ultimate attk)
    int attack;// .0 .2 .3
    int ultimate; //.0 .1
    int used; // .-1 .0 .1 // -1 means "this space does not have card"
}card;


typedef struct{
    int userNumber;
    char *username;
    int cardCount;
    card *cardDeck;
}userInfo;


void playerScreenMaker(char **playerScreen,userInfo *player,int playerCount,char commonScreen[]);
void currnetCardInDeck(char outputScreen[],card currentDeckCard);
void blindCard(char outputScreen[],userInfo playerInfo);
void printCard(char outputScreen[],userInfo playerInfo);
void cardToScreen(char outputScreen[],card currentCard);
void cardsLeftInDeck(char commonScreen[],char cardLeft[]);
int cardDeckCounter(card *cardDeck);

void sendCardToClient(int cardCount,userInfo *playerInfo, card *cardDeck);
int checkCardCount(int cardCount, userInfo playerinfo);
void storePlayerRoomInfo(int players[], userInfo *playerinfo);
void shuffleCard(card* cardDeck);
static card *totalCard;
const static int totalCardType=4;
const static int totalCardStyle=15;



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
        currentUsers[i].playingStatus=0;
        currentUsers[i].room=-1;
        waitingLine[i]=-1;
        //printf("[%d] num is : %d\n",i,currentUsers[i].userNumber); test code
    }



    //int specialRequestClient[userlimit];
    int maxRoom=5;
    pthread_t cardRoom[maxRoom];
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
                    
                    if(connectedClient==playerlimit){
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
                    /*
                    playerSetStore(players,socket_client);
                    if(totalWaitingPlayer==0)
                        turn=players[0];
                        */
                    connectedClient++;
                    printf("current number of connected client : %d \n",connectedClient);
                    //ask purpose of connection
                    send(socket_client, askPurpose,strlen(askPurpose), 0);
/*
                    if(totalWaitingPlayer==1){
                        printf("count start \n");
                        int status;
                    	status=pthread_create(&thread_id, NULL, waitingTimeOut, (void *)socket_client);
                        if(status==-1){
                    		fprintf(stderr,"PThread  Creation Error \n");
                    		exit(0);
                    	}
                        pthread_detach(thread_id);

                    }
                    else if(totalWaitingPlayer>1){
                        printf("sending  signal to pthread.. \n");
                        signal(SIGCONT,signalDetection);
                        pthread_kill(thread_id,SIGCONT);
                        printf("count down terminated\n");
                    }
                    
*/

                } 
                else {

                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);
                    int request=0;


                    //client_exit
                    if (bytes_received < 1) { 
                        printf("exit detected.. \n");
                        FD_CLR(i, &master);
                        /*
                        if (turn==i){
                            turn=changeTurn(players,turn);
                        }
                    
                        playerSetDelete(players, i);
                        */
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
                    
                    if(checkClientPlayingCard(i)){
                        //printf("client %d is playing card.. message ignored..\n",i);
                        continue; //ignore client playing card
                    }
                    
                    
                    clientCode=i;
                    
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
                            char pwRequest[]= "<searching for other players>\n";
                            storeUserPurpose(clientCode,purpose);
                            send(i,pwRequest, strlen(pwRequest), 0);
                            
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
                            
                            char pwRequest[]= "=? Are you sure? y/n ?=\n";
                            storeUserPurpose(clientCode,purpose);
                            send(i,pwRequest, strlen(pwRequest), 0);
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
                                start=1;//countdown end
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
                            start=1;//countdown end
                            printf("CountDown disabled...\n");
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
                                start=1;//countdown end
                                
                                loginResult=loginCheck(read ,bytes_received);
                                if(loginResult){
                                    char loginComplete[]="#!Message from server : login success!! welcome!!#\n";
                                    send(i,loginComplete, strlen(loginComplete), 0);
                                }
                                else{
                                    char loginError[]="##Message from server : wrong ID or PW ##";
                                    send(i,loginError, strlen(loginError), 0);
                                }
                                purpose=-1;//situation disabled
                                continue;
                            }
                            printf("client %d is connecting for login\n",i);
                            start=1;//countdown end
                            printf("CountDown disabled...\n");
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
                currentUsers[i].playingStatus=1;
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
                printf("sending  signal to pthread.. \n");
                signal(SIGCONT,signalDetection);
                //pthread_kill(thread_id,SIGCONT);
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

}/*

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

void *waitingTimeOut(){
    int waitingTime=10;
    int addTime=5;
    int lonePlayer;
    int addTimecount=0;
    char failMessage[]="<# Message from server : match failed #>\n";
    char successMessage[]="< match success game will start soon... >\n";
	for(int i=0; i < waitingTime; i++){
        if(totalWaitingPlayer==4){
            printf("client count >> %d / Waitng Queue full\n",totalWaitingPlayer);
            break;
        }
        printf("current waitng client >> %d \n",totalWaitingPlayer);
        printf("%d seconds before closing waiting room.. \n",waitingTime-i);
        sleep(1);
        if((start==1) &&(totalWaitingPlayer==2)&&(addTimecount==0)){
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
    int players[4];
    int *room=(int *)roomNumber;
    int roomNum = *room;
    int playerCount=0;
    int turn =-1;
    int initialCard=6;
    
    SOCKET max_socket=-1;
    
    card currentDeck;
    card clientInput;

    card *cardDeck = malloc((sizeof(card))*(totalCardStyle*totalCardType));
    memcpy(cardDeck,totalCard,(sizeof(*totalCard))*(totalCardStyle*totalCardType));
    
    card *usedCardDeck= malloc((sizeof(card))*(totalCardStyle*totalCardType));
    
    shuffleCard(cardDeck);

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


    FD_ZERO(&masterCard);

    for(int i=0;i<playerlimit;i++){
        players[i] = playerSets[i];
        if(players[i]!=-1){
            FD_SET(players[i],&masterCard);
            if(turn ==-1){
                turn = players[i];
                printf("first turn is %d in room %d \n",turn,roomNum);
            }
            if(max_socket<players[i]){
                max_socket=players[i];
            }
            printf("client %d entered in room %d \n",players[i],roomNum);
            playerCount++;
        }
    }

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



    while(1){
        //masterCard is global
        if (select(max_socket+1, &masterCard, 0, 0, 0) < 0) {
            fprintf(stderr, "In thread select() failed. (%d)\n", GETSOCKETERRNO());
            exit(1);
        }

        for(int i=1;i<=max_socket;i++){
            if (FD_ISSET(i, &masterCard)) {
                // if not turn ignore 
                // -> if item effect first and continue turn doesn't change

                char read[15];
                int bytes_received = recv(i, read, 15, 0);
                /*
                if(turn!=i){ //ignore client if not turn
                    //printf("current turn client>> %d \n",turn);
                    continue;
                }
                */
                char commonScreen[100];
                char currentDeckCard[20];
                char **playerScreen=(char**)malloc((sizeof(char*))*playerCount);
                for(int m=0;m<playerCount;m++){
                    playerScreen[m]=(char*)malloc(sizeof(char)*1024);
                }
                
                // count cardDeck left
                int nullCount=0;
                int cardDeckLeft=0;
                char cardLeft[3];
                nullCount = cardDeckCounter(cardDeck);
                cardDeckLeft = totalCardStyle*totalCardType -nullCount;
                sprintf(cardLeft,"%d",cardDeckLeft);

                cardsLeftInDeck(commonScreen,cardLeft);
                currnetCardInDeck(commonScreen,currentDeck);
                playerScreenMaker(playerScreen,playerInfo,playerCount,commonScreen);

                SOCKET j; // send message to others
                
                for (int j = 0; j <playerCount; j++) {
                    
                    send(players[j],playerScreen[j], strlen(playerScreen[j]), 0);
                    
                    
                }
                free(playerScreen);                
                
            }//if FD_ISSET

                continue;
        }//for loop
    }//while
}// func create thread

void playerScreenMaker(char **playerScreen,userInfo *player,int playerCount,char commonScreen[]){

    for(int i=0; i<playerCount;i++){
        strcpy(playerScreen[i],commonScreen);
        printCard(playerScreen[i],player[i]);
        for(int j=0;j<playerCount;j++){
            if(i!=j){
                
                blindCard(playerScreen[i],player[j]);
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

    for(int i=0;i<playerInfo.cardCount;i++){
        strcat(outputScreen," [ ?? ] ");
    }
    strcat(outputScreen,"\n");

}

void takeCardFromClient(int cardPosition, card *playerDeck ){
    printf("receving %c%c from player \n",playerDeck[cardPosition].cardType,playerDeck[cardPosition].cardStyle);
    playerDeck[cardPosition].cardType='x';
    playerDeck[cardPosition].cardStyle='x';
    playerDeck[cardPosition].used= -1; //does not exist
}

int changeTurn(int *players,int currentTurn){
    printf("Prev turn : %d \n",currentTurn);
    for(int i=0;i<playerlimit;i++){
        if (players[i]==currentTurn)
        {   
            while(1){
                i++;
                if(i==playerlimit)
                    i=0;
                if(players[i]==-1)
                    continue;
                else{
                    printf("Turn changed >> %d \n",players[i]);
                    return players[i];
                }
            }
        }
        
    }
}

int checkClientCardExist(card inputCard, card *playerDeck ){

    for (int i=0;i<MAX_CARD;i++){
        if(playerDeck[i].used == 0){ // this space has card
            if((inputCard.cardType==playerDeck[i].cardType)&&(inputCard.cardStyle==playerDeck[i].cardStyle))
                return i; //exist return position of selected card
            else
                continue;
        }
    }
    return 0; //card not found
}


int checkClientCardValid(card inputCard, card currentDeck){

    if((inputCard.cardType==currentDeck.cardType)||(inputCard.cardStyle==currentDeck.cardStyle))
        return 1; //possible
    else
        return 0; // condition of attk cards needed to be added
    

    
} 
int cardDeckCounter(card *cardDeck){
    int nullCount=0;
    while(1){
        if(cardDeck[nullCount].used == 1){
            nullCount++;
        }
        else
            break;

    }
    return nullCount;
}

void sendCardToClient(int cardCount, userInfo *playerInfo, card *cardDeck){
    
    int nullCount=0;

    nullCount = cardDeckCounter(cardDeck);

    int checkpoint=0;
    for(int i=0;i<cardCount;i++){
        for(int j=checkpoint;j<MAX_CARD;j++){
            if(playerInfo->cardDeck[j].used== -1){
                playerInfo->cardDeck[j]=cardDeck[nullCount];
                cardDeck[nullCount].used =1;//gave card to client >>used
                playerInfo->cardCount=(playerInfo->cardCount)+1;
                nullCount++;
                if(nullCount==totalCardStyle*totalCardType){
                    printf("cardDeck empty reshuffle active\n");
                    shuffleCard(cardDeck);
                    checkpoint=0;
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
                break;
            }
        }
    }
    return;
}

void shuffleCard(card* cardDeck){
    srand(time(NULL));

    card tmp;
    int shuffleCount = 100;
    int pickNum;
    int pickNum2;
    if(cardDeck[0].used !=0){ //shuffle function activate onlt when used is 100% 0 or 1 
        for(int i=0;i<(totalCardStyle*totalCardType);i++){
            cardDeck[i].used = 0;
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
    printf("card shuffle complete >> cardDeck ready\n");
}

void signalDetection(int sig){
    if(sig ==SIGCONT){
    //printf("signal received.. termainating countdown..");
    start=1;
    }   
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
    char path[12+strlen(username)+1];//./USER_DATA/+ strlen(username)+null
    strcpy(path,userData);
    strcat(path,"/");
    strcat(path,username);
    
    int dirCreate=0;
    dirCreate=mkdir(path,0744);
    if(dirCreate==0){
        printf("DIR_PATH >> %s created\n",path);
        return ;
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
        loginConfirm = checkLoginFile(username,password);//login :1 error :0
        if(loginConfirm){
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


