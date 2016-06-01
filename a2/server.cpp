/*
server.cpp
Author: Siddhant Sutar (sas869)
Description: This source file represents the server program of Go-Back-N (GBN) protocol implementation.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fstream>
#include "packet.cpp"

#define SEQNUM_COUNT 8 // Define total sequence number values starting from 0

using namespace std;

int main(int argc, char *argv[]) {

	int nextseqnum = 0; // Initialize the variable that keeps the track of expected seqnum
	int s, r; // File descriptors
	char buf[1024]; // Buffer for receiver
	char send_pkt_data[12]; // Char array to store the serialized packet value (12 is the max possible length for a serialized packet sent by server)
	char recv_pkt_data[37]; // Char array to store the deserialized packet value (37 is the max possible length for a deserialized packet received by client)
    struct sockaddr_in sendto_em, recvfrom_em; // sockaddr_in for sending to and receiving from the emulator
	struct hostent *s_he = gethostbyname(argv[1]); // Convert host address
    struct hostent *r_he = gethostbyname("localhost"); // Convert receiving address
	socklen_t n1, n2;
	
	packet *recv_pkt = new packet(-1, -1, -1, recv_pkt_data); // Initialize the packet that contains the received data

	// Create sockets
	s = socket(AF_INET, SOCK_DGRAM, 0);
	r = socket(AF_INET, SOCK_DGRAM, 0);
	
	// Server: receiver
	recvfrom_em.sin_family = AF_INET;
    recvfrom_em.sin_port = htons(atoi(argv[2]));
	memcpy(&recvfrom_em.sin_addr, r_he->h_addr_list[0], r_he->h_length);
	bind(r,(struct sockaddr *) &recvfrom_em, sizeof(recvfrom_em));
	
	// Server: sender
    sendto_em.sin_family = AF_INET;
    sendto_em.sin_port = htons(atoi(argv[3]));
	memcpy(&sendto_em.sin_addr, s_he->h_addr_list[0], s_he->h_length);

    n1 = sizeof(sendto_em);
	n2 = sizeof(recvfrom_em);
	
	ofstream arrival_file("arrival.log"), out_file(argv[4]); // Output files to store the arriving sequence numbers and the file sent by client respectively

    while(1) {
		
		// Clear buffers
		bzero(buf, sizeof(buf));
		bzero(send_pkt_data, sizeof(send_pkt_data));
		bzero(recv_pkt_data, sizeof(recv_pkt_data));
		
		// Receive data
		recvfrom(r, buf, sizeof(buf), 0, (struct sockaddr *) &recvfrom_em, &n2);
		recv_pkt->deserialize(buf);
		
		// Write the sequence number of the arrived packet to arrival.log
		arrival_file << recv_pkt->getSeqNum() << endl;
		
		// If client EOT received, send server EOT
		if (recv_pkt->getType() == 3) {
			packet *send_pkt = new packet(2, recv_pkt->getSeqNum(), 0, NULL);
			send_pkt->serialize(send_pkt_data);
			delete send_pkt;
			sendto(s, send_pkt_data, strlen(send_pkt_data), 0, (struct sockaddr *) &sendto_em, n1);
			break;
		}
					
		// Send ACK if the sequence number of arriving packet matches the expected 
		if (recv_pkt->getSeqNum() == (nextseqnum % SEQNUM_COUNT)) {
			packet *send_pkt = new packet(0, recv_pkt->getSeqNum(), 0, NULL);
			send_pkt->serialize(send_pkt_data);
			delete send_pkt;
			sendto(s, send_pkt_data, strlen(send_pkt_data), 0, (struct sockaddr *) &sendto_em, n1);
			out_file << recv_pkt->getData(); // Write the received data to the output file
			nextseqnum++;
		}
		
	}
    close(s); // Close sending port
	close(r); // Close receiving port
	delete recv_pkt; // Delete the dynamically allocated packet object to store the received data
    return 0;
}