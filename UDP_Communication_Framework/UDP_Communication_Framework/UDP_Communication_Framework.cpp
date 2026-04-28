#pragma comment(lib, "ws2_32.lib")

#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

#define TARGET_IP "10.1.3.239"

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
		getchar();
		return 1;
	}
	char buffer_rx[BUFFERS_LEN];
	char buffer_tx[BUFFERS_LEN];

#ifdef SENDER
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	std::cout << "WELCOME TO THE FILE SENDING PROGRAM!\n\n";
	std::string filePath;
	std::cout << "Please provide a file name. In case it's in other directory than the executed program provide the file path!\n";
	std::cout << "FILE: ";
	std::getline(std::cin, filePath);
	std::cout << "*********************************************\n";

	// najdu posledni lomitko
	size_t lastSlash = filePath.find_last_of("\\");
	// kdyz tam neni lomitko name je filepath, jinak to je o 1 znak dal nez lomitko
	std::string fileName = (lastSlash == std::string::npos) ? filePath : filePath.substr(lastSlash + 1);

	std::ifstream file(filePath, std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << "Could not open the file!\n" << std::endl;
		return 1;
	}

	file.seekg(0, std::ios::end); 
	std::streamsize fileSize = file.tellg(); 
	file.seekg(0, std::ios::beg);

	std::string fileNameSendMsg = "NAME=" + fileName;
	int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", fileNameSendMsg.c_str());
	// takto v c++ spojuju stringy
	std::cout << "The file NAME is: " << fileName << "\n";
	std::cout << "I'm sending the file NAME!\n";
	std::cout << "*********************************************\n";
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	std::string fileSizeString = std::to_string(fileSize);
	std::string fileSizeSendMsg = "SIZE=" + fileSizeString + "\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", fileSizeSendMsg.c_str());
	// takto v c++ spojuju stringy
	std::cout << "The file SIZE is: " << fileSizeString << "\n";
	std::cout << "I'm sending the file SIZE!\n";
	std::cout << "*********************************************\n";
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	std::string startMsg = "START\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", startMsg.c_str());
	std::cout << "I'm sending the START signal!\n";
	std::cout << "*********************************************\n";
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	Sleep(10);

	int offset_num = 0;
	int packetNum = 1;

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
		std::cout << "Sent packet number: " << packetNum << "\n";
		Sleep(1);
		offset_num += fileReadLen;
		packetNum++;
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}
	std::cout << "All packets were succesfully send!\n";
	std::cout << "*********************************************\n";

	
	std::string endingMsg = "STOP\n";
	sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", endingMsg.c_str());
	std::cout << "I'm sending the STOP signal!\n";
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	memset(buffer_tx, 0, sizeof(buffer_tx));
	

	file.close();

	closesocket(socketS);

#endif


#ifdef RECEIVER

	std::cout << "Waiting for data..\n";
	std::ofstream outputFile;
	bool isReceiving = true;
	char filename[256] = "received.bin";

	int receivedPackets = 0; 

	// Loop for receiving
	while (isReceiving) {
		int receivedLength = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		if (receivedLength == SOCKET_ERROR) {
			std::cout << "Socket error!\n";
			break;
		}
						if (strncmp(buffer_rx, "NAME=", 5) == 0) {
							int nameLength = receivedLength - 5;
							if (nameLength > 0 && nameLength < 256) {
								memcpy(filename, buffer_rx + 5, nameLength);
								filename[nameLength] = '\0';
								std::cout << "We have received a file with a name: " << filename << "\n";
							}
						}
						else if (strncmp(buffer_rx, "SIZE=", 5) == 0) {
							std::cout << "File size info received: " << (buffer_rx + 5) << "\n";
						}
						else if (strncmp(buffer_rx, "START", 5) == 0) {
							std::cout << "We have received START and opening the file!\n";
							outputFile.open(filename, std::ios::binary);
							if (!outputFile.is_open()) {
								std::cout << "We could not create the file!\n";
							}
							receivedPackets = 0; 
						}
		else if (strncmp(buffer_rx, "DATA", 4) == 0) {
			if (receivedLength >= 8) {
				uint32_t offset = *(uint32_t*)(buffer_rx + 4);
				int dataLength = receivedLength - 8;
				if (outputFile.is_open()) {
					outputFile.seekp(offset);
					outputFile.write(buffer_rx + 8, dataLength);

					receivedPackets++; 

					std::cout << "\rReceived pacet number: " << receivedPackets;

				}
			}
		}
		else if (strncmp(buffer_rx, "STOP", 4) == 0) {
			std::cout << "\nWe have received STOP, that means exiting our connection!\n";

			if (outputFile.is_open()) {
				outputFile.close();
			}
			isReceiving = false;
		}
	}
	std::cout << "Transfer was a success!\n";

#endif
	getchar();
	return 0;
}


