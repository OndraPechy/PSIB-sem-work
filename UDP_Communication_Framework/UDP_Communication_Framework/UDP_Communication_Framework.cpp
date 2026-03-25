// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//

#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#define TARGET_IP	"127.0.0.1"

#define BUFFERS_LEN 1024

#define SENDER
//#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5111
#define LOCAL_PORT 5222
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 5222
#define LOCAL_PORT 5111
#endif // RECEIVER


void InitWinsock()
{
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
}

//**********************************************************************
int main()
{
	SOCKET socketS;

	InitWinsock();

	struct sockaddr_in local;
	struct sockaddr_in from;

	int fromlen = sizeof(from);
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;


	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar(); //wait for press Enter
		return 1;
	}
	//**********************************************************************
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];

#ifdef SENDER

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);


	int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "Hello world payload!\n"); //put some data to buffer
	printf("Sending packet.\n");
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));

	closesocket(socketS);

#endif // SENDER

#ifdef RECEIVER

	printf("Waiting for datagram ...\n");

	int receivedPacketLength = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);

	if (receivedPacketLength == SOCKET_ERROR) {
		printf("Socket error!\n");
		getchar();
		return 1;
	}
	else
	{
		printf("Bytes received: %d\n", receivedPacketLength);
		buffer_rx[receivedPacketLength] = 0x00;
		printf("Datagram: %s", buffer_rx);
	}

	closesocket(socketS);
#endif
	//**********************************************************************

	getchar(); //wait for press Enter
	return 0;
}
