/*
client.cpp
Author: Siddhant Sutar (sas869)
Description: This source file represents the client program of Go-Back-N (GBN) protocol implementation.
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

using namespace std;

#define WINDOW_SIZE 7 // Define window size. (Max number of packets a window can hold)
#define SEQNUM_COUNT 8 // Define total sequence number values starting from 0

int main (int argc, char *argv[]) {
	
	int raw_send_base = 0; // Raw send base value. (If sequence numbers = {0, ..., 7}, raw_send_base can equal to 8)
	int nextseqnum; // Variable to hold nextseqnum value
	int sent = 0; // Number of sent (not re-sent) packet chunks
	int received = 0; // Number of received ACKs
	int s, r; // File descriptors
	char buf[1024]; // Buffer for receiver
	char chunk[30]; // Buffer to read file chunks
	char file_buf[1024][31]; // Array to store the read file chunks (Can read a a maximum of 1024 chunks of 30 characters each)
	char send_pkt_data[37]; // Char array to store the serialized packet value (37 is the max possible length for a serialized packet sent by client)
    char recv_pkt_data[12]; // Char array to store the deserialized packet value (12 is the max possible length for a deserialized packet received by client)
	struct sockaddr_in sendto_em, recvfrom_em; // sockaddr_in for sending to and receiving from the emulator
	struct hostent *s_he = gethostbyname(argv[1]); // Convert host address
    struct hostent *r_he = gethostbyname("localhost"); // Convert receiving address
	struct timeval tv; // Struct to handle timeout
	socklen_t n1, n2;

	packet *recv_pkt = new packet(-1, -1, -1, recv_pkt_data); // Initialize the packet that contains the received data

	// Timeout of 800ms
	tv.tv_sec = 0;
	tv.tv_usec = 800000; 

	// Create sockets
	s = socket(AF_INET, SOCK_DGRAM, 0);
	r = socket(AF_INET, SOCK_DGRAM, 0);

    // Client: sender
	sendto_em.sin_family = AF_INET;
    sendto_em.sin_port = htons(atoi(argv[2]));
	memcpy(&sendto_em.sin_addr, s_he->h_addr_list[0], s_he->h_length);
	
	// Client: receiver
	recvfrom_em.sin_family = AF_INET;
    recvfrom_em.sin_port = htons(atoi(argv[3]));
	memcpy(&recvfrom_em.sin_addr, r_he->h_addr_list[0], r_he->h_length);
	bind(r,(struct sockaddr *) &recvfrom_em, sizeof(recvfrom_em));
	setsockopt(r, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); // Set the required timeout

    n1 = sizeof(sendto_em);
	n2 = sizeof(recvfrom_em);
	
	ifstream in_file(argv[4], ifstream::binary); // Open the file to be sent
	ofstream seqnum_file("seqnum.log"), ack_file("ack.log"); // Output from the client
		    
	while(1) {			
		
		/* Sending phase: keep sending packets until the window is full. */
		while ((sent < (raw_send_base + WINDOW_SIZE)) && (in_file.read(chunk, sizeof(chunk)).gcount() > 0)) { // Check if there is still more of file to be read
			bzero(send_pkt_data, sizeof(send_pkt_data)); // Clear send_pkt_data buffer
			nextseqnum = sent % SEQNUM_COUNT; // Get the modulo SEQNUM_COUNT sequence number
			strcpy(file_buf[sent], chunk); // Copy the read chunk into the file_buf array
			packet *send_pkt = new packet(1, nextseqnum, strlen(file_buf[sent]), file_buf[sent]); // Initialize the packet to be sent
			send_pkt->serialize(send_pkt_data); // Serialize the packet
			delete send_pkt; // Delete the packet object
			sendto(s, send_pkt_data, strlen(send_pkt_data), 0, (struct sockaddr *) &sendto_em, n1); // Send the serialized version of the packet
			printf("Sent packet %d: %s\n", nextseqnum, send_pkt_data);
			seqnum_file << nextseqnum << endl; // Write the sent seqnum to the seqnum.log
			sent++; // Increment the number of packet chunks 
			bzero(chunk, sizeof(chunk)); // Clear chunk buffer
		}
	
		/* Listening phase */
		bzero(buf, sizeof(buf)); // Clear buffer for receiver
		bzero(recv_pkt_data, sizeof(recv_pkt_data)); // Clear buffer that holds the deserialized received data
		if ((recvfrom(r, buf, sizeof(buf), 0, (struct sockaddr *) &recvfrom_em, &n2) == -1) && (errno == 11)) { // If timeout....
			for (int i=raw_send_base; i<sent; i++) { // Resend the entire window starting from send_base
				nextseqnum = i % SEQNUM_COUNT;
				bzero(send_pkt_data, sizeof(send_pkt_data));
				packet *send_pkt = new packet(1, nextseqnum, strlen(file_buf[i]), file_buf[i]);
				send_pkt->serialize(send_pkt_data);
				delete send_pkt;
				printf("Resent packet %d: %s\n", nextseqnum, send_pkt_data);
				sendto(s, send_pkt_data, strlen(send_pkt_data), 0, (struct sockaddr *) &sendto_em, n1);
				seqnum_file << nextseqnum << endl;
			}
			continue; // After resending all the packets in the window, restart the listening phase
		} 
		else { // If no timeout...
			recv_pkt->deserialize(buf); // Deserialize the received data
			if (recv_pkt->getSeqNum() == (raw_send_base % SEQNUM_COUNT)) { // Compare the sequence number of the received packet with the interpreted raw_send_base value
				printf("Received ACK for packet %d\n", recv_pkt->getSeqNum());
				ack_file << recv_pkt->getSeqNum() << endl; // Write the received ACK's seqnum to ack.log
				raw_send_base++; // Increment the raw send base value
				received++; // Increment the count of received packets
			}
		}
		
		if (received == sent) { // If ACKs for all the packets are received
			
			// Send EOT
			nextseqnum = raw_send_base % SEQNUM_COUNT;
			bzero(send_pkt_data, sizeof(send_pkt_data));
			packet *send_pkt = new packet(3, nextseqnum, 0, NULL);
			send_pkt->serialize(send_pkt_data);
			delete send_pkt;
			sendto(s, send_pkt_data, strlen(send_pkt_data), 0, (struct sockaddr *) &sendto_em, n1);
			printf("Sent EOT\n");
			seqnum_file << nextseqnum << endl;
		
			// Receive EOT
			bzero(buf, sizeof(buf));
			recvfrom(r, buf, sizeof(buf), 0, (struct sockaddr *) &recvfrom_em, &n2);
			recv_pkt->deserialize(buf);
			if (recv_pkt->getType() == 2) { 
				printf("Received EOT\n"); 
				ack_file << recv_pkt->getSeqNum() << endl;
				break;
			}
		
		}
	    
	}
    close(s); // Close sending port
	close(r); // Close receiving port
	seqnum_file.close(); // Close seqnum.log file
	ack_file.close(); // Close ack.log file
	delete recv_pkt; // Delete the dynamically allocated packet object to store the received data
    return 0;
}