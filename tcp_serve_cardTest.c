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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#define SEM_NAME "/Time"

void *waitingTimeOut(void* socket);
void signalDetection(int sig);
static int start=0;
static int connectionEnd=0;
static int create_account=0;
static sem_t * sem;


int loginCheck(char username[],char password[]);
int storeData(char username[],char password[]);
int checkIdData(char username[]);
int checkUserInfo(char *receviedData ,int dataLength);
int saveUserInfo(char *receviedData ,int dataLength);
void playerStore(int players[], int newcomer);
void playerDelete(int players[], int goodbye);
int changeTurn(int players[],int currentTurn);
static int totalPlayer=0;
const static int playerlimit=2;
fd_set master;


int main(int argc, char** argv) {

	if(argc != 2) {
		fprintf(stderr,"Usage: %s Port",argv[0]);
		return 0;  
	}	
    
    int players[4]={-1,-1,-1,-1};
    int turn=-1;
    int purpose=-1;
    char notEnoughPlayer[]="not enough player please wait ... \n";
    char notYourTurn[]="it's not your turn please wait for your turn... \n";
    char askPurpose[]="?purpose?\n";
    

    pthread_t thread_id;
    sem = sem_open(SEM_NAME, O_RDWR | O_CREAT, 0777, 1);
    if(sem == SEM_FAILED)
	{
		fprintf(stderr, "Sem_Open Error \n");
		exit(1);
	}

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
        /*
        if(connectionEnd==1){
            int lonePlayer=unconnectMessage(players);
            FD_CLR(lonePlayer, &master);
            CLOSESOCKET(lonePlayer);        
            totalPlayer=0;
            connectionEnd=0;
            start=0;
            continue;
        }
        */

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

                    if(totalPlayer==playerlimit){
                        char notEnoughSpace[]="#Message from server : Room full connection refused#\n";
                        send(socket_client,notEnoughSpace,strlen(notEnoughSpace),0);
                        CLOSESOCKET(socket_client);
                        printf("connection refused.. \n");
                        continue;    
                    } 

                    FD_SET(socket_client, &master);
                    if (socket_client > max_socket)
                        max_socket = socket_client;
                    printf("connected.. \n");
                    playerStore(players,socket_client);
                    if(totalPlayer==0)
                        turn=players[0];
                    totalPlayer++;
                    printf("player count : %d \n",totalPlayer);
                    //ask purpose of connection
                    send(socket_client, askPurpose,strlen(askPurpose), 0);

                    if(totalPlayer==1){
                        printf("count start \n");
                        int status;
                    	status=pthread_create(&thread_id, NULL, waitingTimeOut, (void *)socket_client);
                        if(status==-1){
                    		fprintf(stderr,"PThread  Creation Error \n");
                    		exit(0);
                    	}
                        pthread_detach(thread_id);

                    }
                    else if(totalPlayer>1){
                        printf("sending  signal to pthread.. \n");
                        signal(SIGTERM,signalDetection);
                        pthread_kill(thread_id,SIGTERM);
                        printf("count down terminated\n");
                    }
                    


                } else {
                    char read[1024];
                    int bytes_received = recv(i, read, 1024, 0);

                    if(read[0]=='!' && read[bytes_received-2]=='!'){
                        purpose=0;// ! ~~ ! means something special from client
                    }

                  

                    if (bytes_received < 1) {
                        printf("exit detected.. \n");
                        FD_CLR(i, &master);
                        if (turn==i){
                            turn=changeTurn(players,turn);
                        }
                        playerDelete(players, i);
                        totalPlayer--;
                        printf("player count : %d \n",totalPlayer);
                        if(totalPlayer==0){
                            connectionEnd=0;
                            start=0;
                        }
                        CLOSESOCKET(i);                          
                        continue;
                    }

                if(purpose== -1){
                    if(totalPlayer<2){
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


                    SOCKET j;
                    for (j = 1; j <= max_socket; ++j) {
                        if (FD_ISSET(j, &master)) {

                        if(purpose!= -1){
                            // !@ ~~ @! command : connecting for card game
                            if((i==j)&&(read[1]=='@' && read[bytes_received-3]=='@')){
                                printf("client %d is connecting for card game \n",j);
                                purpose=-1;//situation disabled
                                continue;
                            }
                            // !^ ~~ ^! command : connecting for creating account
                            if((i==j)&&(read[1]=='^' && read[bytes_received-3]=='^')){
                                if(read[2]=='!' && read[bytes_received-4]=='!'){
                                    //client sent ID and password
                                    int resultOfReceivedData=0;
                                    start=1;//countdown end
                                    resultOfReceivedData=saveUserInfo(read ,bytes_received);
                                    if(resultOfReceivedData){
                                        char saveComplete[]="##Message from server : your account is created##";
                                        send(j,saveComplete, strlen(saveComplete), 0);
                                    }
                                    else{
                                        char saveError[]="##Message from server : identical username exists##";
                                        send(j,saveError, strlen(saveError), 0);
                                    }
                                    purpose=-1;//situation disabled
                                    continue;
                                }
                                printf("client %d is connecting for creating account \n",j);
                                start=1;//countdown end
                                printf("CountDown disabled...\n");
                                purpose=-1; //situation disabled
                                char requireData[]="?^send me ID and PW^?\n"; 
                                create_account=1;// activate creating module
                                send(j,requireData, strlen(requireData), 0);
                                create_account=0;
                                continue;
                            }
                            // !* ~~ *! command : connecting for login
                            if((i==j)&&(read[1]=='*' && read[bytes_received-3]=='*')){
                                if(read[2]=='!' && read[bytes_received-4]=='!'){
                                    //client sent ID and password
                                    int resultOfReceivedData=0;
                                    start=1;//countdown end
                                    resultOfReceivedData=saveUserInfo(read ,bytes_received);
                                    if(resultOfReceivedData){
                                        char saveComplete[]="##Message from server : your account is created##";
                                        send(j,saveComplete, strlen(saveComplete), 0);
                                    }
                                    else{
                                        char saveError[]="##Message from server : identical username exists##";
                                        send(j,saveError, strlen(saveError), 0);
                                    }
                                    purpose=-1;//situation disabled
                                    continue;
                                }
                                printf("client %d is connecting for login\n",j);
                                start=1;//countdown end
                                printf("CountDown disabled...\n");
                                purpose=-1; //situation disabled
                                char requireData[]="?^send me ID and PW^?\n"; 
                                create_account=1;// activate creating module
                                send(j,requireData, strlen(requireData), 0);
                                create_account=0;
                                continue;
                            }


                            continue;
                        }


                            if (j == socket_listen || i==j)
                                continue;
                            else{
                                send(j, read, bytes_received, 0);                              
                            }
                        }
                    }

                }
            } //if FD_ISSET
        } //for i to max_socket
    } //while(1)



    printf("Closing listening socket...\n");
    CLOSESOCKET(socket_listen);

    printf("Finished.\n");

    return 0;
}

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
    int storeResult=0;
    printf("Extraction complete..\n");
    printf("ID : %s \nPW: : %s \n",username,password);
    storeResult = storeData(username,password);
    if(storeResult){
        printf("client Data Stored in ID_DATA \n");
        return 1;
    }
    else{
        printf("Client Data name : %s already exists.. \n",username);
        return 0;
    }


}

void playerStore(int *players, int newcomer){

    for(int i=0;i<playerlimit;i++){
        if(players[i]==-1){
            players[i] = newcomer;
            printf("player %d >> %d stored.. \n",i,newcomer);
            break;
        }        
    } 

}

void playerDelete(int *players, int goodbye){

    for(int i=0;i<playerlimit;i++){
        if(players[i]==goodbye){
            players[i] = -1;
            printf("player %d >> %d deleted.. \n",i,goodbye);        
            break;
        }
    } 

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

void *waitingTimeOut(void * socket)
{

    int client = (int)socket;
    char message[]="##Message from server : wating time out##\n";
	for(int i=0; i < 10; i++)
	{
        printf("%d seconds before end connection.. \n",10-i);
        sleep(1);
        if(start==1){
            return NULL; 
        }
	}
    connectionEnd=1;

    printf("disconnection value : %d >>Activated..\n",connectionEnd);
    send(client, message,strlen(message), 0);
    FD_CLR(client, &master);
    CLOSESOCKET(client);        
    totalPlayer=0;
    connectionEnd=0;
    start=0;


	return NULL;
}
void signalDetection(int sig){
    if(sig ==SIGTERM){
    //printf("signal received.. termainating countdown..");
    start=1;
    }   
}
int loginCheck(char username[],char password[]){
    FILE *fptr;
    FILE *fptr_ID;
    printf("opening file.. \n");
    fptr= fopen("./maybeTest","r");
    if(fptr==NULL){
        printf("FILE READ ERROR.. \n");
        exit(1);
    }

    int alreadyExist = 0;
    alreadyExist = checkIdData(username);

    if(alreadyExist  == 1){
        fclose(fptr);
        return 0; // not 
    }
    // store 

    printf("store process completed.. \n\n");
    
    printf("Noticing ID_DATA.. \n");
    
    fclose(fptr);
    return 1; //
}


int storeData(char username[],char password[]){
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
/*
    fptr=freopen("./maybeTest","r",fptr);
    printf("reopening file... \n");
*/
    fclose(fptr);
    fclose(fptr_ID);
    return 1; //stored
}

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




