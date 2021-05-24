
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
#include <stdbool.h>
#include <signal.h>
#include <pthread.h>


#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

bool specialSymbolKiller(char userinput[]);
int guaranteeData(char userinput[]);
int askingPurpose();
void askUsername(char *username);
void askPassword(char *password);

int submenu_myinfo();
int submenu_cardgame();
int submenu(int purpose);
int menu();

int inputPossibleCheck(char read[]);
//void currentCardReceive(char read[]);
int cardRequestCheck(char read[]);
int waitForCardGame();

typedef struct{
    int server;
    int create_success;
}waitForGame;
static waitForGame *waitForServer;

static int additionalPlayer=0; // if 0 you are the one who created a room for card game

//static int currentTurn;
//static char currentDeck[2]; // type/style

int main(int argc, char** argv) {


    if (argc < 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return 1;
    }
//---login or account make----
    int gamestart=0;

    int purpose =0; //default 
    int showMenu=0;
    int actioncode=0;
    char codeNumber[5];
    char requestForserver[10];
    
    char purposeLogin[]="!*login*!\n";
    char purposeAccount[]="!^creating account^!\n";
    char userinput[20];
    char username[20];
    char password[20];


    // while loop until fetch proper purpose from user 
    //1: login / 2: create 
    purpose=askingPurpose();
    askUsername(username);

    askPassword(password);
    
    printf("address : %s\n",argv[1]);
    printf("port : %s\n",argv[2]);

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo(argv[1], argv[2], &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
            address_buffer, sizeof(address_buffer),
            service_buffer, sizeof(service_buffer),
            NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);


    printf("Creating socket...\n");
    SOCKET socket_peer;
    socket_peer = socket(peer_address->ai_family,
            peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(socket_peer)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }


    printf("Connecting.........\n");
    if (connect(socket_peer,
                peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        return 1;
    }
    

    printf("Connected.\n");
    printf("To send data, enter text followed by enter.\n");

    fd_set master;
    FD_SET(socket_peer, &master);
    FD_SET(0, &master);
    SOCKET max_socket = socket_peer;

    freeaddrinfo(peer_address);
    //int screenActive=0;
    while(1) {

        if(showMenu==1){
            actioncode= menu();
            printf("request : %d \n",actioncode);
            showMenu=0;
            sprintf(codeNumber,"%d",actioncode);
            strcpy(requestForserver,"=");
            strcat(requestForserver,codeNumber);
            strcat(requestForserver,"=\n");
            printf("my request to server %s \n",requestForserver);
            sleep(0.7);
            send(socket_peer, requestForserver, strlen(requestForserver), 0);
            system("clear");
            continue;
        }
        /*
        if(gamestart==1&&screenActive==0){
            screenActive=1;
            char gameScreenRequest[]="@GAME_SCREEN_REQUEST@\n";
            send(socket_peer, gameScreenRequest, strlen(gameScreenRequest), 0);
            continue;
        }
        */
        
        fd_set reads;
        reads = master;

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(max_socket+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        //from server
        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            
            // activate only when game is running
            if(gamestart==1){
                /*
                if(read[0]=='[' && read[bytes_received-2]==']'){
                    //extractor
                    printf("recevied %d bytes >> [%.*s]\n",bytes_received,bytes_received, read);
                    currentCardReceive(read);
                    continue;
                }
                */
                //else{
                //sleep(0.1);
                if(bytes_received < 1){
                    system("clear");
                    printf("connection closed by server>> game end \n");
                    break;
                }
                if((read[0]=='!' && read[bytes_received-2]=='!')){
                    printf("\n\nTime Out !!\n");
                    char confirmMessage[]="! confirmed !\n";
                    send(socket_peer, confirmMessage, strlen(confirmMessage), 0);
                    continue;
                }

                if((read[0]=='#' && read[bytes_received-2]=='#')){
                    system("clear");
                    if(read[1]=='#' && read[bytes_received-3]=='#'){
                        printf("\nOther player(s) eliminated you won!! \n");
                    }
                    else if(read[1]=='!' && read[bytes_received-3]=='!'){
                        printf("\nYou Won!!\n");
                    }
                    else if(read[1]=='~' && read[bytes_received-3]=='~'){
                        printf("\nYou Lose..\n");
                    }
                    else if(read[1]=='*' && read[bytes_received-3]=='*'){
                        printf("\nYou Died\n");
                    }
                    sleep(1);
                    printf("\nBack to Menu.. \n");
                    sleep(1);
                    /*
                    char backToMenu[]="$ client back to menu $\n";
                    send(socket_peer, backToMenu, strlen(backToMenu), 0);
                    */
                    gamestart=0;
                    showMenu=1;
                    continue;
                }

                if((read[0]=='@' && read[bytes_received-2]=='@')){
                    printf("%.*s \n",bytes_received, read);
                    char readyMessage[]="@ Ready @\n";
                    send(socket_peer, readyMessage, strlen(readyMessage), 0);
                    sleep(0.3);
                    continue;
                }
                
                system("clear");
                printf("%.*s \n",bytes_received, read);

                //char informaionRequest[]="?turn and currnet deck?\n";
                //send(socket_peer, informaionRequest, strlen(informaionRequest), 0);
                //printf("screen request sent\n");
                continue;
                //}
            }
            //kill connnection or message from server
            if (bytes_received < 1 || (read[0]=='#' && read[bytes_received-2]=='#')) {
                if(read[1]=='!' && read[bytes_received-3]=='!'){
                    printf("%.*s \n",bytes_received, read);
                    showMenu=1;
                    continue;
                }
                printf("%.*s \n",bytes_received, read);
                printf("Connection closed by server.\n");
                break;
            }
            // login : 0  create_account :1;
            if ((read[0]=='?' && read[bytes_received-2]=='?')){  
                if((read[1]=='^' && read[bytes_received-3]=='^')){
                    printf("server asked your ID & PW \n");
                    char sendIDPW[3+strlen(username)+1+strlen(password)+5];
                    //!^!username+password!^!\\n + null
                    if(purpose==1)
                        strcpy(sendIDPW,"!*!"); //login form
                    else
                        strcpy(sendIDPW,"!^!"); //account form
                    strcat(sendIDPW,username);
                    strcat(sendIDPW,"+");
                    strcat(sendIDPW,password);
                    if(purpose==1)
                        strcat(sendIDPW,"!*!\n");
                    else
                        strcat(sendIDPW,"!^!\n");
                    send(socket_peer, sendIDPW, strlen(sendIDPW), 0);
                    printf("sent ID PW \n");
                    continue;
                }
                printf("server asked your purpose of connnection\n");
                
                if(purpose==1){
                    send(socket_peer, purposeLogin, strlen(purposeLogin), 0);
                }
                else if(purpose==2){ 
                    send(socket_peer, purposeAccount, strlen(purposeAccount), 0);
                }
                printf("sent purpose \n");
                continue;
            }
            //information requset
            if ((read[0]=='=' && read[bytes_received-2]=='=')){
                if ((read[1]=='*' && read[bytes_received-3]=='*')){
                    char repassword[20];
                    while(1){
                        printf("=change password=\n");
                        askPassword(password);
                        printf("=new password recheck=\n");
                        askPassword(repassword);
                        if(!strcmp(password,repassword)){
                            printf("pw confirmed.. \n");
                            showMenu=1;
                            break;
                        }
                    }
                    send(socket_peer, password, strlen(password), 0);
                    continue;
                } 
                if((read[1]=='?' && read[bytes_received-3]=='?')){
                    char checkLastTime[10];
                    while(1){
                        printf("Are you sure? y/n >> ");
                        
                        if(!fgets(checkLastTime,sizeof checkLastTime,stdin)){
                            printf("unproper value\n");
                            sleep(1);
                            system("clear");
                            continue;
                        }
                        
                        if(!strcmp(checkLastTime,"y\n")||!strcmp(checkLastTime,"Y\n")){
                            printf("delete process activate.. \n");
                            send(socket_peer, checkLastTime, strlen(checkLastTime), 0);
                            break;
                        }
                        else if(!strcmp(checkLastTime,"n\n")||!strcmp(checkLastTime,"N\n")){
                            send(socket_peer, checkLastTime, strlen(checkLastTime), 0);
                            printf("canceled\n");
                            showMenu=1;
                            break;
                        }
                        else{
                            printf("unvalid input\n");
                            sleep(1);
                            system("clear");
                            continue;
                        }
                    }
                    continue;
                }
            }
            //card game request
            if ((read[0]=='<' && read[bytes_received-2]=='>')){
                
                printf("%.*s\n", bytes_received,read);
                
                int connectionReusult=0;
                waitForServer = malloc((sizeof(*waitForServer)));
                (*waitForServer).server = socket_peer;
                waitForServer->create_success=0;

                connectionReusult = waitForCardGame();
                printf("game screen request to server \n");
                //printf("connectionReusult : %d \n",connectionReusult);
                //system("clear");
                // do something with result 0 or 1 
                if(connectionReusult){
                    
                    gamestart=1;
                    //printf("game start : %d \n",gamestart);
                }
                else{
                    showMenu=1;
                }

                continue;
            }
            //back to menu
            if ((read[0]=='$' && read[bytes_received-2]=='$')){ 
                printf("\n\n server ready to response\n\n");
                continue;
            }

            continue;
        }
        //stdin
        if(FD_ISSET(0, &reads)) {
            if(gamestart==1){
                char read[50];
                while(1){
                    if (!fgets(read, 50, stdin)){ 
                        printf("FATAL ERROR >> unproper input\n");
                        break;
                    }
                    /*
                    if(cardRequestCheck(read)&&inputPossibleCheck(read)){ // input valid check// input possible check
                        break;
                    }
                    else
                    */ 
                    
                    if((read[0]=='0' &&read[1]=='\n')){ // means no card to send request one card to server
                        break;
                    }
                    if(isalpha(read[0])&&isalnum(read[1])&&(read[2]=='\n')){
                        break;
                    }
                    
                    /*
                    else{
                        printf("invalid input\n");
                        continue;
                    }
                    */
                }
                //printf("Sending: %s", read);
                send(socket_peer, read, strlen(read), 0);
            }
            //int bytes_sent = send(socket_peer, read, strlen(read), 0);
            //printf("Sent %d bytes.\n", bytes_sent);
        }
    } //end while(1)

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

    printf("Finished.\n");
    return 0;
}
/*
void currentCardReceive(char read[]){// input is 100% [card+turn]\n
    int idx=0;
    int turnIdx=0;
    int turnRead=0;
    char turnReader[4]="\0\0\0";
    currentDeck[0]=read[1]; //card Type
    currentDeck[1]=read[2];//card Style

    while(read[idx]!='\n'){

        if(read[idx]=='+'){
            turnRead=1;
        }
        
        if(turnRead){
            turnReader[turnIdx]=read[idx];
            turnIdx++;
        }
        idx++;
    }
    currentTurn = atoi(turnReader);
}
*/
/*
int inputPossibleCheck(char read[]){
    
        read[0]=toupper(read[0]);
        read[1]=tolower(read[1]);  
        
        if((read[0]==currentDeck[0])||(read[1]==currentDeck[1])){
            return 1;
        }
        else
            return 0; // no match invalid
    
}
*/
/*
int cardRequestCheck(char read[]){
    int idx=0;
    while(1){
        if(isalnum(read[idx]) && (idx<2)){
            idx++;
        }
        else if(read[idx]=='\n' && idx==2){
            return 1;
        }
        else{
            return 0; // nor alpha,num \n then invalid input
        }
    }
}
*/
int askingPurpose(){
    char userinput[20];
    int purpose=0;
    printf("What do you want to do ?\n");
    printf("[command List]\n");
    printf("-Login          : 1\n");
    printf("-Create Account : 2\n");

    
    while(1){
    printf("Your command >> type number: ");
    if(!fgets(userinput,sizeof userinput,stdin)){
        printf("unproper input\n");
        continue;
        //printf("length of id and pw must be lower than 20\n");
        //printf("special symbols not allowed\n only eng and numbers are allowed..\n");
    }
    else{
        purpose= atoi(userinput);

        if(purpose>0 &&purpose<3){
            printf("your order >> %d confirmed \n",purpose);
            return purpose;
        }
        else{
            printf("unidentified order.. \n");
            continue;
        }
    }
    }
}

int waitForCardGame(){

    int bytes_received;


    char waitForRoom[]="waitingForRoom";
    send(waitForServer->server, waitForRoom, strlen(waitForRoom), 0); // needed to activate 101 code
    
    fd_set reads_thread;
    FD_ZERO(&reads_thread);
    FD_SET(waitForServer->server, &reads_thread);
    SOCKET max_socket = waitForServer->server;
    while(1){
        
        fd_set read_in_loop;
        read_in_loop = reads_thread;
        

        int waiting=0;
        char read[100];

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        if (select(max_socket+1, &read_in_loop, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }

    
        if(FD_ISSET(waitForServer->server, &read_in_loop)){
            bytes_received = recv(waitForServer->server, read, 100, 0);
            if(read[0]=='$' && read[bytes_received-2]=='$'){
                additionalPlayer=1;// you entered waiting room which other made
                continue;
            }
            
            if(read[0]=='{' && read[bytes_received-2]=='}'){
                if(read[1]=='#' && read[bytes_received-3]=='#'){
                    printf("%.*s \n",bytes_received, read);
                    sleep(1);
                    return 0; // go back to main menu
                }
                else{
                    printf("%.*s \n",bytes_received, read);
                    if(additionalPlayer==0){
                        send(waitForServer->server, waitForRoom, strlen(waitForRoom), 0);// needed to make thread from server      
                    }
                    else if(additionalPlayer==1){
                        sleep(0.3);
                        send(waitForServer->server, waitForRoom, strlen(waitForRoom), 0);// wait for room creator   
                    }
                    return 1;
                    //go to game function 
                }
            }
        }
        else{
            continue;
            system("clear");
        }
        
    }
    
}   

void askUsername(char username[]){
    char userinput[20];
    
    while(1){
        printf("type username:");
        fgets(userinput,sizeof userinput,stdin);
        if(guaranteeData(userinput)){
            printf("username : %s confirmed \n",userinput);
            strcpy(username,userinput); 
            break;
        }
    }
}

void askPassword(char password[]){
    char userinput[20];
    while(1){
        printf("type password:");
        fgets(userinput,sizeof userinput,stdin);
        if(guaranteeData(userinput)){
            printf("password checking..\n");
            strcpy(password,userinput); 
            break;
        }
    }
}

int guaranteeData(char userinput[]){
    
    bool vaild=false;

    if((strlen(userinput)<8 ) ||strlen(userinput)>16){
        printf("unproper input\n");
        printf("length of id and pw must be shorter than 15, longer than 6\n");
        return 0;
    }
    else{
        vaild=specialSymbolKiller(userinput);
        if(vaild==false){
            printf("special symbols not allowed\nonly eng and numbers are allowed..\n");
            return 0;
        }
        else{
            userinput[strlen(userinput)-1]='\0'; // erase '\n' 
            return 1;
        }
    }
    

}

bool specialSymbolKiller(char userinput[]){

    int endIndex = strlen(userinput)-2;

    for(int i=0; i<endIndex;i++){
        if(isalpha(userinput[i])||isdigit(userinput[i]))
            continue;
        else{
            printf("unproper letter <%c> detected \n",userinput[i]);
            return false;
        }
    }
    return true;
}

int menu(){
    char userinput[20];
    int purpose=0;
    while(1){
        sleep(0.7);
        system("clear");
        printf("==== MENU ====\n");
        printf("[command List]\n");
        printf("-start card game : 1\n");
        printf("-my information  : 2\n");
        printf("-shop            : 3\n");

        while(1){
            printf("Your command >> type number: ");
            if(!fgets(userinput,sizeof userinput,stdin)){
                printf("unproper input\n");
                continue;
                //printf("length of id and pw must be lower than 20\n");
                //printf("special symbols not allowed\n only eng and numbers are allowed..\n");
        }
        else{
            purpose= atoi(userinput);

            if(purpose>0 &&purpose<4){
                if(purpose ==3){
                    printf("\nshop yet developed\n");
                    sleep(1.2);
                    break;
                }
                printf("your order >> %d confirmed \n",purpose);
                return submenu(purpose);
            }
            else{
                printf("unidentified order.. \n");
                printf("returning to menu..\n");
                break;
            }
        }
        }
    }


}

int submenu(int purpose){
    switch(purpose){
        case 1:
            return submenu_cardgame();
            break;
        case 2:
            return submenu_myinfo();
            break;
    }
}

int submenu_cardgame(){
    char userinput[20];
    int purpose=0;
    while(1){
        sleep(0.7);
        system("clear");
        printf("==== SUB MENU ====\n");
        printf("[command List]\n");
        printf("-match cardgame      : 1\n");
        printf("-match with computer : 2\n");
        printf("-help                : 3\n");

        while(1){
            printf("Your command >> type number: ");
            if(!fgets(userinput,sizeof userinput,stdin)){
                printf("unproper input\n");
                continue;
                //printf("length of id and pw must be lower than 20\n");
                //printf("special symbols not allowed\n only eng and numbers are allowed..\n");
        }
        else{
            purpose= atoi(userinput);
            if(purpose>0 &&purpose<4){
                if(purpose == 2){
                    printf("\nmatch with computer yet developed\n");
                    sleep(1.2);
                    break;
                }
                //manual
                if(purpose ==3){
                    system("clear");
                    printf("Card type : R G B Y \n");
                    printf("R : Red / G : Green / B : Blue / Y : Yellow\n\n");
                    printf("Card style : 1~9 p j d a s q\n");
                    printf("1~9 : number \n");
                    printf("p : extra turn / j : jump / d : turn reverse\n");
                    printf("a : weak attack / s : strong attack / q : ultimate\n\n");
                    printf("Red ultimate : ultimate attack \n");
                    printf("Green ultimate : reflect all attack\n");
                    printf("Blue ultimate : block and disable all attack\n");
                    printf("Yellow ultimate : skip turn or dodge attack  \n\n");
                    printf("15+ card : card overflow kick out\n");
                    printf("ignore turn 3 times : kick out \n\n");
                    printf("press ENTER to go back to menu \n");
                    if(fgets(userinput,sizeof userinput,stdin)){
                        break;
                    }                
                }

                printf("your order >> %d confirmed \n",purpose);
                return purpose+100; // match code 101~102
            }
            else{
                printf("unidentified order.. \n");
                printf("returning to submenu..\n");
                break;
            }
        }
        }
    }
}

int submenu_myinfo(){
    char userinput[20];
    int purpose=0;
    while(1){
        sleep(0.7);
        system("clear");
        printf("==== SUB MENU ====\n");
        printf("[command List]\n");
        printf("-Change password : 1\n");
        printf("-Delete account  : 2\n");
        printf("-Change title    : 3\n");
        printf("-match history   : 4\n");
        
        while(1){
            printf("Your command >> type number: ");
            if(!fgets(userinput,sizeof userinput,stdin)){
                printf("unproper input\n");
                continue;
                //printf("length of id and pw must be lower than 20\n");
                //printf("special symbols not allowed\n only eng and numbers are allowed..\n");
        }
        else{
            purpose= atoi(userinput);
            if(purpose>0 &&purpose<5){
                if(purpose == 3){
                    printf("\ntitle yet developed\n");
                    sleep(1.2);
                    break;
                }

                printf("your order >> %d confirmed \n",purpose);
                return purpose+200; // info code 201~202
            }
            else{
                printf("unidentified order.. \n");
                printf("returning to submenu..\n");
                break;
            }
        }
        }
    }
}