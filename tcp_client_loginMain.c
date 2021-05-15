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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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

int main(int argc, char** argv) {


    if (argc < 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return 1;
    }
//---login or account make----
    
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



    freeaddrinfo(peer_address);
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
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(socket_peer, &reads);
        FD_SET(0, &reads);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        if (select(socket_peer+1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            return 1;
        }
        //from server
        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            
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

            printf("Received (%d bytes): %.*s \n",bytes_received, bytes_received, read);
            
        }

        //stdin
        if(FD_ISSET(0, &reads)) {
            char read[4096];
            if (!fgets(read, 4096, stdin)){ 
                printf("FATAL ERROR >> unproper input\n");
                break;
            }
            printf("Sending: %s", read);
            int bytes_sent = send(socket_peer, read, strlen(read), 0);
            printf("Sent %d bytes.\n", bytes_sent);
        }
    } //end while(1)

    printf("Closing socket...\n");
    CLOSESOCKET(socket_peer);

    printf("Finished.\n");
    return 0;
}



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