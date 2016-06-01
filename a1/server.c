#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

// sigchld_handler, get_in_addr, and main() functions referenced from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html
void sigchld_handler(int s)
{
	int saved_errno = errno;

	while(waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

// Return the IPv4 or IPv6 address of the connecting client in struct sockaddr type.
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Define main() function for stage 1 of the communication.
int main(int argc, char *argv[])
{
	int sockfd, new_fd;  // Listen on sock_fd, new connection on new_fd.
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr; // Client's address information.
	socklen_t sin_size;
	struct sigaction sa;
	int yes=1;
	char s[INET6_ADDRSTRLEN]; // Variable to store the string representation of the client's IP address.
	int rv;

	char buf[10000];
	int numbytes;

	int n_port = atoi(argv[1]);
	
	if ((n_port < 1024) || (n_port > 65535)) {
		fprintf(stderr, "n_port must be between 1024 and 65535.");
		return 1;
	}
	
	// Generate random port number for the stage 2 of the communication - transaction using UDP sockets.
	srand (time(NULL));
	int r_port = (rand() % 64512) + 1024;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	// Get the address info of the client.
	if ((rv = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo() error: %s.", gai_strerror(rv));
		return 1;
	}

	// Loop through all the results and bind to the first client.
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("socket() error.");
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
					sizeof(int)) == -1) {
			perror("setsockopt() error.");
			exit(1);
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("bind() error.");
			continue;
		}

		break;
	}

	// Free the address info variables retrieved.
	freeaddrinfo(servinfo); 

	if (p == NULL)  {
		fprintf(stderr, "Server failed to bind.");
		exit(1);
	}

	if (listen(sockfd, 10) == -1) {
		perror("listen() error.");
		exit(1);
	}

	sa.sa_handler = sigchld_handler; // Reap all dead processes.
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction() error.");
		exit(1);
	}

	while(1) {  // Main loop.
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size); // Accept connection from the client.
		if (new_fd == -1) {
			perror("accept() error.");
			continue;
		}

		// Store the readable IP address of the client in variable s.
		inet_ntop(their_addr.ss_family,
		get_in_addr((struct sockaddr *)&their_addr),
		s, sizeof s);

		while(1) {

			// Connection error.
			if ((numbytes = recv(new_fd, buf, sizeof(buf), 0)) == -1) {
				perror("recv() error.");
				exit(1);
			}

			// Client connection closed.
			else if (numbytes == 0) {
				printf("Connection closed.\n"); // Server still listening to other connections
				break;
			}

			// Connection success.
			printf("\nNegotiation detected. Selected random port %d\n\n", r_port);

			// Send r_port to the client.
			if ((send(new_fd, &r_port, sizeof(int), 0)) == -1) {
				fprintf(stderr, "Sending r_port failed.\n");
			}
			close(new_fd); // Close the TCP connection with the client.
			break;

		}
		
		// Negotiation phase (stage 1) complete. Move to UDP transaction.
		file_transfer(r_port);
		break;
	
	}
	return 0; // Return from the function.
}

// Define function for UDP file transfer.
int file_transfer (int r) {
	struct sockaddr_in client, server; // Declare client, server connections variables.
	int sockfd, n, i=0; // Define variables for socket, client packet length, and loop counter respectively.
	char buf[4]; // 4-byte buffer.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	server.sin_family = AF_INET; // IPv4
	server.sin_port = r; // Port = r_port
	inet_pton(AF_INET, "localhost", &(server.sin_addr)); // Store the string, readable address of localhost.
	bind(sockfd,(struct sockaddr *)&server, sizeof(server)); // Bind.
	n=sizeof(client);

	// Define payload size. Equal to the size of the file, as sent by the client.
	int payload_size;
	recvfrom(sockfd, &payload_size, sizeof(int), 0, (struct sockaddr *)&client, &n);

	// Variable to keep track of the amount of data received.
	int received = 0;

	// Open file for writing.
	FILE *f = fopen("received.txt", "w");

	// Main loop.
	while(1) {
		recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client, &n); // Receive chunk of file data (4-bytes) sent by client.
		fprintf(f, "%s", buf); // Write to the output file.
		for (i = 0; i < sizeof(buf); i++) { // Convert the 4-byte string to uppercase.
			buf[i] = toupper(buf[i]);
		}
		sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&client, n); // Send the acknowledgement (ACK) to client.
		received += sizeof(buf); // Increment received data size with the size of buffer.
		if (received > payload_size) { // If it exceeds the payload size evaluated before, the transaction is complete. Close the socket and break.
			close(sockfd);			
			break; 
		}
	}
	fclose(f); // Close file.
	return 0; // Return from the function.
}
