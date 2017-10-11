#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>

#define MAX_CLIENT 1000
#define MAX_FILESIZE 1000
#define GARBAGE_CHAR '^'

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MSG_LEN 10024
#define CURRENT_LEVEL 5
#define LOG 4
#define DEBUG 3
#define ERROR 2
#define SEVERE 1
#define MAX_THREAD 10
#define MAX_USER 1000

pthread_mutex_t connLock;
pthread_mutex_t leaderLock;
int openConnection = 0;

struct sockaddr_in address;
int addrlen = sizeof(address);

int serverPort = 12345;
int recieve_message(char *, int, int);

typedef struct{
	char username[100];
	char password[100];

}CredentialInfo;

typedef struct{
	char playerName[1024];
	int gamePlayed;
	int gameWon;
}LeaderboardInfo;

typedef struct{
        int playerCount;
	LeaderboardInfo leader[MAX_USER];	
}Leaderboard;

Leaderboard leaderboard;
typedef struct{
	int totalCount;
	CredentialInfo credentials[MAX_CLIENT];
}Credentials;

typedef struct{
	char ch;	
	int status; // 0 hide 1 show
}GuessLetter;

typedef struct{
	int letterCount;
	GuessLetter *hangmanLetter;
}LetterHeader;


typedef struct{
	char word[512];
	char type[512];
}HangmanInfo;

typedef struct{
        int totalCount;
        HangmanInfo hangman[MAX_FILESIZE];
}Hangman;

Hangman hangmanData;

Credentials creds;

void addPlayerGame(char *playerName){
	pthread_mutex_lock(&leaderLock);
	strcpy(leaderboard.leader[leaderboard.playerCount].playerName, playerName);
	leaderboard.leader[leaderboard.playerCount].gamePlayed = 1;
	leaderboard.leader[leaderboard.playerCount].gameWon = 0;
	leaderboard.playerCount += 1;
	pthread_mutex_unlock(&leaderLock);
}

void incrementPlayerGame(char *playerName){
	int indx = 0;

	for(; indx < leaderboard.playerCount; indx++){
        	if(strcmp(leaderboard.leader[indx].playerName, playerName) == 0){
        		leaderboard.leader[indx].gamePlayed += 1;
		}
	}
}

void incrementPlayerWon(char *playerName){
        int indx = 0;

        for(; indx < leaderboard.playerCount; indx++){
                if(strcmp(leaderboard.leader[indx].playerName, playerName) == 0){
                        leaderboard.leader[indx].gameWon += 1;
                }
        }
}



void release_resource(int communicationSocket){
	if(communicationSocket!=-1){
		close(communicationSocket);
	}

	pthread_mutex_lock(&connLock);
	openConnection--;
	pthread_mutex_unlock(&connLock);
}

int check_numeric(char *string){
	int indx = 0;

	for(indx = 0; indx < strlen(string); indx++){
		if(!isdigit(string[indx])){
			return 0;
		}
	}

	return 1;
}

int send_message(char *msg, int communicationSocket){
	// First send length
	char sendMsg[MSG_LEN] = "";
	int indx = 0;
	memset(sendMsg, '\0', sizeof(sendMsg));
	strcpy(sendMsg, msg);

	for(indx = strlen(msg); indx < MSG_LEN-1; indx++){
		sendMsg[indx] = GARBAGE_CHAR;
	}
	
	if(send(communicationSocket, sendMsg, MSG_LEN, 0 ) >= 0){
		return 1;
	}
		

	return 0;
}

char *trim(char *str){
        int indx = 0;

        for(indx = strlen(str)-1; indx >= 0; indx--){
                if(str[indx] != ' ' && str[indx] != '\t' && str[indx] != '\r' && str[indx] != '\n'){
                        break;
                }
        }

        str[indx+1] = '\0';

        return str;
}


int createConnection(int serverPort){
	int serverFd;
        int opt = 1;

	// Creating socket file descriptor
        if ((serverFd = socket(AF_INET, SOCK_STREAM, 0)) == 0)
        {
                perror("socket failed");
                exit(EXIT_FAILURE);
        }

        // Forcefully attaching socket to the port
        if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
        {
                perror("setsockopt");
                exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons( serverPort );

        // Forcefully attaching socket to the port 8080
        if (bind(serverFd, (struct sockaddr *)&address, sizeof(address))<0)
        {
                perror("bind failed");
                exit(EXIT_FAILURE);
        }

	return serverFd;
}

void load_credentials(){
	FILE *authenticatioFile = fopen("Authentication.txt", "r");
	char *line, *pch;
	size_t len = 0;

	if(authenticatioFile == NULL){
		perror("Authentication File");
		exit(EXIT_FAILURE);
	}

	getline(&line, &len, authenticatioFile); //ignore header
	while (getline(&line, &len, authenticatioFile) != -1) {
		pch = trim(strtok (line, "\t"));
		strcpy(creds.credentials[creds.totalCount].username, pch);
		pch = trim(strtok (NULL, "\t"));
		strcpy(creds.credentials[creds.totalCount].password, pch);
		creds.totalCount++;
	}

	fclose(authenticatioFile);
	if (line)
		free(line);
}

void load_hangman_data(){
	FILE *hangmanFile = fopen("hangman_text.txt", "r");
	char *line, *pch;
        size_t len = 0;

        if(hangmanFile == NULL){
                perror("Hangman File");
                exit(EXIT_FAILURE);
        }

        while (getline(&line, &len, hangmanFile) != -1) {
                pch = trim(strtok (line, ","));
                strcpy(hangmanData.hangman[hangmanData.totalCount].type, pch);
                pch = trim(strtok (NULL, ","));
                strcpy(hangmanData.hangman[hangmanData.totalCount].word, pch);
                hangmanData.totalCount++;
        }

        fclose(hangmanFile);
        if (line)
                free(line);

}

int recieve_message(char *buffer, int length, int communicationSocket){
	char received[MSG_LEN];
	int indx;
	memset(received, '\0', sizeof(received));


	memset(buffer, '\0', length-1);
	if(read(communicationSocket, received, MSG_LEN) < 0){
		release_resource(communicationSocket);
        }

	for(indx = 0; indx < MSG_LEN-1; indx++){
		if(received[indx] == GARBAGE_CHAR){
			received[indx] = '\0';	
			break;
		}
	}	

	strcpy(buffer, received);
	trim(buffer);
	

	return 1;
}

char *authenticate_user(int communicationSocket){
	char *user = (char *)malloc(sizeof(char) * 100);
	char pass[100];
	int indx = 0;
	send_message("Username:", communicationSocket);
	recieve_message(user, sizeof(user), communicationSocket);
	
	send_message("Password:", communicationSocket);
        recieve_message(pass, sizeof(pass), communicationSocket);

	for(indx = 0; indx < creds.totalCount; indx++){
		
		if(strcmp(creds.credentials[indx].username, user) == 0 && strcmp(creds.credentials[indx].password, pass) == 0){
			return user;	
		}
	}

	return NULL;
}

int fetch_word_index(){
	time_t tm;
	srand((unsigned) time(&tm));
	return rand() % hangmanData.totalCount;
}

LetterHeader *create_letter(int hangmanIndex){
	int wordSize, typeSize, indx, letterIndx = 0;
	LetterHeader *hangman;
	wordSize = strlen(hangmanData.hangman[hangmanIndex].word);
	typeSize = strlen(hangmanData.hangman[hangmanIndex].type);
	hangman = (LetterHeader *)malloc(sizeof(LetterHeader));
        hangman->hangmanLetter = (GuessLetter *)malloc((wordSize + typeSize + 2) * sizeof(GuessLetter));
	hangman->letterCount = wordSize+typeSize+1;

	for(indx = 0; indx < typeSize; indx++){
		hangman->hangmanLetter[letterIndx].ch = hangmanData.hangman[hangmanIndex].type[indx];
		hangman->hangmanLetter[letterIndx].status = 0;
		letterIndx++;
	}

	hangman->hangmanLetter[letterIndx].ch = ' ';
	hangman->hangmanLetter[letterIndx].status = 1;
	letterIndx++;

	for(indx = 0; indx < wordSize; indx++){
                hangman->hangmanLetter[letterIndx].ch = hangmanData.hangman[hangmanIndex].word[indx];
                hangman->hangmanLetter[letterIndx].status = 0;
                letterIndx++;
        }

	hangman->hangmanLetter[letterIndx].ch = '\0';
        hangman->hangmanLetter[letterIndx].status = 1;
	

	return hangman;
}

char *prepare_word_to_send(LetterHeader *hangman){
	int indx = 0, wordIndx = 0;
	char *letter = (char *)malloc((hangman->letterCount+10)*sizeof(char));
	memset(letter, '\0', (hangman->letterCount+10)*sizeof(char));
	
	for(indx = 0; hangman->hangmanLetter[indx].ch != '\0'; indx++){
		
		if(hangman->hangmanLetter[indx].status == 0){
			letter[wordIndx] = '_';
			wordIndx++;
			letter[wordIndx] = ' ';
		}else{
			letter[wordIndx] =  hangman->hangmanLetter[indx].ch;
			wordIndx++;
			letter[wordIndx] = ' ';
		}
	}
	return letter;
}

int reveal_letter(LetterHeader *hangman, char ch){
	int indx = 0, completed = 1;

        for(indx = 0; hangman->hangmanLetter[indx].ch != '\0'; indx++){
                if(hangman->hangmanLetter[indx].ch == ch){
			hangman->hangmanLetter[indx].status = 1;
                }

		if(hangman->hangmanLetter[indx].status == 0){
			completed = 0;
		}
        }

	return completed;
}

char fetchGuessChar(char *str){
	int indx = 0, length = strlen(str);
	char ch = '\0';

	for(indx = 0; indx < length; indx++){
		if((str[indx] >= 'a' && str[indx] <='z') || (str[indx] >= 'A' && str[indx] <='Z')){
			ch = tolower(str[indx]);
		}
	}
	
	return ch;
}

void game_won(char *msg, int communicationSocket){
	char sndMsg[MSG_LEN];
	char tempMsg[MSG_LEN];
	memset(sndMsg, '\0', sizeof(sndMsg));
	memset(tempMsg, '\0', sizeof(tempMsg));

	sprintf(sndMsg, "%s\nWeldone. You won this round.\nPress any key continue...\n", msg);
	send_message(sndMsg, communicationSocket);
	recieve_message(tempMsg, sizeof(tempMsg), communicationSocket);
}

void game_loss(int communicationSocket){
	char tempMsg[MSG_LEN];
        memset(tempMsg, '\0', sizeof(tempMsg));

	send_message("\nBad luck!! You lose. Better luck next time\n", communicationSocket);
	recieve_message(tempMsg, sizeof(tempMsg), communicationSocket);
}

void start_game(int communicationSocket, char *player){
	int hangmanIndex, totalGuess, count = 0;
	LetterHeader *hangman;
	char *word;
	char msg[MSG_LEN];
	char attempt;
	char guessList[30];
	int guessIndex = 0;
	int completed = 0;
	incrementPlayerGame(player);

	memset(guessList, '\0', sizeof(guessList)-1);

	hangmanIndex = fetch_word_index();
	snprintf(msg, 100, "Hangman Index:%d", hangmanIndex);
	totalGuess = MIN(strlen(hangmanData.hangman[hangmanIndex].type) + strlen(hangmanData.hangman[hangmanIndex].word) + 10, 26); 
	hangman = create_letter(hangmanIndex);

	for(count = 0; count < totalGuess; count++){
		word = prepare_word_to_send(hangman);
		memset(msg, '\0', sizeof(msg)-1);
		snprintf(msg, MSG_LEN-1, "\n\n=======================================\n\nGuessed Letters:%s\nNo of guesses left:%d\nWord:%s\nEnter your guess -", guessList, totalGuess-count, word);
		send_message(msg, communicationSocket);

		recieve_message(msg, sizeof(msg)-1, communicationSocket);
		attempt = fetchGuessChar(msg);

		if(attempt == '\0'){
			send_message("Invalid attempt: Press key to continue\n", communicationSocket);
			recieve_message(msg, sizeof(msg)-1, communicationSocket);
			count--;
			continue;
		}
	
		guessList[guessIndex] = attempt;
		guessIndex++;

		completed = reveal_letter(hangman, attempt);
		
		if(completed){
			break;
		}
	}

	if(completed){
		incrementPlayerWon(player);
		word = prepare_word_to_send(hangman);
		memset(msg, '\0', sizeof(msg)-1);
		snprintf(msg, MSG_LEN-1, "Guessed Letters:%s\nNo of guesses left:%d\nWord:%s\n", guessList, totalGuess, word);

		game_won(msg, communicationSocket);
	}else{
		game_loss(communicationSocket);
	}
}

void sort_leaderBoard(Leaderboard *board){
	LeaderboardInfo temp;
	int i = 0 , j = 0, n = board->playerCount;
	for (i = 1; i < n; i++)
		for (j = 0; j < n - i; j++) {
			if (board->leader[j].gameWon > board->leader[j + 1].gameWon) {
				temp = board->leader[j];
				board->leader[j] = board->leader[j + 1];
				board->leader[j + 1] = temp;
			}
		}	

}

void show_leaderboard(int communicationSocket){
	Leaderboard currentBoard = leaderboard;	
	char msg[MSG_LEN], tmpMsg[MSG_LEN];
	int indx = 0;
	sort_leaderBoard(&currentBoard);

	memset(msg, '\0', sizeof(msg));
	strcpy(msg, "");
	for(indx = 0; indx < currentBoard.playerCount; indx ++){
		sprintf(tmpMsg, "\n\n=========================================================\n\nPlayer - %s\nNumber of games won - %d\nNumber of games played - %d\n", currentBoard.leader[indx].playerName, currentBoard.leader[indx].gameWon, currentBoard.leader[indx].gamePlayed);
		strcat(msg, tmpMsg);
	}
	strcat(msg, "\n\n=========================================================\n\nPress any key to continue:");
	
	send_message(msg, communicationSocket);
	memset(tmpMsg, '\0', sizeof(tmpMsg));	
	recieve_message(tmpMsg, sizeof(tmpMsg), communicationSocket);

}

void quit_game(int connectionSocket){
	release_resource(connectionSocket);
}

void *game_thread(void *arg){
	int *srvFd = (int *)arg;
	char playOption[8];
	int communicationSocket;
	int serverFd = *srvFd;
	int currentConnection = 0;
	char *player = NULL;

	if ((communicationSocket = accept(serverFd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0)
        {
                perror("accept");
		release_resource(-1);
        }

	pthread_mutex_lock(&connLock);
	currentConnection = openConnection;
	
	if(currentConnection < MAX_THREAD){
        	openConnection++;
	}
        pthread_mutex_unlock(&connLock);

	if(currentConnection >= MAX_THREAD){
		 send_message("All connection are busy. Please try again later", communicationSocket);
		 release_resource(-1);
		 return NULL;
        }

	player = authenticate_user(communicationSocket);
        if(player == NULL){
                send_message("Authentication failed", communicationSocket);
                release_resource(communicationSocket);
		return NULL;
        }

	addPlayerGame(player);
	
	while(1){
		send_message("Welcome to the Hangman Gaming System\n\nPlease enter a selection\n<1> Play Hangman\n<2> Show Leaderboard\n<3> Quit\nSelection option 1-3 ->", communicationSocket);

		recieve_message(playOption, sizeof(playOption), communicationSocket);


		if(strcmp(playOption, "1") == 0){
			start_game(communicationSocket, player);
		}else if(strcmp(playOption, "2") == 0){
			show_leaderboard(communicationSocket);
		}else if(strcmp(playOption, "3") == 0){
			quit_game(communicationSocket);
			return NULL;
		}
	}
	return NULL;
}

void initiate_gaming_server(){
	int serverFd = 0;
	pthread_t tid;
	
	serverFd = createConnection(serverPort);

	while(1){
		if (listen(serverFd, 3) < 0)
		{
			perror("listen");
			exit(EXIT_FAILURE);
		}

		pthread_create(&tid, NULL, game_thread, (void *)(&serverFd));
	}
}

void sighandler(int signum)
{
	release_resource(-1);
	signal(SIGPIPE, sighandler);
}

int main(int argc, char const *argv[])
{
	signal(SIGPIPE, sighandler);
	char strPortNo[8];


	if(argc == 2){
		if(strlen(argv[1]) > 5){
			fprintf(stderr, "Invalid Port Number\n");
			exit(EXIT_FAILURE);
		}

		strncpy(strPortNo, argv[1], sizeof(strPortNo)-1);	

		if(!check_numeric(strPortNo)){
			fprintf(stderr, "Non Numeric Port Number not allowed\n");
                        exit(EXIT_FAILURE);
		}

		serverPort = atoi(strPortNo);
	
		if(serverPort < 1024 || serverPort > 65535){
			fprintf(stderr, "Invalid Port Number");
                        exit(EXIT_FAILURE);
		}
	
	}

	load_credentials();
	load_hangman_data();


	initiate_gaming_server();

	return 0;
}

