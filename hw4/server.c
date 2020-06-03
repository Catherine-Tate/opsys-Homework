#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>

#include <arpa/inet.h>

#include <netinet/in.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAXCLIENTS 32
#define bufferSize 1024 //using the buffer size from in class examples

struct user {
    char * uName;
    int fd;
};

//array of logged in users
struct user allUsers[MAXCLIENTS];
//highest fd of a logged in user
int connected = 0;


//mutex for locking user list
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


char ** getMesInfo(char * buffer) {
    char ** recAndLen = calloc(2, sizeof(char*));
    char * newBuf = calloc(strlen(buffer)+1, sizeof(char));
    strcpy(newBuf, buffer);
    char * rec = strtok(newBuf, " ");
    rec = strtok(NULL, " ");
    char * len = strtok(NULL, " ");
#ifdef DEBUGMODE
    printf("target: %s\n", rec);
    printf("length: %s\n", len);
#endif
    recAndLen[0] = rec;
    recAndLen[1] = len;
    return(recAndLen);
}

void logout(struct user * client) {
    pthread_mutex_lock(&mutex);
    free(client -> uName);
    client -> uName = NULL;
    pthread_mutex_unlock(&mutex);
    write(client -> fd, "OK!\n", 4);
}

void sendMes(struct user client, char * recUser, char * len, char * message, int i) {
    int recFD = -1;
    for(int i = 0; i < connected; i++) {
        if(allUsers[i].uName == NULL) continue;
        if(strcmp(recUser, allUsers[i].uName) == 0) recFD = allUsers[i].fd; 
    }
    if(recFD == -1) {
        char * errMes = "ERROR Unknown userid\n";
        write(client.fd, errMes, strlen(errMes));
        printf("CHILD %ld: Sent ERROR (Unknown userid)\n", pthread_self());
        return;
    }

    if(i==0)write(client.fd, "OK!\n", 4);
    sleep(1);
    char * toSend = calloc(8 + sizeof(client.uName) + sizeof(len) + sizeof(message) \
            , sizeof(char));
    strcpy(toSend, "FROM ");
    strcat(toSend, client.uName);
    strcat(toSend, " ");
    strcat(toSend, len);
    strcat(toSend, " ");
    strcat(toSend, message);
    strcat(toSend, "\n");
    write(recFD, toSend, strlen(toSend));
}

//sends a message to every connected user on the server
void broadcast(struct user client, char * buffer) {
    char * len = strtok(buffer, " ");
    len = strtok(NULL, " ");
    len = strtok(len, "\n");
    char * message = strtok(NULL, "\n");
#ifdef DEBUGMODE
    printf("len: %s\n", len);
    printf("mes: %s\n", message);
    printf("trulen: %ld\n", strlen(message));
#endif
    //message = strtok(NULL, "\n");
    
    //write(client.fd, "OK!\n", 4);
    for(int i = 0; i < connected; i++) {
        if(!allUsers[i].uName) continue;
        sendMes(client, allUsers[i].uName, len, message, i);           
    }
}

/*
 *extracts username from an input buffer
 */
char * getUsername(char * buffer) {
    char * name = strtok(buffer, " ");
    name = strtok(NULL, " ");
    char * spot = name;
    while(*spot != '\n') {
        spot++;
    }
    *spot = '\n';
    return(name);
}

/*
 * sets the clients userid to the given username if the given username is valid
 * valid username: 4-16 chars long, unused
 */
void login(struct user * client, char * name) {
    name = strtok(name, "\n");
    if(strlen(name) < 4 || strlen(name) > 16) {
        char * errMes = "ERROR Invalid userid\n";
        write(client -> fd, errMes, strlen(errMes));
        printf("CHILD %ld: Sent ERROR (Invalid userid)\n", pthread_self());
        return;
    }
#ifdef DEBUGMODE
    printf("Client FD is: %d\n", client -> fd);
#endif
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < connected; i++) {
        if(!allUsers[i].uName) continue;
        if(strcmp(allUsers[i].uName, name) == 0) {
            char * errMes = "ERROR Already connected\n";
            write(client -> fd, errMes, strlen(errMes));
            printf("CHILD %ld: Sent ERROR (Already connected)\n", pthread_self());
            pthread_mutex_unlock(&mutex);
            return;
        }
    }
    client -> uName = calloc(strlen(name)+1, sizeof(char));
    strcpy(client -> uName, name);
    pthread_mutex_unlock(&mutex);
    write(client -> fd, "OK!\n", 4);
}


/*
 *Lists logged in users in the server in alphabetical order
 *type is 1 for tcp, 0 for udp
 *client is only needed for udp
 */
void who(struct user client, char type, struct sockaddr * udpClient) {

    char temp[16];
    char ** alphaUsers = NULL;

    //copy all users
    int hasName = 0;
    for(int i = 0; i < connected; i++) {
        if(!allUsers[i].uName) continue;
#ifdef DEBUGMODE
        printf("%d: %s\n", i, allUsers[i].uName);
#endif
        hasName++;
        alphaUsers = realloc(alphaUsers, hasName * sizeof(char *));
        alphaUsers[hasName-1] = calloc(strlen(allUsers[i].uName)+1, sizeof(char));
        strcpy(alphaUsers[hasName-1], allUsers[i].uName);
    }
    
    //sort alphabetically
    for(int i = 0; i < hasName; i++) {
        for(int j = i+1; j < hasName; j++){
            if(strcmp(alphaUsers[i], alphaUsers[j]) > 0) {
                strcpy(temp, alphaUsers[i]);
                strcpy(alphaUsers[i], alphaUsers[j]);
                strcpy(alphaUsers[j], temp);
            }
        }    
    }

    char * result = calloc(5, sizeof(char));
    strcpy(result, "OK!\n");
    for(int i = 0; i < hasName; i++) {
#ifdef DEBUGMODE
        printf("user %d is: %s\n", i, alphaUsers[i]);
#endif
        result = realloc(result, strlen(result) + strlen(alphaUsers[i])+2);
        strcat(result, alphaUsers[i]);
        strcat(result, "\n");
    }

#ifdef DEBUGMODE
    printf("result is: %s", result);
#endif
    
    if(type == 1) write(client.fd, result, strlen(result));
    else {
        sendto(client.fd, result, strlen(result), 0, udpClient, sizeof(client));

    }
}  

void handleUDP(char * buffer, int len, int fd, struct sockaddr_in client) {
    struct user udpClient;
    udpClient.fd = fd;
    udpClient.uName = "UDP-client";
    if(strstr(buffer, "LOGIN") == buffer) {
        sendto(fd, "Not supported on UDP\n", 21, 0, (struct sockaddr *)&client, sizeof(client));
        //printf("CHILD %ld: Rcvd LOGIN request for userid %s", pthread_self(), username);
        return;
    }
    else if(strstr(buffer, "WHO") == buffer){
        printf("MAIN: Rcvd WHO request\n");
        who(udpClient, 0, (struct sockaddr *)&client);    
    }
    else if(strstr(buffer, "LOGOUT") == buffer) {
        char * errMes = "ERROR Not supported on UDP\n";
        sendto(fd, errMes, strlen(errMes), 0, (struct sockaddr *)&client, sizeof(client));
        return;
    }
    else if(strstr(buffer, "SEND") == buffer) {
        char * errMes = "ERROR SEND not supported on UDP\n";
        sendto(fd, errMes, strlen(errMes), 0, \
         (struct sockaddr *)&client, sizeof(client));
        //printf("CHILD %ld: Rcvd SEND request to userid %s\n", pthread_self(), sendInfo[0]);
        return;
    }
    else if(strstr(buffer, "BROADCAST") == buffer) {
        printf("MAIN: Rcvd BROADCAST request\n");
        broadcast(udpClient, buffer); 
    }
    
}

//function to handle tcp connections
//only ran by child threads
//no return value
void * handleClient(void * clientNum) {
    int *num = clientNum;
#ifdef DEBUGMODE
    printf("num is %d\n", *num);
#endif
    struct user * newClient = (allUsers + *num);
    while(1) {
        int fd = newClient -> fd;
        char buffer[bufferSize];
        int inBytes = recv(fd, buffer, bufferSize-1, 0);
        if(inBytes == 0) {
            //TODO: error catching? DC? idk yet
            printf("CHILD %ld: Client disconnected\n", pthread_self());
            pthread_exit(NULL);
        }
        if(strstr(buffer, "LOGIN") == buffer) {
            char * username = getUsername(buffer);
            printf("CHILD %ld: Rcvd LOGIN request for userid %s", pthread_self(), username);
            login(newClient, username);
#ifdef DEBUGMODE
            for(int i = 0; i < connected; i++) printf("%d: %s\n", i, allUsers[i].uName);
#endif
        }
        else if(strstr(buffer, "WHO") == buffer){
            printf("CHILD %ld: Rcvd WHO request\n", pthread_self());
            who(*newClient, 1, NULL);    
        }
        else if(strstr(buffer, "LOGOUT") == buffer) {
            printf("CHILD %ld: Rcvd LOGOUT request\n", pthread_self());
            logout(newClient);
        }
        else if(strstr(buffer, "SEND") == buffer) {
            char * data = strtok(buffer, "\n");
            char * message = strtok(NULL, "\n");
#ifdef DEBUGMODE
            printf("message is: %s\n", message);
#endif
            char ** sendInfo = getMesInfo(data);
            if(!atoi(sendInfo[1])) {
                char * errMes = "ERROR Invalid msglen\n";
                write(newClient->fd, errMes, strlen(errMes));
            }
            //int mesLen = recv(fd, messBuf, bufferSize-1, 0);
            //if(!mesLen) {
            //    fprintf(stderr, "Child %ld: recv errror\n", pthread_self());
            //}
            printf("CHILD %ld: Rcvd SEND request to userid %s\n", pthread_self(), sendInfo[0]);
            sendMes(*newClient, sendInfo[0], sendInfo[1], message, 0);           
        }
        else if(strstr(buffer, "BROADCAST") == buffer) {
            printf("CHILD %ld: Rcvd BROADCAST request\n", pthread_self());
            broadcast(*newClient, buffer); 
        }
    }
   
}

int main(int argc, char ** argv) {
    
    setvbuf( stdout, NULL, _IONBF, 0 );
    //users = calloc(MAXCLIENTS, sizeof(char *));    

	if(argc != 2) {
		fprintf(stderr, "ERROR: Must have one argument\n");
		return(EXIT_FAILURE);
	}

    fd_set readfds;
    int clientSockets[MAXCLIENTS];
    pthread_t tid[MAXCLIENTS];
    int clientSocketIndex = 0;
    struct sockaddr_in servaddr, client;

    int port = atoi(argv[1]);

    printf("MAIN: Started server\n");

    /* create listening TCP socket */
    int tcp = socket(AF_INET, SOCK_STREAM, 0); 
    bzero(&servaddr, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET; 
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(port); 
    printf("MAIN: Listening for TCP connections on port: %d\n", port);

    // binding server addr structure to listenfd 
    bind(tcp, (struct sockaddr*)&servaddr, sizeof(servaddr)); 
    listen(tcp, 10); 
  
    /* create UDP socket */
    int udp = socket(AF_INET, SOCK_DGRAM, 0); 
    // binding server addr structure to udp sockfd 
    bind(udp, (struct sockaddr*)&servaddr, sizeof(servaddr)); 
    printf("MAIN: Listening for UDP datagrams on port: %d\n", port);


    //int tcp = makeTCPServer(port);
    //int udp = makeUDPServer(port);
    
    //TODO: put error checking stuff here

    int fromLen = sizeof(client);

    FD_ZERO( &readfds );
    while(1) {

        FD_SET( tcp, &readfds );
        FD_SET( udp, &readfds );
 
        for(int i = 0; i < clientSocketIndex; i++) {
            FD_SET(clientSockets[i], &readfds);
        }
        
        int ready = select(FD_SETSIZE, &readfds, NULL, NULL, NULL);
        if(ready == 0) continue;

        //check incoming tcp connections first
        if(FD_ISSET(tcp, &readfds)) {
            int newFD = accept(tcp, (struct sockaddr*) &client, (socklen_t*) &fromLen);
            char * ip = inet_ntoa(client.sin_addr);
            printf("MAIN: Rcvd incoming TCP connection from: %s\n", ip);
            clientSockets[clientSocketIndex] = newFD;
            struct user newUser;
            newUser.fd = newFD;
            newUser.uName = NULL;
            allUsers[clientSocketIndex] = newUser;
            clientSocketIndex++; 
            connected = clientSocketIndex; 
            int num = clientSocketIndex-1;
            int rc = pthread_create(&tid[clientSocketIndex-1], NULL, handleClient, \
                (void *) &num);
            
            if(rc != 0) {
                fprintf(stderr, "ERROR: pthread_create failed\n");
            }
        }        
        
        //check UDP here
        //TODO: handle UDP connections
        if(FD_ISSET(udp, &readfds)) {
            char buffer[bufferSize];
            char * ip = inet_ntoa(client.sin_addr);
            int fromLen = sizeof(client);
            bzero(&servaddr, sizeof(servaddr));
            printf("MAIN: Rcvd incoming UDP datagram from: %s\n", ip);
            int n = recvfrom(udp, buffer, sizeof(buffer), 0, \
             (struct sockaddr*)&client, (socklen_t *)&fromLen);
            if(n == -1) {
                fprintf(stderr, "MAIN: ERROR recv failed\n");
                continue;
            }
#ifdef DEBUGMODE
            //sendto(udp, "hello\n", 6, 0, (struct sockaddr*)&client, sizeof(client));
#endif 
            handleUDP(buffer, n, udp, client);
            
        }

    }


	return(EXIT_SUCCESS);
}
