/**

    Author:  Athanasios Kirmizis
    Dept.:   EE AUTH
    AEM :    8835
    Course:  Real Time Embedded Systems
	Season:  2019 - 2020
    E-mail : athakirm@ece.auth.eng.gr
    Prof:    Nikolaos Pitsianis | Dimitrios Floros

    Raspberry Pi Zero W p2p messaging exchange application

**/

#include <unistd.h> 
#include <stdio.h> 
#include <sys/socket.h> 
#include <stdlib.h> 
#include <netinet/in.h> 
#include <string.h> 
#include <pthread.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>

#define PORT 2288
#define AEM_LIST_LEN 5
#define BUFFER_SIZE 2000
#define MAX_MESSAGE_LEN 277

/**
/Declarations
**/

char *extract_receiver(char *msg);
char *extract_sender(char *msg);
char *AEM_to_IP(char *AEM);
void alarm_handler(int signlNum);
char *generate_msg();
int check_for_duplicates(char *msg);
int check_for_mine_duplicates(char *msg);
void *server_thread();
void *client_thread(void *i);
void server_connection_handler(int socket_desc);
void client_connection_handler(int socket_desc, int position);
void save_session_stats(int fd, int flag, char *ip);

/**
/Global Variables
**/

char all_messages_buffer[BUFFER_SIZE][MAX_MESSAGE_LEN];
int buffer_pointer = 0;
char messages_towards_me[BUFFER_SIZE][MAX_MESSAGE_LEN];
int mine_counter = 0;
int not_sent_yet_pointer[AEM_LIST_LEN];
int start_looking_flag = 0;

char new_message_text[30] = "New message by thanosthehuge!";
char host_AEM[5] = "8835";
char AEM_list[AEM_LIST_LEN][5] = {"8915", "8860", "8821", "1122", "3344"};

static FILE* f1;
static FILE* f2;
static FILE* f3;

/**
/The main function.
**/

int main(){
	
	//Set random seed
	srand(time(NULL));
	
	//Setup SIGALRM handler to generate messages at random times
    struct sigaction sign ;

    memset(&sign,0,sizeof(sign));
    sign.sa_handler = &alarm_handler;
	sign.sa_flags = SA_RESTART;

    sigaction(SIGALRM,&sign,NULL);

	//Setup inital timer between 0.5 and 2 minutes
	struct itimerval timer;
	int r = (rand() % 91) + 30;
		
    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = r;

    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    setitimer(ITIMER_REAL,&timer,NULL);
	
	//Setup ONE server thread for constant listening
	pthread_t srv_thread;
	
	if( pthread_create( &srv_thread , NULL , server_thread , NULL) < 0) {
		
		perror("could not create server thread");
		return 1;
	}
	puts("Server thread created!");
	
	while(1) {
		
		//If a message has arrived to the "ring" buffer, start looking for other nodes to connect
		if(start_looking_flag == 1) {
			
			//Setup AEM_LIST_LEN client threads to talk with the many existing servers (nodes)
			pthread_t clnt_thread[AEM_LIST_LEN];
			
			for(int i=0; i<AEM_LIST_LEN; i++) {
				
				if( pthread_create( &clnt_thread[i] , NULL , client_thread , (void *)i ) < 0) {
					
					perror("could not create client thread");
					return 1;
				}
				printf("Client thread created for AEM: %s\n", AEM_list[i]);
			}
			
			//Join the client threads
			for(int i=0; i<AEM_LIST_LEN; i++) {
				pthread_join(clnt_thread[i], NULL);
			}
			
			start_looking_flag = 0;
		}
	}
	
	free(alarm_handler);
	
	puts("Finishing now...\n");
	
    return 0;
}

/**
/The thread responsible to implement the 'server' part of the node.
**/

void *server_thread(){
	
	int socket_desc , new_socket , c , *new_sock;
	struct sockaddr_in server , client;
	
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1) {
		
		puts("Could not create socket");
	}
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( PORT );
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0) {
		
		puts("bind failed");
		return 0;
	}
	puts("bind done");
	
	//Listen
	listen(socket_desc , 5);
	
	//Accept incoming connection
	puts("Waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	
	while( (new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) ) {
		
		puts("Connection accepted");
		
		//Keep the stats for starting of session
		save_session_stats(1, 0, inet_ntoa(client.sin_addr));
			
		//Handle the connection appropriately
		server_connection_handler(new_socket);
		
		//Keep the stats for ending of session
		save_session_stats(2, 0, inet_ntoa(client.sin_addr));
	}
	
	if (new_socket<0) {
		
		perror("accept failed");
		return 0;
	}
	
}

/**
/The thread responsible to implement the 'client' part of the node.
/Argument i is the position of the chosen AEM in the AEM_list (passed by reference).
**/

void *client_thread(void *i){
	
	int sock = 0, valread; 
	struct sockaddr_in serv_addr; 
	
	//Extract the AEM position argument and the corresponding AEM
	int pos = (int) i;
	char server_AEM[5];
	strcpy(server_AEM, AEM_list[pos]);
	
	//Convert the AEM to IP address
	char server_IP[11];
	char *temp = malloc(11*sizeof(char));
	temp = AEM_to_IP(server_AEM);
	strcpy(server_IP, temp);
	
	//Create socket
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
	
		puts("\n Socket creation error \n"); 
		return 0; 
	} 

	//Prepare the sockaddr_in structure
	serv_addr.sin_family = AF_INET; 
	serv_addr.sin_port = htons(PORT); 
	
	//Convert IPv4 and IPv6 addresses from text to binary form 
	if(inet_pton(AF_INET, server_IP, &serv_addr.sin_addr)<=0) { 
	
		puts("\nInvalid address/ Address not supported \n"); 
		return 0; 
	} 

	//Connect
	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { 
	
		puts("\nConnection Failed"); 
		return 0; 
	} 
	else {
		
		//Keep the stats for starting of session
		save_session_stats(1, 1, inet_ntoa(serv_addr.sin_addr));
			
		//Handle the connection appropriately
		client_connection_handler(sock, pos);
		
		//Keep the stats for ending of session
		save_session_stats(2, 1, inet_ntoa(serv_addr.sin_addr));
	}
}

/**
/Helper function responsible of handling the events after the host node has been connected to a client.
/Argument socket_desc is the socket descriptor of the socket created for the specific communication.
**/

void server_connection_handler(int socket_desc){
	
	int read_size;
	char client_message[MAX_MESSAGE_LEN];
	
	//Receive all messages from client
	while(read_size = recv(socket_desc , client_message , MAX_MESSAGE_LEN , 0) > 0) {
		
		printf("Message received: %s\n", client_message);

		//Copy message to other buffer for safe processing
		char safe_buffer[MAX_MESSAGE_LEN];
		strcpy(safe_buffer, client_message);
		
		//Extract the receiver of the message
		char *tmp = malloc(5*sizeof(char));
		char receiver[5];
		tmp = extract_receiver(safe_buffer);
		strcpy(receiver, tmp);
		
		//If the message is for me, keep it on a seperate buffer
		if(strcmp(receiver, host_AEM) == 0) {
			
			printf("MY PRECIOUS!\n");
			
			if(check_for_mine_duplicates(safe_buffer) == 0) {
				
				//Store message to list
				strcpy(messages_towards_me[mine_counter], safe_buffer);

				f3 = fopen("mine.txt","a");
				fprintf(f3,"Message %d: %s\n", mine_counter+1, messages_towards_me[mine_counter]);
				fclose(f3);
				
				mine_counter++;
				
				//If buffer has reached the limit of 2000 (highly unprobable), go back to beginning of the "ring"
				if(mine_counter == BUFFER_SIZE) {
				
					mine_counter = 0;
				}
			}
			else {
			
				//Message is already in list
				puts("Message is duplicate!");
			}
		}
		//Else if the message is for someone else, store it to the ring buffer with all the other messages and update the 'start looking' flag
		else {
			
			printf("NOT MINE!\n");
			 		
			if(check_for_duplicates(safe_buffer) == 0) {
				
				//Store message to list
				strcpy(all_messages_buffer[buffer_pointer], safe_buffer);
				buffer_pointer++;
				
				//If buffer has reached the limit of 2000, go back to beginning of the "ring" and also reset all the 'not_sent_yet' pointers
				if(buffer_pointer == BUFFER_SIZE) {
				
					buffer_pointer = 0;
					for(int i=0; i<AEM_LIST_LEN; i++) {
						
						not_sent_yet_pointer[i] = 0;
					}
				}
				
				//Update the 'start looking' flag to forward the message to other nodes
				start_looking_flag = 1;
			}
			else {
				
				//Message is already in list
				puts("Message is duplicate!");
			}
		}
		
		//Clear some buffers
		memset(receiver, 0, 5*sizeof(receiver[0]));
		free(tmp);
		memset(client_message, 0, MAX_MESSAGE_LEN*sizeof(client_message[0]));
		
		//Send ack signal to receive next message
		char ack[2];
		send(socket_desc, ack, 2, 0);
	}

	//Disconnection or failed to receive cases
	if(read_size == 0) {
		
		puts("Client disconnected\n");
		fflush(stdout);
	}
	else if(read_size == -1) {
		
		perror("recv failed");
	}
}

/**
/Helper function responsible of handling the events after the host node has been connected to a server.
/Argument socket_desc is the socket descriptor of the socket created for the specific communication.
/Argument position is an integer pointing at the position of the specific AEM in the array AEM_list
**/

void client_connection_handler(int socket_desc, int position){
	
	char server_message[MAX_MESSAGE_LEN];
	char ack[2];
	
	//Extract the buffer pointer for the last sent message to this server's AEM
	int pointer = not_sent_yet_pointer[position];
	
	//Send all the new messages from the last time that this server node talked with host node
	for(int i=pointer; i<buffer_pointer; i++) {
		
		strcpy(server_message, all_messages_buffer[i]);
		send(socket_desc, server_message, strlen(server_message) + 1, 0);
		printf("Message sent: %s\n", server_message);
		
		memset(server_message, 0, MAX_MESSAGE_LEN*sizeof(server_message[0]));
		
		//Receive ack signal to send next message
		recv(socket_desc, ack, 2, 0);
	}
	
	//After sending all the new messages, update this node's 'not_sent_yet' pointer
	not_sent_yet_pointer[position] = buffer_pointer;
	
	//And finally close the socket
	close(socket_desc);
}

/**
/Helper function responsible of saving statistics of the duration of the session with each connected node.
/Argument fd is the file descriptor to save stats in. File f1 keeps starting times and file f2 keeps ending times.
/Argument flag dictates whether the connection was made as a server (flag = 0) or as a client (flag = 1) from this node's standpoint. This only affects the note text to be saved.
/Argument ip is the IP of the node connected with this node during the session to keep stats for.
**/

void save_session_stats(int fd, int flag, char *ip){
	
	//Get date & time and IP (AEM) for statistics
	char *date_buffer = malloc(30);
	char ms_str[4];
	struct timeval sample;
	gettimeofday(&sample,NULL);
	time_t exact_time = sample.tv_sec;
	time_t exact_us = sample.tv_usec;
	int ms = (exact_us / 1000);
	strftime(date_buffer,30,"%m-%d-%Y | %T.",localtime(&exact_time));
	if(ms<10){
			sprintf(ms_str, "00%d", ms);
		}
	else if(ms<100) {
			sprintf(ms_str, "0%d", ms);
		}
	else {
			sprintf(ms_str, "%d", ms);
		}
	strcat(date_buffer, ms_str);
	
	//Open corresponding file to write above stats
	if(flag == 0) {
		
		if(fd == 1) {
			
			f1 = fopen("start.txt","a");
			fprintf(f1,"Node with IP %s connected at: %s\n", ip, date_buffer);
			fclose(f1);
		}
		else if(fd == 2) {
			
			f2 = fopen("end.txt","a");
			fprintf(f2,"Node with IP disconnected %s at: %s\n", ip, date_buffer);
			fclose(f2);
		}
		
	}
	else if(flag == 1) {
		
		if(fd == 1) {
			
			f1 = fopen("start.txt","a");
			fprintf(f1,"Connected to node with IP %s at: %s\n", ip, date_buffer);
			fclose(f1);
		}
		else if(fd == 2) {
			
			f2 = fopen("end.txt","a");
			fprintf(f2,"Disconnected from node with IP %s at: %s\n", ip, date_buffer);
			fclose(f2);
		}
	}
	memset(date_buffer, 0, 30);
}

/**
/Helper function responsible of converting an AEM to an IP (as strings).
/Argument AEM is the AEM to be converted in string format.
**/

char *AEM_to_IP(char *AEM){
	
	if(sizeof(AEM) > 4*sizeof(char*)) {
		
		return "ERR";
	}
	else {
		
		char *final = malloc(11*sizeof(char));
		char prefix[6] = "10.0.";
		char xx[3] = {AEM[0],AEM[1]};
		char yy[3] = {AEM[2],AEM[3]};
		strcpy(final,prefix);
		strcat(final, xx);
		strcat(final, ".");
		strcat(final, yy);
		
		return final;
	}
}

/**
/Helper function responsible of extracting the sender's AEM from a message.
/Argument msg is the message to be parsed.
**/

char *extract_sender(char *msg){
	
	char *tmp_sender = malloc(5*sizeof(char));
	for(int i = 0; i<4; i++) {
		
		tmp_sender[i] = msg[i];
	}
	tmp_sender[4] = '\0';
	
	return tmp_sender;
}

/**
/Helper function responsible of extracting the receiver's AEM from a message.
/Argument msg is the message to be parsed.
**/

char *extract_receiver(char *msg){
	
	char *tmp_receiver = malloc(5*sizeof(char));
	for(int i = 5; i<9; i++){
		
		tmp_receiver[i-5] = msg[i];
	}
	tmp_receiver[4] = '\0';
	
	return tmp_receiver;
}

/**
/Handler function of the SIGALRM signal. Adds a new generated message to the list of messages and resets a random timer.
/Argument signlNum is the signal number in the POSIX library.
**/

void alarm_handler(int signlNum){
	
	//Create a new message
	char msg_to_add[MAX_MESSAGE_LEN];
	char* temp_msg = malloc(MAX_MESSAGE_LEN*sizeof(char));
	temp_msg = generate_msg();
	strcpy(msg_to_add, temp_msg);
	
	printf("Message generated: %s\n", msg_to_add);
	
	//Store the message to the "ring" buffer with all the messages
	strcpy(all_messages_buffer[buffer_pointer], msg_to_add);
	buffer_pointer++;
	
	//If buffer has reached the limit of 2000, go back to beginning of the "ring" and also reset all the 'not_sent_yet' pointers
	if(buffer_pointer == BUFFER_SIZE) {
	
		buffer_pointer = 0;
		for(int i=0; i<AEM_LIST_LEN; i++) {
			
			not_sent_yet_pointer[i] = 0;
		}
	}
	
	start_looking_flag = 1;
	
	//Setup next random timer
	struct itimerval timer ;
	int r = (rand() % 91) + 30;
	
    timer.it_value.tv_usec = 0;
    timer.it_value.tv_sec = r;

    timer.it_interval.tv_usec = 0;
    timer.it_interval.tv_sec = 0;

    setitimer(ITIMER_REAL,&timer,NULL);
	
	free(temp_msg);
}

/**
/Helper function responsible of generating a message at a random point in time, according to the specified format.
**/

char *generate_msg(){
	
	char *msg_to_return = malloc(MAX_MESSAGE_LEN*sizeof(char));
	
	int r = rand() % AEM_LIST_LEN;
	char lin_tmstmp[41];
	sprintf(lin_tmstmp, "%d", (int)time(NULL));
	
	strcpy(msg_to_return, host_AEM);
	strcat(msg_to_return, "_");
	strcat(msg_to_return, AEM_list[r]);
	strcat(msg_to_return, "_");
	strcat(msg_to_return, lin_tmstmp);
	strcat(msg_to_return, "_");
	strcat(msg_to_return, new_message_text);
	
	return msg_to_return;
}

/**
/Helper function responsible of checking whether a message already exists in the ring buffer with all the messages.
/Argument msg is the message to be checked.
**/

int check_for_duplicates(char *msg){
	
	int ret_flag = 0;
	
	for(int i=0; i<buffer_pointer; i++){
		
		if(strcmp(all_messages_buffer[i], msg) == 0){
			
			ret_flag = 1;
			break;	
		}
	}
	
	return ret_flag;
}

/**
/Helper function responsible of checking whether a message already exists in the buffer with 'my' messages.
/Argument msg is the message to be checked.
**/

int check_for_mine_duplicates(char *msg){
	
	int ret_flag = 0;
	
	for(int i=0; i<mine_counter; i++){
		
		if(strcmp(messages_towards_me[i], msg) == 0){
			
			ret_flag = 1;
			break;	
		}
	}
	
	return ret_flag;
}
