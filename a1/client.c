#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

// get_in_addr, and main() functions referenced from http://beej.us/guide/bgnet/output/html/multipage/clientserver.html

// Return the IPv4 or IPv6 address of the connecting client in struct sockaddr type.
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// Define main function for stage 1 of the communication.
int main(int argc, char *argv[])
{
	int sockfd;  
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN]; // Variable to store the string representation of readable IP address.

	char buf[3];
	int numbytes;
	int r_port;

	if (argc != 4) {
		fprintf(stderr, "Usage: ./client <hostname> <port> <filename>\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	// Get the address info of the server.
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo() error: %s.", gai_strerror(rv));
		return 1;
	}

	// Loop through all the results and connect to the first.
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
						p->ai_protocol)) == -1) {
			perror("socket() error.");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("connect() error.");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "Failed to connect.");
		return 2;
	}

	// Store the readable IP address of the server in variable s.
	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
	s, sizeof s);

	// Free the address info variables retrieved.
	freeaddrinfo(servinfo);

	// Negotiation phase: two-way client-server communication

	while(1) { // Main loop.
		
		memcpy(buf, "117", 3); // Copy the negotiation message to be sent into buffer array.

		// Connection failure.
		if ((send(sockfd, buf, strlen(buf), 0)) == -1) {
			fprintf(stderr, "Failure sending negotiation message.");
			close(sockfd);
			exit(1);
		}

		else {
			
			numbytes = recv(sockfd, &r_port, sizeof(int), 0);
			
			// Server connection closed or error.
			if (numbytes <= 0) {
				printf("Either connection closed or error.\n");
			}
			
			// Store the random port (r_port) number sent by the server.
			else {
				printf("\nRandom port: %d\n\n", r_port);
			}
			break;
		}
	}
	close(sockfd); // Close the TCP connection with the server.
	
	// Negotiation phase (stage 1) complete. Move to UDP transaction.
	file_transfer(s, r_port, argv[3]);

}

// Define function for UDP file transfer.
int file_transfer (char hostname[], int r, char filename[]) {
	struct sockaddr_in client, server; // Declare client, server connection variables.
	int sockfd, n; // Define variables for socket, server packet length.
	char buf[4]; // 4-byte buffer.
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	server.sin_family = AF_INET; // IPv4
	server.sin_port = r; // Port = r_port
	inet_pton(AF_INET, hostname, &(server.sin_addr)); // Store the string, readable address of server (hostname).

	n=sizeof(server);
	
	FILE *f = fopen (filename, "r"); // Open file to be sent.

	// Calculate the size of the file in bytes. This will be the payload size sent to the server initially.
	fseek(f, 0L, SEEK_END);
	int size = ftell(f);
	fseek(f, 0L, SEEK_SET);
	sendto(sockfd, &size, sizeof(int), 0, (struct sockaddr *) &server, n);
	
	int c;
	int count = 0; // Variable to keep track of chunks.
	while(1) { // Main loop.
		while ((c = fgetc(f)) != EOF) { // While not reached the end of file..
			buf[count] = c; // Store the current character.
			count++;
			if (count == 4) { // If the buffer is full...
				count = 0; // Reset the count.
				sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &server, n); // Send the 4-byte data.
				recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &server, &n); // Receive the acknowledgement (ACK) data from the server.
				buf[sizeof(buf)] = '\0'; // Line termination/null character.
				printf("%s\n", buf); // Display the ACK.
				bzero(buf, 4); // Reset the buffer (put zero-valued bytes).
				
			}
		}
		if (count != 0) { // The last packet to be sent (if it is not already sent; where size(file) % 4 != 0)
			sendto(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &server, n); // Send the 4-byte data.
			recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *) &server, &n); // Receive the acknowledgement (ACK) data from the server.
			printf("%s\n", buf); // Display the ACK.
		}
		close(sockfd); // Close the socket.
		break; // Break.
	}
	return 0; // Return from the function.
}
