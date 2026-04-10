// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//


// TODO: add system libraries for reading 
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

// TODO: edit IP to send it to your friend and not to yourself
#define TARGET_IP	"127.0.0.1"

#define BUFFERS_LEN 1024


// TODO: uncomment the one you are currently building and comment the other
#define SENDER
//#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5111
#define LOCAL_PORT 5222
#define PICTURE_NAME "obrazek.png"
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


// ----- TODO: ONDRA - MAKE A SENDER -----
#ifdef SENDER

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);


	// int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "Hello world payload!\n"); //put some data to buffer
	// printf("Sending packet.\n");
	// sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));

	//closesocket(socketS);

	// std znamena standartni knihovna, dvojtecka znamena, ze se clovek zanori do dane knihovny a jde v ni neco hledat
	// takto otevru soubor pro binarni cteni v c++
	std::ifstream file(PICTURE_NAME, std::ios::in | std::ios::binary);

	// ify a loopy pisu v c++ uplne stejne jako v c
	if (!file) {
		// cerr je znakovy chybovy vystup, neco jako stderr v c, << jsou vkladaci operatory (vlozi na chybovy vystup), endl znamena endline
		std::cerr << "Could not open the file!" << std::endl;
		return 1;
	}

	// zjistim velikost souboru, seekg presune kurzor v souboru na konkretni misto
	file.seekg(0, std::ios::end); // presunu na konec
	// std::streamsize je datovy typ na uchovani velikosti souboru
	std::streamsize fileSize = file.tellg(); // tellg vrati cislo pozice kde aktualne je
	file.seekg(0, std::ios::beg); // presunu zpet na zacatek


	
	// vytvorim si jednotlive casti hlavicky
	std::string fileNameSendMsg = "NAME=" + std::string(PICTURE_NAME) + "\n";
	std::string fileSizeString = std::to_string(fileSize);
	std::string fileSizeSendMsg = "SIZE=" + fileSizeString + "\n";
	// vytvorim si celkovou zpravu hlavicky
	std::string headerMsg = fileNameSendMsg + fileSizeSendMsg + "START\n";

	// vlozim si headerMsg do bufferu
	int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", headerMsg.c_str()); //put some data to buffer
	// a odeslu hlavicku
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	// procistim buffer
	memset(buffer_tx, 0, sizeof(buffer_tx));

	// v hlavnim loopu budu take posilat to celkove
	int offset_num = 0;
	// dam nekonecny loop
	while (true) {
		
		// nactu na indexy 4-1023 v bufferu 
		file.read(buffer_tx + 4, BUFFERS_LEN - 4);
		// zjistim, kolik jsem nacetl
		int fileReadLen = file.gcount();
		// pokud jsem nacetl 0, asi jsem u konce a koncim
		if (fileReadLen == 0) {
			break;
		}
		// nactu offset do buffer_tx na prvni 4 pozice
		uint32_t offset_bin = offset_num;
		memcpy(buffer_tx, &offset_bin, 4);
		// zjistim delku packetu
		int sendingPacketLength = fileReadLen + 4;
		// poslu packet
		sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		// prictu offset
		offset_num += fileReadLen;
		// procistim buffer
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}
	
	// na zaver poslu end zpravu
	std::string endingMsg = "STOP\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", endingMsg.c_str());
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));

	closesocket(socketS);




#endif // SENDER
// ----- END OF SENDER EDITING -----





// ----- TODO: VOJTA - MAKE A RECEIVER -----
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
// ----- END OF RECEIVER EDITING -----





	//**********************************************************************

	getchar(); //wait for press Enter
	return 0;
}
