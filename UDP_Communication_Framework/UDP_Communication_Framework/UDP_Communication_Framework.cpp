// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//
#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"


// Added system libraries for reading 
#include <iostream>
#include <fstream>
#include <vector>
#include <string>



// TODO: Edit IP to send it to your friend and not to yourself
#define TARGET_IP "127.0.0.1"

#define BUFFERS_LEN 1024


// TODO: Uncomment the one you are currently building and comment the other
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


	// ----- SENDER -----
#ifdef SENDER

	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	// Opening the file for binary reading
	std::ifstream file(PICTURE_NAME, std::ios::in | std::ios::binary);

	if (!file) {
		std::cerr << "Could not open the file!" << std::endl;
		return 1;
	}

	// Getting the file size
	file.seekg(0, std::ios::end); 
	std::streamsize fileSize = file.tellg(); 
	file.seekg(0, std::ios::beg);



	// Creating all 3 header messages
	std::string fileNameSendMsg = "NAME=" + std::string(PICTURE_NAME);
	int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", fileNameSendMsg.c_str());
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	std::string fileSizeString = std::to_string(fileSize);
	std::string fileSizeSendMsg = "SIZE=" + fileSizeString + "\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", fileSizeSendMsg.c_str());
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	std::string startMsg = "START\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", startMsg.c_str());
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	int offset_num = 0;

	// Loop for sendinf picture data
	while (true) {
		file.read(buffer_tx + 8, BUFFERS_LEN - 8);
		int fileReadLen = file.gcount();
		if (fileReadLen == 0) {
			break;
		}
		uint32_t offset_bin = offset_num;
		memcpy(buffer_tx, "DATA", 4);
		memcpy(buffer_tx + 4, &offset_bin, 4);
		int sendingPacketLength = fileReadLen + 8;
		sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		Sleep(1);
		offset_num += fileReadLen;
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}

	// Sending ending message (many times to be sure it doesn't get lost)
	for (int i = 0; i < 3; ++i) {
		std::string endingMsg = "STOP\n";
		sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", endingMsg.c_str());
		sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}

	file.close();

	closesocket(socketS);

#endif // SENDER





	// ----- RECEIVER -----
#ifdef RECEIVER

	std::cout << "Waiting for data..\n";
	std::ofstream outputFile;
	bool isReceiving = true;
	char filename[256] = "received.bin";

	// Loop for receiving
	while (isReceiving) {
		int receivedLength = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);

		if (receivedLength == SOCKET_ERROR) {
			std::cout << "Socket error!\n";
			break;
		}
						// Searching for "NAME="
						if (strncmp(buffer_rx, "NAME=", 5) == 0) {
							int nameLength = receivedLength - 5;
							if (nameLength > 0 && nameLength < 256) {
								memcpy(filename, buffer_rx + 5, nameLength);
								filename[nameLength] = '\0';
								std::cout << "We have received a file with a name: " << filename << "\n";
							}
						}

						// Searching for "SIZE="
						else if (strncmp(buffer_rx, "SIZE=", 5) == 0) {
							std::cout << "File size info received: " << (buffer_rx + 5) << "\n";
						}

						// Searching for "START="
						else if (strncmp(buffer_rx, "START", 5) == 0) {
							std::cout << "We have received START and opening the file!\n";
							outputFile.open(filename, std::ios::binary);
							if (!outputFile.is_open()) {
								std::cout << "We could not create the file!\n";
							}
						}

		//Searching for "DATA"
		else if (strncmp(buffer_rx, "DATA", 4) == 0) {
			if (receivedLength >= 8) {
				//MOVE by 4 bytes for the first byte of the picture
				//first * - need to look at it as a 4byte number
				//second * - dereference
				uint32_t offset = *(uint32_t*)(buffer_rx + 4);

				int dataLength = receivedLength - 8;

				if (outputFile.is_open()) {
					outputFile.seekp(offset);
					outputFile.write(buffer_rx + 8, dataLength);

				}
			}
		}


		else if (strncmp(buffer_rx, "STOP", 4) == 0) {
			std::cout << "We have received STOP, that means exiting our connection!\n";

			if (outputFile.is_open()) {
				outputFile.close();
			}
			isReceiving = false;
		}

	}
	std::cout << "Transfer was a success!\n";

#endif




		//**********************************************************************

	getchar();
	return 0;
}


