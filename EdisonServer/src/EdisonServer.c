/*
 * EdisonServer.c
 * Author: Aaron Needles
 * Copyright (c) 2015 Aaron Needles.
 *
 * Desc:
 * TCP connection, command handler
 * UDP one-way data update, configurable clocking
 * Intel Edison with Arduino breakout board and Grove base shield
 *
 *
 * Issues:
 *   On disconnect from remote .NET client on Windows, the process exits without indicating why.
 *   Putty client disconnect does not have the same issue, and the process drops back to
 *   "Waiting for accept()..."
 *   For Putty use "raw", target IP = <your Edison IP>, Port = 10000.
 *   Once connected, press h for help and q to disconnect.
 *
 *   MRAA library causes WiFi interference.
 *
 */

// For standard stuff
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// For socket I/O
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// For digital and analog I/O
#include "localIO.h"

// For TCP socket handling
#define LEN_TCP_BUFFER 256
int init_sockfd, tcp_sockfd, tcp_portnum;
char tcp_buffer[LEN_TCP_BUFFER];
struct sockaddr_in serv_addr, cli_addr;
socklen_t clilen;
uint16_t seqNum = 0;

char response_buffer[LEN_TCP_BUFFER];
int resp_len;

// For UDP socket handling
#define UDP_LOCAL_PORTNUM 10001
#define UDP_REMOTE_PORTNUM 10001
#define LEN_UDP_BUFFER 256
int udp_sockfd, udp_portnum, udp_remote_portnum;
char udp_buffer[LEN_UDP_BUFFER];
struct sockaddr_in udp_local_addr, udp_remote_addr;
socklen_t udp_addr_len;

// Packet rate
int pacUpdateRateIndex =  2;
int packetRates[9] = {1,5,10,50,100,500,1000,5000,10000};

// Timing control
struct timespec deadline;
#define ONE_BILLION 1000000000

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

void udpSendStandardPackage()
{
	seqNum++;

	bzero(udp_buffer, LEN_UDP_BUFFER);
	udp_buffer[0] = 0xAA;
	udp_buffer[1] = 0xAA;
	udp_buffer[2] = 0xAA;
	udp_buffer[3] = 0xAA;
	udp_buffer[4] = (uint8_t) seqNum;
	udp_buffer[5] = (uint8_t) (seqNum >> 8);
	udp_buffer[6] = curDigOuputs;
	udp_buffer[7] = 0;
	udp_buffer[8] = curDigInputs;
	udp_buffer[9] = 0;
	udp_buffer[10] = (uint8_t) curAnaInputs[0];
	udp_buffer[11] = (uint8_t) (curAnaInputs[0] >> 8);
	udp_buffer[12] = (uint8_t) curAnaInputs[1];
	udp_buffer[13] = (uint8_t) (curAnaInputs[1] >> 8);
	udp_buffer[14] = (uint8_t) curAnaInputs[2];
	udp_buffer[15] = (uint8_t) (curAnaInputs[2] >> 8);
	udp_buffer[16] = (uint8_t) curAnaInputs[3];
	udp_buffer[17] = (uint8_t) (curAnaInputs[3] >> 8);
	int thislen = 18;
    if (sendto(udp_sockfd, udp_buffer, thislen, 0, (struct sockaddr*) &udp_remote_addr, udp_addr_len) == -1)
    {
		 error("ERROR on UDP sendto");
    }
    /*
    printf("UDP sent to %d.%d.%d.%d: %d\r\n",
    		udp_remote_addr.sin_addr.s_addr % 256,
    		(udp_remote_addr.sin_addr.s_addr >> 8) % 256,
    		(udp_remote_addr.sin_addr.s_addr >> 16) % 256,
    		(udp_remote_addr.sin_addr.s_addr >> 24) % 256,
			ntohs(udp_remote_addr.sin_port)
			);
	*/
}

#define RESP_ACK '!'
#define RESP_NACK '?'
#define RESP_HELP "Commands:\r\n\
  ra    - Read Analog\r\n\
  rd    - Read Digital\r\n\
  wdx,y - Set Digital Output 'x' to value 'y'\r\n\
  pux   - Set Packet Update Rate to x/second\r\n\
  h     - Help\r\n\
  q     - Quit\r\n\
  k     - Kill\r\n\
  \r\n"
int commandHandler(char *tcp_buffer, int buflen, char *response_buffer, int *resp_len)
{
	char command[3];
	char digAddr;
	char digVal;

	// Assume bad response
	response_buffer[0] = RESP_NACK;
	response_buffer[1] = '\r';
	response_buffer[2] = '\n';
	*resp_len = 3;

	// Assume at least a one character command with \r\n
	if (buflen < 3) return 0; // Command not valid
	// If just one character, then set second as space
	if (buflen == 3) {
		tcp_buffer[1] = ' '; // So command[1] will include this below
	}

	// Get first two bytes as the command
	strncpy(command, tcp_buffer, 2);
	command[2] = '\0';

	// Read analog values
	if (strcmp(command, "ra") == 0) {
		scan_IO();
		bzero(response_buffer, LEN_TCP_BUFFER);
		sprintf(response_buffer, "%d, %d, %d, %d\r\n", curAnaInputs[0], curAnaInputs[1], curAnaInputs[2], curAnaInputs[3]);
		*resp_len = strlen(response_buffer);
	}
	// Read digital values
	else if (strcmp(command, "rd") == 0) {
		scan_IO();
		bzero(response_buffer, LEN_TCP_BUFFER);
		sprintf(response_buffer, "%d\r\n", curDigInputs);
		*resp_len = strlen(response_buffer);
	}
	// Write digital values
	else if (strcmp(command, "wd") == 0) {
		if (tcp_buffer[3] != ',') {
			return 0; // Bad command format
		}
		digAddr = tcp_buffer[2] - '0';
		digVal = tcp_buffer[4] - '0';
		if ((digAddr < 0) || (digAddr > 2)) {
			return 0; // Invalid number
		}
		if ((digVal < 0) || (digVal > 1)) {
			return 0; // Invalid number
		}
		writeDigOut(digAddr, digVal);
		response_buffer[0] = RESP_ACK;
	}
	// Packet update rate
	else if (strcmp(command, "pu") == 0) {
		digVal = tcp_buffer[2] - '0';
		if ((digVal < 1) || (digVal > 9)) {
			return 0; // Invalid number
		}
		pacUpdateRateIndex = digVal - 1;
		response_buffer[0] = RESP_ACK;
	}
	// Help
	else if (strcmp(command, "h ") == 0) {
		strcpy(response_buffer, RESP_HELP);
		*resp_len = sizeof (RESP_HELP);
	}
	// Quit session
	else if (strcmp(command, "q ") == 0) {
		*resp_len = 0;
	}
	// Kill process
	else if (strcmp(command, "k ") == 0) {
		*resp_len = 0;
	}
	else {
		return 0; // Invalid command
	}
	return 1; // Good response
}


int main()
{
    int buflen;
    int optval;
    int status;

	tcp_portnum = 10000;
	udp_portnum = UDP_LOCAL_PORTNUM;
	udp_remote_portnum = UDP_REMOTE_PORTNUM;

	if (!init_IO()) {
		return -1; // Error
	}

	// TCP socket
    init_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (init_sockfd < 0)
       error("ERROR opening TCP socket");

    // set to non-blocking
//    if (fcntl(init_sockfd, F_SETFL, O_NONBLOCK) < 0) {
//        error("ERROR setting TCP socket to NONBLOCK");
//	}

	// UDP socket
    udp_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (init_sockfd < 0)
       error("ERROR opening UDP socket");

    // TCP bind and listen
    //
	// Set SO_REUSEADDR option on the socket to true (1)
	// to avoid "Address already in use" error messages when restarting server
	//  after a recent crash or shutdown.
	optval = 1;
	setsockopt(init_sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(tcp_portnum);
	if (bind(init_sockfd, (struct sockaddr *) &serv_addr,
			 sizeof(serv_addr)) < 0)
			 error("ERROR on TCP binding");

	listen(init_sockfd,5);

    // UDP bind
    //
	bzero((char *) &udp_local_addr, sizeof(udp_local_addr));
	udp_local_addr.sin_family = AF_INET;
	udp_local_addr.sin_port = htons(udp_portnum); // Local portnum
	udp_local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Our IP

	if (bind(udp_sockfd, (struct sockaddr *) &udp_local_addr, sizeof(udp_local_addr)) < 0)
			 error("ERROR on UDP binding");

	// Start the timebase
	clock_gettime(CLOCK_MONOTONIC, &deadline);

	while (1) {
		printf("Waiting for accept()...\n");
		clilen = sizeof(cli_addr);
		tcp_sockfd = accept(init_sockfd,
					(struct sockaddr *) &cli_addr,
					&clilen);
		if (tcp_sockfd < 0)
			 error("ERROR on accept");

		// set to non-blocking
		if (fcntl(tcp_sockfd, F_SETFL, O_NONBLOCK) < 0) {
			error("ERROR setting TCP socket tcp_sockfd to NONBLOCK");
		}

		// Save address for UDP output later
		// We assume sizeof(cli_addr) == sizeof(udp_remote_addr)
		memcpy(&udp_remote_addr, &cli_addr, sizeof(udp_remote_addr)); // Remote IP
		udp_remote_addr.sin_port =  htons(udp_remote_portnum); // Remote portnum
		// By definition, udp_remote_addr.sin_family should be AF_INET
		udp_addr_len = sizeof(udp_remote_addr);

		while (1) {
			printf("Periodic processing and TCP input check...\n");
			do {
				deadline.tv_nsec += ONE_BILLION/packetRates[pacUpdateRateIndex]; // Set period
				if (deadline.tv_nsec >= ONE_BILLION)	{
					deadline.tv_nsec -= ONE_BILLION;
					deadline.tv_sec++;
				}
				clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &deadline, NULL);
				// Previoius method: MicroSleep(1000000L/packetRates[pacUpdateRateIndex]);

				scan_IO();
				udpSendStandardPackage();

				// Look for TCP input
				bzero(tcp_buffer,LEN_TCP_BUFFER);
				buflen = read(tcp_sockfd,tcp_buffer,LEN_TCP_BUFFER - 1);
			} while (buflen == -1);
			// Repeats until a successful read, allowing this non-blocking read
			// to retry. TBD: Check for other errors. Using error() yields
			//  the message "Resource temporarily unavailable".

			if (buflen < 0) {
				error("ERROR reading from socket"); // TBD: make this recoverable
			}
			status = commandHandler(tcp_buffer, buflen, response_buffer, &resp_len);
			if (status != 0) {
				// TBD
			}
			//printf("Message: %s\n",tcp_buffer);
			if (resp_len != 0) {
				buflen = write(tcp_sockfd, response_buffer, resp_len);
				if (buflen < 0) {
					error("ERROR writing to socket"); // TBD: make this recoverable
				}
			}

			// It would probably be better to have commandHandler "status" drive these two,
			// but here it is for now...
			// Look for quit command
			if ((strlen(tcp_buffer) == 3) && (tcp_buffer[0] == 'q')) {
				printf("Dropping connection. Bye.\r\n");
				buflen = write(tcp_sockfd,"Dropping connection. Bye.\r\n",27);
				sleep(1); // Give a little time for message to show on other end
				break;
			}
			// Look for kill command
			if ((strlen(tcp_buffer) == 3) && (tcp_buffer[0] == 'k')) {
				printf("Killing process. Bye.\r\n");
				buflen = write(tcp_sockfd,"Killing process. Bye.\r\n",27);
				sleep(2); // Give a little time for message to show on other end
				exit(0);
			}
		}
		close(tcp_sockfd);
	}
    close(init_sockfd);

    printf("Should never get here...\n");
    sleep(3);

	return 0;
}
