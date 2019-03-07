#ifndef __UDP_SENDER__H
#define __UDP_SENDER__H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "Darknet.hpp"



struct udp_packet_head {
	char magic[4];  // "dnet"
	uint8_t nothing_a;
	uint8_t cameraNumber;
	uint8_t nothing_b;
	uint8_t numDetections;
};

struct det_packet {
	uint32_t _class;  // waste of 3 bytes.... 
	float prob;
	float x;
	float y;
};


class UdpSender {
private:
	const char *_ServerAddress = "127.0.0.1";
	const uint16_t _DataRxPort = 3101;
	struct sockaddr_in data_server;
	int data_sockfd;

	char udpBuffer[1400];
	struct udp_packet_head *head;
	struct det_packet *det_packets;
	int maxCount = 1400 / sizeof(struct det_packet) - 1;

public:
	UdpSender();
	void sendData(int, detection**, int*);
};


#endif // __UDP_SENDER__H
