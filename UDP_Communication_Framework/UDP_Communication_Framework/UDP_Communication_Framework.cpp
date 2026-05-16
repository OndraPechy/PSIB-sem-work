#pragma comment(lib, "ws2_32.lib")

#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <cstring>
#include <cstdio>
#include "crc.h"
#include "sha256.h"
#include <stdexcept>
#include <cmath>

#define TARGET_IP "127.0.0.1"
#define BUFFERS_LEN 1024
#define HEADER_LENGTH 13
#define WAIT_TIMER 500
#define DATA_PAYLOAD_SIZE (BUFFERS_LEN - HEADER_LENGTH)
#define RECEIVE_WINDOW_SIZE 20

#define SENDER
//#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 14000
#define LOCAL_PORT 15001
#endif

#ifdef RECEIVER
#define LOCAL_PORT 15000
#define TARGET_PORT 14001
#endif

enum stateEnum {
	EMPTY,
	SEND,
	ACKNOWLEDGED,
};

struct packetData {
	std::string identifier;
	const char* data;
	int length;
	uint32_t offset;
};

struct senderInfo {
	SOCKET socketS;
	sockaddr_in addrDest;
	char alternatingBit;
};

struct windowSlot {
	char payload[DATA_PAYLOAD_SIZE];
	packetData pcktData;
	stateEnum state;
	DWORD time;
};

void createPacketWithoutCRC(char* buffer_tx, packetData sendData, char alternatingBit);
void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength);
void sendSelectiveRepeatPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256]);
void sendPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256]);
bool checkIdentifierSTOPSending(char* buffer_tx, int* currentCount);
bool checkReceivedAcknowledge(char* buffer_rx, char alternatingBit,
							  uint32_t(&table)[256], int packetLength);
bool checkBufferForCRC(char* buffer_rx, uint32_t(&table)[256], int packetLength);

void sendControlPacket(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256],
	const char* id, char alternatingBit);

void sendControlPacketForOffset(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256],
	const char* id, uint32_t offset, char alternatingBit);

uint32_t getPacketOffset(char* buffer);
int getDataPacketIndex(uint32_t offset);
bool isInsideReceiveWindow(int packetIndex, int receiveBasePacket, int windowSize);
void slideReceiveWindow(const std::vector<bool>& receivedDataPackets, int& receiveBasePacket);
bool allDataPacketsReceived(const std::vector<bool>& receivedDataPackets);
uint32_t getFirstMissingOffset(const std::vector<bool>& receivedDataPackets);

std::string calculateSHA256(const std::string& filePath);

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
	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;
	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar();
		return 1;
	}

	DWORD timeoutMs = WAIT_TIMER;
	setsockopt(socketS, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

	uint32_t table[256];
	crc32::generate_table(table);



#ifdef SENDER
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	senderInfo myContext;
	myContext.socketS = socketS;
	myContext.addrDest = addrDest;
	myContext.alternatingBit = 0;

	std::string filePath;
	std::cout << "Please provide a file name. In case it's in other directory than the executed program provide the file path!\n";
	std::cout << "FILE: ";
	std::getline(std::cin, filePath);
	std::cout << "*********************************************\n";


	size_t lastSlash = filePath.find_last_of("\\");
	std::string fileName = (lastSlash == std::string::npos) ? filePath : filePath.substr(lastSlash + 1);

	std::ifstream file(filePath, std::ios::in | std::ios::binary);
	if (!file) {
		std::cerr << "Could not open the file!\n" << std::endl;
		return 1;
	}

	file.seekg(0, std::ios::end); 
	std::streamsize fileSize = file.tellg(); 
	file.seekg(0, std::ios::beg);

	char alternatingBit = 0;

	std::cout << "The file NAME is: " << fileName << "\n";
	std::cout << "Sending the file NAME!\n";
	int nameStrigLength = fileName.length();
	packetData nameStruct = { "NAME", fileName.c_str(), nameStrigLength, 0 };
	sendPacket(nameStruct, myContext, table);
	std::cout << "*********************************************\n";

	std::string fileSizeString = std::to_string(fileSize);
	std::cout << "The file SIZE is: " << fileSizeString << "\n";
	std::cout << "Sending the file SIZE!\n";
	int sizeStringLength = fileSizeString.length();
	packetData sizeStruct = { "SIZE", fileSizeString.c_str(), sizeStringLength, 0 };
	sendPacket(sizeStruct, myContext, table);
	std::cout << "*********************************************\n";

	std::cout << "Sending the START signal!\n";
	packetData startStruct = { "STRT", nullptr, 0, 0 };
	sendPacket(startStruct, myContext, table);
	std::cout << "*********************************************\n";

	uint32_t offsetNum = 0;
	int packetNum = 1;
	windowSlot windowArray[RECEIVE_WINDOW_SIZE] = {};
	bool stillSendingData = true;
	bool stillInProccess = true;
	std::cout << "SENDING DATA PACKETS!\n";
	while (stillInProccess) {
		for (size_t windowIndex = 0; windowIndex < RECEIVE_WINDOW_SIZE; ++windowIndex) {
			if (!stillSendingData) {
				break;
			}
			if (windowArray[windowIndex].state == EMPTY) {
				char data[DATA_PAYLOAD_SIZE] = { 0 };
				file.read(data, DATA_PAYLOAD_SIZE);
				int fileReadLen = file.gcount();
				if (fileReadLen == 0) {
					stillSendingData = false;
					break;
				}
				memcpy(windowArray[windowIndex].payload, data, DATA_PAYLOAD_SIZE);
				windowArray[windowIndex].pcktData = { "DATA", windowArray[windowIndex].payload, fileReadLen, offsetNum };
				windowArray[windowIndex].time = GetTickCount();
				windowArray[windowIndex].state = SEND;
				uint32_t currentBasePacket = (windowArray[0].state != EMPTY) ?
					(windowArray[0].pcktData.offset / DATA_PAYLOAD_SIZE): (offsetNum / DATA_PAYLOAD_SIZE);
				uint32_t maxWindowPacket = currentBasePacket + (RECEIVE_WINDOW_SIZE - 1);
				std::cout << "-->[OUT]--> Sending PACKET " << packetNum << ", OFFSET = " << offsetNum
						  << ", Window (indexes): [" << currentBasePacket << " - " << maxWindowPacket << "]\n";
				sendSelectiveRepeatPacket(windowArray[windowIndex].pcktData, myContext, table);
				offsetNum += fileReadLen;
				packetNum++;
			}
		}

		struct sockaddr_in from;
		int fromlen = sizeof(from);
		char buffer_rx[BUFFERS_LEN];
		int recVal = recvfrom(myContext.socketS, buffer_rx, BUFFERS_LEN, 0, (sockaddr*)&from, &fromlen);
		if (recVal != -1 && checkBufferForCRC(buffer_rx, table, recVal)) {
			bool isACK = (memcmp(buffer_rx, "ACK ", 4) == 0);
			uint32_t offset = getPacketOffset(buffer_rx);
			uint32_t incomingPacketNum = (offset / DATA_PAYLOAD_SIZE) + 1;
			for (size_t windowIndex = 0; windowIndex < RECEIVE_WINDOW_SIZE; ++windowIndex) {
				if (windowArray[windowIndex].state != EMPTY && windowArray[windowIndex].pcktData.offset == offset) {
					if (isACK) {
						std::cout << "<--[IN]<-- ACK for PACKET " << incomingPacketNum << " received!\n";
						windowArray[windowIndex].state = ACKNOWLEDGED;
					}
					else {
						std::cout << "<--[IN]<-- !!!NAK for PACKET " << incomingPacketNum << " received!!! Resending!\n";
						windowArray[windowIndex].time = GetTickCount();
						sendSelectiveRepeatPacket(windowArray[windowIndex].pcktData, myContext, table);
					}
					
					break;
				}
			}
		}

		for (size_t windowIndex = 0; windowIndex < RECEIVE_WINDOW_SIZE; ++windowIndex) {
			if (windowArray[windowIndex].state != SEND) {
				continue;
			}
			if (GetTickCount() - windowArray[windowIndex].time >= WAIT_TIMER) {
				uint32_t timeoutPacketNum = (windowArray[windowIndex].pcktData.offset / DATA_PAYLOAD_SIZE) + 1;
				std::cout << " !!![ERROR]!!! TIMEOUT: PACKET " << timeoutPacketNum << " Resending!\n";
				windowArray[windowIndex].time = GetTickCount();
				sendSelectiveRepeatPacket(windowArray[windowIndex].pcktData, myContext, table);
			}
		}
		bool windowSlide = false;
		uint32_t oldBase = windowArray[0].pcktData.offset;
		uint32_t oldBasePacket = (oldBase / DATA_PAYLOAD_SIZE) + 1;
		while (windowArray[0].state == ACKNOWLEDGED) {
			windowSlide = true;
			for (size_t windowIndex = 0; windowIndex < RECEIVE_WINDOW_SIZE - 1; ++windowIndex) {
				windowArray[windowIndex] = windowArray[windowIndex + 1];
				windowArray[windowIndex].pcktData.data = windowArray[windowIndex].payload;
			}
			windowArray[RECEIVE_WINDOW_SIZE - 1] = windowSlot{};
			windowArray[RECEIVE_WINDOW_SIZE - 1].state = EMPTY;
		}
		if (windowSlide) {
			uint32_t currentBase = (windowArray[0].state != EMPTY) ? windowArray[0].pcktData.offset : offsetNum;
			uint32_t currentBasePacket = (currentBase / DATA_PAYLOAD_SIZE) + 1;
			std::cout << "***[INFO]*** WINDOW SLIDE: Packets [" << oldBasePacket << " -> " << currentBasePacket << "]\n";
		}
		if (!stillSendingData) {
			stillInProccess = false;
			for (size_t windowIndex = 0; windowIndex < RECEIVE_WINDOW_SIZE; ++windowIndex) {
				if (windowArray[windowIndex].state != EMPTY) {
					stillInProccess = true;
					break;
				}
			}
		}
	}

	std::cout << "ALL DATA PACKETS WERE SUCCESSFULLY SENT!\n";
	std::cout << "*********************************************\n";

	std::string fileHash = calculateSHA256(filePath);
	std::cout << "SHA-256 of original file: " << fileHash << "\n";
	std::cout << "I'm sending the HASH!\n";
	packetData hashStruct = {"HASH", fileHash.c_str(), static_cast<int>(fileHash.length()), 0};
	sendPacket(hashStruct, myContext, table);
	std::cout << "*********************************************\n";

	std::cout << "I'm sending the STOP signal!\n";
	packetData stopStruct = { "STOP", nullptr, 0, 0 };
	sendPacket(stopStruct, myContext, table);
	std::cout << "*********************************************\n";

	std::cout << "FILE WAS SUCCESSFULLY SENT :)!\n";

	file.close();
	closesocket(socketS);
#endif





#ifdef RECEIVER
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	struct sockaddr_in from;
	int fromlen = sizeof(from);
	char buffer_rx[BUFFERS_LEN];

	std::ofstream outputFile;
	bool isReceiving = true;

	std::string originalFileName;
	std::string outputFileName = "received.bin";
	std::string expectedHash;

	long long expectedFileSize = -1;
	long long receivedBytes = 0;

	int receivedPackets = 0;
	int totalDataPackets = 0;
	int receiveBasePacket = 0;

	std::vector<bool> receivedDataPackets;
	std::vector<int> receivedDataLengths;

	std::cout << "*********************************************\n";
	std::cout << "Selective Repeat receiver window size: " << RECEIVE_WINDOW_SIZE << "\n";
	std::cout << "DATA payload size: " << DATA_PAYLOAD_SIZE << " bytes\n";
	std::cout << "Waiting for data!\n";
	std::cout << "*********************************************\n";

	while (isReceiving) {
		memset(buffer_rx, 0, sizeof(buffer_rx));
		fromlen = sizeof(from);

		int receivedLength = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);
		if (receivedLength == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if (error == WSAETIMEDOUT) {
				continue;
			}
			std::cout << "Socket error: " << error << "\n";
			break;
		}
		std::cout << "------------------------------\n";
		if (receivedLength < HEADER_LENGTH) {
			std::cout << "Received packet shorter than HEADER_LENGTH.\n";
			continue;
		}
		char packetBit = buffer_rx[12];
		char packetId[5] = { 0 };
		memcpy(packetId, buffer_rx, 4);
		uint32_t packetOffset = getPacketOffset(buffer_rx);
		std::cout << "Packet '" << packetId << "' received. LENGTH = " << receivedLength
				  << " bytes, offset = " << packetOffset << "\n";
		bool crcOk = checkBufferForCRC(buffer_rx, table, receivedLength);
		if (!crcOk) {
			std::cout << "CRC ERROR in packet " << packetId
					  << " -> sending NAK for offset " << packetOffset << "\n";
			sendControlPacketForOffset(socketS, addrDest, table, "NAK ", packetOffset, packetBit);
			continue;
		}

		int payloadLength = receivedLength - HEADER_LENGTH;
		char* payload = buffer_rx + HEADER_LENGTH;
		if (memcmp(buffer_rx, "NAME", 4) == 0) {
			if (outputFile.is_open() || receivedPackets > 0) {
				std::cout << "Late or duplicate NAME packet -> sending ACK again.\n";
				sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
				continue;
			}
			originalFileName.assign(payload, payloadLength);
			if (originalFileName.empty()) {
				std::cout << "File name NOT received, output will be: received.bin\n";
				outputFileName = "received.bin";
			}
			else {
				std::cout << "File name received: " << originalFileName << "\n";
				outputFileName = originalFileName;
			}
			std::cout << "NAME processed -> sending ACK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
			continue;
		}

		else if (memcmp(buffer_rx, "SIZE", 4) == 0) {
			if (outputFile.is_open() || receivedPackets > 0) {
				std::cout << "Late or duplicate SIZE packet -> sending ACK again.\n";
				sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
				continue;
			}
			std::string sizeString(payload, payloadLength);
			try {
				expectedFileSize = std::stoll(sizeString);
				totalDataPackets = static_cast<int>(
					(expectedFileSize + DATA_PAYLOAD_SIZE - 1) / DATA_PAYLOAD_SIZE
					);
				receivedDataPackets.assign(totalDataPackets, false);
				receivedDataLengths.assign(totalDataPackets, 0);
				receivedBytes = 0;
				receivedPackets = 0;
				receiveBasePacket = 0;
				std::cout << "File size received: " << expectedFileSize << " bytes\n";
				std::cout << "Expected DATA packets: " << totalDataPackets << "\n";
			}
			catch (...) {
				std::cout << "Could not parse file size -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", packetOffset, packetBit);
				continue;
			}
			std::cout << "SIZE processed -> sending ACK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
			continue;
		}

		else if (memcmp(buffer_rx, "STRT", 4) == 0) {
			if (outputFile.is_open() || receivedPackets > 0) {
				std::cout << "Duplicate START packet -> sending ACK again.\n";
				sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
				continue;
			}
			std::cout << "START received, opening output file...\n";
			outputFile.open(outputFileName, std::ios::binary);
			if (!outputFile.is_open()) {
				std::cout << "Could not create output file -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", packetOffset, packetBit);
				continue;
			}
			std::cout << "Output file opened successfully.\n";
			std::cout << "STRT processed -> sending ACK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
			continue;
		}

		else if (memcmp(buffer_rx, "DATA", 4) == 0) {
			uint32_t offset = packetOffset;
			int packetIndex = getDataPacketIndex(offset);
			std::cout << "DATA packet index: " << packetIndex << ", RECEIVE WINDOW: [" << receiveBasePacket
				<< " - " << (receiveBasePacket + RECEIVE_WINDOW_SIZE - 1) << "]\n";
			if (!outputFile.is_open()) {
				std::cout << "Output file is not open, DATA packet ignored -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", offset, packetBit);
				continue;
			}
			if (totalDataPackets <= 0 || packetIndex < 0 || packetIndex >= totalDataPackets) {
				std::cout << "DATA packet index outside expected file range -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", offset, packetBit);
				continue;
			}
			if (packetIndex < receiveBasePacket) {
				std::cout << "Sending ACK.\n";
				sendControlPacketForOffset(socketS, addrDest, table, "ACK ", offset, packetBit);
				continue;
			}
			if (!isInsideReceiveWindow(packetIndex, receiveBasePacket, RECEIVE_WINDOW_SIZE)) {
				std::cout << "DATA packet outside receive window -> ignored. Sender should resend after timeout.\n";
				continue;
			}
			if (receivedDataPackets[packetIndex]) {
				std::cout << "Duplicate DATA packet inside window -> sending ACK again for offset " << offset << "\n";
				sendControlPacketForOffset(socketS, addrDest, table, "ACK ", offset, packetBit);
				continue;
			}
			outputFile.seekp(offset, std::ios::beg);
			outputFile.write(payload, payloadLength);
			if (!outputFile.good()) {
				std::cout << "Error while writing DATA packet to file -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", offset, packetBit);
				continue;
			}
			receivedDataPackets[packetIndex] = true;
			receivedDataLengths[packetIndex] = payloadLength;
			receivedBytes += payloadLength;
			receivedPackets++;
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", offset, packetBit);
			int oldReceiveBasePacket = receiveBasePacket;
			slideReceiveWindow(receivedDataPackets, receiveBasePacket);
			if (receiveBasePacket == oldReceiveBasePacket && packetIndex > receiveBasePacket) {
				uint32_t missingOffset = static_cast<uint32_t>(receiveBasePacket * DATA_PAYLOAD_SIZE);
				std::cout << "GAP DETECTED. First missing packet index: " << receiveBasePacket << ", missing offset: "
						  << missingOffset << " -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", missingOffset, packetBit);
				continue;
			}
			std::cout << "Sending ACK.\n";
			continue;
		}
		else if (memcmp(buffer_rx, "HASH", 4) == 0) {
			expectedHash.assign(payload, payloadLength);
			std::cout << "Expected SHA-256 received: " << expectedHash << "\n";
			std::cout << "HASH processed -> sending ACK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
			continue;
		}
		else if (memcmp(buffer_rx, "STOP", 4) == 0) {
			if (!receivedDataPackets.empty() && !allDataPacketsReceived(receivedDataPackets)) {
				uint32_t missingOffset = getFirstMissingOffset(receivedDataPackets);
				std::cout << "STOP received. Some DATA packets still missing. First missing offset: "
						  << missingOffset << " -> sending NAK\n";
				sendControlPacketForOffset(socketS, addrDest, table, "NAK ", missingOffset, packetBit);
				continue;
			}
			std::cout << "STOP received. All DATA packets present, closing output file!\n";
			if (outputFile.is_open()) {
				outputFile.close();
			}
			if (!expectedHash.empty()) {
				try {
					std::string receivedHash = calculateSHA256(outputFileName);
					std::cout << "*********************************************\n";
					std::cout << "SHA-256 of received file: " << receivedHash << "\n";
					if (receivedHash == expectedHash) {
						std::cout << "FILE HASH OK: received file is correct.\n";
					}
					else {
						std::cout << "FILE HASH ERROR: received file is corrupted!\n";
						std::cout << "Expected: " << expectedHash << "\n";
						std::cout << "Received: " << receivedHash << "\n";
					}
				}
				catch (const std::exception& e) {
					std::cout << "Could not calculate SHA-256: " << e.what() << "\n";
				}
			}
			else {
				std::cout << "WARNING: HASH packet was not received.\n";
			}
			if (expectedFileSize >= 0) {
				try {
					std::cout << "*********************************************\n";
					long long receivedFileSize = std::filesystem::file_size(outputFileName);
					std::cout << "Expected file size: " << expectedFileSize << " bytes\n";
					std::cout << "Received file size: " << receivedFileSize << " bytes\n";
					if (receivedFileSize == expectedFileSize) {
						std::cout << "FILE SIZE OK.\n";
					}
					else {
						std::cout << "FILE SIZE ERROR.\n";
					}
				}
				catch (...) {
					std::cout << "Could not check received file size.\n";
				}
			}
			std::cout << "STOP processed -> sending ACK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "ACK ", packetOffset, packetBit);
			isReceiving = false;
			continue;
		}
		else {
			std::cout << "Unknown packet type: " << packetId << " -> sending NAK\n";
			sendControlPacketForOffset(socketS, addrDest, table, "NAK ", packetOffset, packetBit);
			continue;
		}
	}
	std::cout << "*********************************************\n";
	std::cout << "RECEIVING FINISHED!.\n";
	closesocket(socketS);
#endif
	getchar();
	return 0;
}



void createPacketWithoutCRC(char* buffer_tx, packetData sendData, char alternatingBit) 
{
	memset(buffer_tx, 0, BUFFERS_LEN);
	memcpy(buffer_tx, sendData.identifier.c_str(), 4);
	memcpy(buffer_tx + 4, &sendData.offset, 4);
	memset(buffer_tx + 8, 0, 4);
	memcpy(buffer_tx + 12, &alternatingBit, 1);
	if (sendData.length > 0) {
		memcpy(buffer_tx + 13, sendData.data, sendData.length);
	}
}


void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength) 
{
	uint32_t crc = crc32::update(table, 0, buffer_tx, packetLength);
	memcpy(buffer_tx + 8, &crc, 4);
}

void sendSelectiveRepeatPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256])
{
	char buffer_tx[BUFFERS_LEN];
	char buffer_rx[BUFFERS_LEN];
	int packetLength = sendData.length + HEADER_LENGTH;
	createPacketWithoutCRC(buffer_tx, sendData, info.alternatingBit);
	addCRCToPacket(buffer_tx, table, packetLength);
	sendto(info.socketS, buffer_tx, packetLength, 0, (sockaddr*)&info.addrDest, sizeof(info.addrDest));
}


void sendPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256]) 
{
	char buffer_tx[BUFFERS_LEN];
	char buffer_rx[BUFFERS_LEN];
	int packetLength = sendData.length + HEADER_LENGTH;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	createPacketWithoutCRC(buffer_tx, sendData, info.alternatingBit);
	addCRCToPacket(buffer_tx, table, packetLength);
	bool correctMessageReceived = false;
	int identifierSTOPCounter = 0;

	while (!correctMessageReceived) {
		sendto(info.socketS, buffer_tx, packetLength, 0, (sockaddr*)&info.addrDest, sizeof(info.addrDest));
		int recVal = recvfrom(info.socketS, buffer_rx, BUFFERS_LEN, 0, (sockaddr*)&from, &fromlen);
		if (recVal == -1) {
			std::cout << "ERROR_TIMEOUT: Haven't received a response in time!\n";
			if (checkIdentifierSTOPSending(buffer_tx, &identifierSTOPCounter)) {
				std::cout << "STOPPED RECEIVING ACKNOWLEDGEMENTS!\n";
				break;
			}
			std::cout << "Resending the packet!\n";
			continue;
		}
		if (!checkReceivedAcknowledge(buffer_rx, info.alternatingBit, table, recVal)) {
			std::cout << "ERROR_ACK: Received acknowldegement does NOT correspond with the expected!\n";
			std::cout << "Resending the packet!\n";
			continue;
		}
		std::cout << "ACK '"  << sendData.identifier << "' received" << "\n";
		correctMessageReceived = true;
	}
}


bool checkReceivedAcknowledge(char* buffer_rx, char alternatingBit,
	uint32_t(&table)[256], int packetLength) 
{
	if (packetLength < HEADER_LENGTH) {
		return false;
	}
	if (memcmp(buffer_rx, "ACK ", 4) != 0) {
		return false;
	}
	else if (buffer_rx[12] != alternatingBit) {
		return false;
	}
	else if (!checkBufferForCRC(buffer_rx, table, packetLength)) {
		return false;
	}
	else {
		return true;
	}
}


bool checkIdentifierSTOPSending(char* buffer_tx, int *currentCount)
{
	int countToStop = 10;
	if (memcmp(buffer_tx, "STOP", 4) != 0) {
		return false;
	}
	else if (*currentCount < countToStop) {
		(*currentCount)++;
		return false;
	}
	else {
		return true;
	}
}


bool checkBufferForCRC(char* buffer_rx, uint32_t(&table)[256], int packetLength)
{
	char originalCRC[4];
	memcpy(originalCRC, buffer_rx + 8, 4);
	memset(buffer_rx + 8, 0, 4);
	uint32_t generatedCRC = crc32::update(table, 0, buffer_rx, packetLength);
	uint32_t receivedCRC;
	memcpy(&receivedCRC, originalCRC, sizeof(uint32_t));
	memcpy(buffer_rx + 8, originalCRC, 4);
	return receivedCRC == generatedCRC;
}


void sendControlPacket(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256],
					   const char* id, char alternatingBit)
{
	char buffer_tx[BUFFERS_LEN];
	packetData controlPacket;
	controlPacket.identifier = std::string(id, 4);
	controlPacket.data = nullptr;
	controlPacket.length = 0;
	controlPacket.offset = 0;
	createPacketWithoutCRC(buffer_tx, controlPacket, alternatingBit);
	addCRCToPacket(buffer_tx, table, HEADER_LENGTH);
	sendto(socketS, buffer_tx, HEADER_LENGTH, 0, (sockaddr*)&addrDest, sizeof(addrDest));
}
 

std::string calculateSHA256(const std::string& filePath)
{
	std::ifstream file(filePath, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("Could not open file for SHA-256 calculation.");
	}
	SHA256 sha256;
	const size_t bufferSize = 4096;
	char buffer[bufferSize];

	while (file.good()) {
		file.read(buffer, bufferSize);
		std::streamsize bytesRead = file.gcount();
		if (bytesRead > 0) {
			sha256.add(buffer, static_cast<size_t>(bytesRead));
		}
	}

	return sha256.getHash();
}

void sendControlPacketForOffset(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256],
								const char* id, uint32_t offset,char alternatingBit)
{
	char buffer_tx[BUFFERS_LEN];
	packetData controlPacket;
	controlPacket.identifier = std::string(id, 4);
	controlPacket.data = nullptr;
	controlPacket.length = 0;
	controlPacket.offset = offset;
	createPacketWithoutCRC(buffer_tx, controlPacket, alternatingBit);
	addCRCToPacket(buffer_tx, table, HEADER_LENGTH);
	sendto(socketS, buffer_tx, HEADER_LENGTH, 0, (sockaddr*)&addrDest,sizeof(addrDest));
}


uint32_t getPacketOffset(char* buffer)
{
	uint32_t offset = 0;
	memcpy(&offset, buffer + 4, sizeof(uint32_t));
	return offset;
}


int getDataPacketIndex(uint32_t offset)
{
	return static_cast<int>(offset / DATA_PAYLOAD_SIZE);
}


bool isInsideReceiveWindow(int packetIndex, int receiveBasePacket, int windowSize)
{
	return packetIndex >= receiveBasePacket
		&& packetIndex < receiveBasePacket + windowSize;
}


void slideReceiveWindow(const std::vector<bool>& receivedDataPackets, int& receiveBasePacket)
{
	while (receiveBasePacket < static_cast<int>(receivedDataPackets.size()) && 
		   receivedDataPackets[receiveBasePacket]) {
		receiveBasePacket++;
	}
}


bool allDataPacketsReceived(const std::vector<bool>& receivedDataPackets)
{
	for (bool received : receivedDataPackets) {
		if (!received) {
			return false;
		}
	}
	return true;
}


uint32_t getFirstMissingOffset(const std::vector<bool>& receivedDataPackets)
{
	for (int i = 0; i < static_cast<int>(receivedDataPackets.size()); i++) {
		if (!receivedDataPackets[i]) {
			return static_cast<uint32_t>(i * DATA_PAYLOAD_SIZE);
		}
	}
	return 0;
}

