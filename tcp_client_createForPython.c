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

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)

int createAccount(char address[],char port[],char username[],char password[]) {

/*
    if (argc < 3) {
        fprintf(stderr, "usage: tcp_client hostname port\n");
        return 1;
    }
*/

    printf("address : %s\n",address);
    printf("port : %s\n",port);
    printf("username : %s \n",username);
    printf("password : %s \n",password);

    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo((const char*)address, (const char*)port, &hints, &peer_address)) {
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

        if (FD_ISSET(socket_peer, &reads)) {
            char read[4096];
            int bytes_received = recv(socket_peer, read, 4096, 0);
            if (bytes_received < 1 || (read[0]=='#' && read[bytes_received-2]=='#')) {
                printf("%.*s \n",bytes_received, read);
                printf("Connection closed by server.\n");
                break;
            }
            if ((read[0]=='?' && read[bytes_received-2]=='?')){
                if((read[1]=='^' && read[bytes_received-3]=='^')){
                    printf("server asked your ID & PW \n");
                    char sendIDPW[3+strlen(username)+1+strlen(password)+5];
                    //!^!username+password!^!\\n + null
                    strcpy(sendIDPW,"!^!");
                    strcat(sendIDPW,username);
                    strcat(sendIDPW,"+");
                    strcat(sendIDPW,password);
                    strcat(sendIDPW,"!^!\n");
                    send(socket_peer, sendIDPW, strlen(sendIDPW), 0);
                    printf("sent ID PW \n");
                    continue;
                }
                printf("server asked your purpose of connnection\n");
                char myPurpose[]="!^creating account^!\n";
                send(socket_peer, myPurpose, strlen(myPurpose), 0);
                printf("sent purpose \n");
                continue;
            }
            else{
            printf("Received (%d bytes): %.*s \n",
                    bytes_received, bytes_received, read);
            }
        }

        if(FD_ISSET(0, &reads)) {
            char read[4096];
            if (!fgets(read, 4096, stdin)) break;
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

