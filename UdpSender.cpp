
#include <stdio.h>
#include <strings.h>  // for bzero()
#include <sstream>
#include<iostream>
#include "UdpSender.hpp"
using namespace std;

UdpSender::UdpSender() {
	bzero((char*)&data_server, sizeof(data_server));
	data_server.sin_family = AF_INET;
	data_server.sin_addr.s_addr = inet_addr(_ServerAddress);
	data_server.sin_port = htons(_DataRxPort);

	data_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (data_sockfd < 0) {
		fprintf(stderr, "Error opening socket");
		//return EXIT_FAILURE;
	}

	// Outgoing buffer-y-ness:
	strcpy(udpBuffer, "dnet");
	head = (struct udp_packet_head*) udpBuffer;
	det_packets = (struct det_packet*) &udpBuffer[8];

	printf("sizeof(struct udp_packet_head) %ld\n", sizeof(struct udp_packet_head));
	printf("sizeof(struct det_packet) %ld\n", sizeof(struct det_packet));
}


void UdpSender::sendData(int camNumber, detection *dets[], int nboxes[]) {

	int count = 0;

	for( int k=0; k < 8; ++k) {
		for (int i=0; i < nboxes[k]; ++i) {
		//for (int j=0; j<dn_meta.classes; ++j) {
			for (int j=0; j<2; ++j) {
		//int j = 0;  // class #0 is "person", the only category we care about
				if (dets[k][i].prob[j] > 0.10) {
					det_packets[count]._class = j;
					det_packets[count].prob = dets[k][i].prob[j];
					det_packets[count].x    = dets[k][i].bbox.x;
					det_packets[count].y    = dets[k][i].bbox.y;

					count++;

					if (count > maxCount) {
					goto breakOut;
					}
				}
			}
		}
	}
	breakOut:

	head->cameraNumber = camNumber;
	head->numDetections = count;

	sendto(data_sockfd, udpBuffer, count * sizeof(struct det_packet) + 8, 0,
		 (const struct sockaddr*)&data_server, sizeof(data_server));
}



