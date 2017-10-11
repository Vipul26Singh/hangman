// Client side C/C++ program to demonstrate Socket programming
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#define MAX_CLIENT 1000
#define MAX_FILESIZE 1000
#define GARBAGE_CHAR '^'
#define MSG_LEN 10024


int connectionSocket;

int checkIsNumeric(char *string){
        int indx = 0;

        for(indx = 0; indx < strlen(string); indx++){
                if(!isdigit(string[indx])){
                        return 0;
                }
        }

        return 1;
}

int make_connection(int serverPort, char *serverIP){
	int sock = 0;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("\n Socket creation error \n");
		return -1;
	}

	memset(&serv_addr, '0', sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(serverPort);

	// Convert IPv4 and IPv6 addresses from text to binary form
	if(inet_pton(AF_INET, serverIP, &serv_addr.sin_addr)<=0)
	{
		printf("\nInvalid address/ Address not supported \n");
		exit(1);
	}

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("\nConnection Failed \n");
		exit(1);
	}

	return sock;
}

int send_message(){
	char *msg = NULL;
	size_t len;
	char sendMsg[MSG_LEN] = "";
        int indx = 0;
	
	getline(&msg, &len, stdin);	

	memset(sendMsg, '\0', sizeof(sendMsg));
        strcpy(sendMsg, msg);

        for(indx = strlen(msg); indx < MSG_LEN-1; indx++){
                sendMsg[indx] = GARBAGE_CHAR;
        }


        if(send(connectionSocket, sendMsg, MSG_LEN, 0 ) >= 0){
                return 1;
        }


	free(msg);

        return 0;
}

int recieve_message(char *buffer, int length){
	char received[MSG_LEN];
        int indx;
        memset(received, '\0', sizeof(received));
        memset(buffer, '\0', length-1);
        if(read(connectionSocket, received, MSG_LEN) < 0){
                return 0;
        }

        for(indx = 0; indx < MSG_LEN-1; indx++){
                if(received[indx] == GARBAGE_CHAR){
                        received[indx] = '\0';
                        break;
                }
        }
        strcpy(buffer, received);
        return 1;
}




int main(int argc, char const *argv[])
{
	char buffer[1024] = {0};
	char strPortNo[8];
	int serverPort = 12345;
	char serverIP[100];

	if(argc != 3){
		fprintf(stderr, "Invalid argument number");
                exit(EXIT_FAILURE);
	} else {
                if(strlen(argv[2]) > 5){
                        fprintf(stderr, "Invalid Port Number");
                        exit(EXIT_FAILURE);
                }

                strncpy(strPortNo, argv[2], sizeof(strPortNo)-1);

                if(!checkIsNumeric(strPortNo)){
                        fprintf(stderr, "Non Numeric Port Number not allowed");
                        exit(EXIT_FAILURE);
                }

                serverPort = atoi(strPortNo);

                if(serverPort < 1024 || serverPort > 65535){
                        fprintf(stderr, "Invalid Port Number");
                        exit(EXIT_FAILURE);
                }

		strncpy(serverIP, argv[1], sizeof(serverIP)-1);

        }

	connectionSocket = make_connection(serverPort, serverIP);

	while(1){
		memset(buffer, '\0', sizeof(buffer)-1);
		recieve_message(buffer, sizeof(buffer));
		printf("%s",buffer );
		fflush(stdout);
		send_message();
	}
	return 0;
}

