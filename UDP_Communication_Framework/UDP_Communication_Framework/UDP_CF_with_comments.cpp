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
#define TARGET_IP "10.1.1.197"

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
	std::string fileNameSendMsg = "NAME=" + std::string(PICTURE_NAME);
	// vlozim si headerMsg do bufferu
	int sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", fileNameSendMsg.c_str());
	// a odeslu hlavicku
	sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
	// procistim buffer
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

	// v hlavnim loopu budu take posilat to celkove
	int offset_num = 0;

	// dam nekonecny loop
	while (true) {

		// nactu na indexy 4-1023 v bufferu 
		file.read(buffer_tx + 8, BUFFERS_LEN - 8);
		// zjistim, kolik jsem nacetl
		int fileReadLen = file.gcount();
		// pokud jsem nacetl 0, asi jsem u konce a koncim
		if (fileReadLen == 0) {
			break;
		}
		// nactu offset do buffer_tx na prvni 4 pozice
		uint32_t offset_bin = offset_num;
		memcpy(buffer_tx, "DATA", 4);
		memcpy(buffer_tx + 4, &offset_bin, 4);
		// zjistim delku packetu
		int sendingPacketLength = fileReadLen + 8;
		// poslu packet
		sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		Sleep(1);
		// prictu offset
		offset_num += fileReadLen;
		// procistim buffer
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}

	// na zaver poslu end zpravu
	for (int i = 0; i < 50; ++i) {
		std::string endingMsg = "STOP\n";
		sendingPacketLength = snprintf(buffer_tx, sizeof(buffer_tx), "%s", endingMsg.c_str());
		sendto(socketS, buffer_tx, sendingPacketLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		memset(buffer_tx, 0, sizeof(buffer_tx));
	}

	file.close();

	closesocket(socketS);

#endif // SENDER
	// ----- END OF SENDER EDITING -----





	// ----- TODO: VOJTA - MAKE A RECEIVER -----
#ifdef RECEIVER

	/* Should be deleted by gemini
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
	*/

	std::cout << "Waiting for data..\n";

	//creates a "roura" for writing into a file
	std::ofstream outputFile;
	//bool for while if we are still receiving
	bool isReceiving = true;
	//array for the name of the file, it is a safety thing if we lost the paket
	char filename[256] = "received.bin";

	while (isReceiving) {
		//how does recvfrom work - takes data from the network and stores it into the buffer_rx
		//finds out the IP address, 
		// (sockaddr*)&from = overwrite for sockaddr
		int receivedLength = recvfrom(socketS, buffer_rx, sizeof(buffer_rx), 0, (sockaddr*)&from, &fromlen);

		if (receivedLength == SOCKET_ERROR) {
			std::cout << "Socket error!\n";
			break;
		}



		// ---------------------- I CHANGED THIS PART SO THAT IT FITS THE SENDER DESIGN ----------------------
		
						//searching for "NAME="
						if (strncmp(buffer_rx, "NAME=", 5) == 0) {
							//Length of the name without the "NAME"
							int nameLength = receivedLength - 5;

							//check if the name can fit into the array
							if (nameLength > 0 && nameLength < 256) {
								//we copy the name from the buffer
								memcpy(filename, buffer_rx + 5, nameLength);
								//ending sequence for the name
								filename[nameLength] = '\0';

								std::cout << "We have received a file with a name: " << filename << "\n";
							}
						}

						else if (strncmp(buffer_rx, "SIZE=", 5) == 0) {
							std::cout << "File size info received: " << (buffer_rx + 5) << "\n";
						}


						else if (strncmp(buffer_rx, "START", 5) == 0) {
							std::cout << "We have received START and opening the file!\n";
							//opening the file in binary
							outputFile.open(filename, std::ios::binary);
							if (!outputFile.is_open()) {
								std::cout << "We could not create the file!\n";
							}
						}
		
		// ---------------------- END OF PREVIOUS VOJTA'S CODE ----------------------
		/*
		if (strncmp(buffer_rx, "NAME=", 5) == 0) {
			char* newline_pos = strchr(buffer_rx, '\n');
			if (newline_pos != nullptr) {
				//Length of the name without the "NAME"
				int nameLength = newline_pos - (buffer_rx + 5);

				//check if the name can fit into the array
				if (nameLength > 0 && nameLength < 256) {
					//we copy the name from the buffer
					memcpy(filename, buffer_rx + 5, nameLength);
					//ending sequence for the name
					filename[nameLength] = '\0';

					std::cout << "We have received a file with a name: " << filename << "\n";
				}
			}

			std::cout << "Opening the file!\n";
			//opening the file in binary 
			outputFile.open(filename, std::ios::binary);
			if (!outputFile.is_open()) {
				std::cout << "We could not create the file!\n";
			}
		}
		*/
		// ---------------------- END OF MY CHANGE ----------------------





		else if (strncmp(buffer_rx, "DATA", 4) == 0) {
			if (receivedLength >= 8) {

				//quite complicated, we have received a message with 
				//[D][A][T][A][byte_of_the_number][byte_of_the_number] .. 2 more times
				// then we have the [first_byte_of_the_picture] etc...
				// and we need to get the number -> so we move buffer by 4 
				// we need to also do casting(pretypovani, idk jak se to rekne), 
				// to this point we were looking at it as a "letter" but we need to look 
				// at it as a 4byte number that's the * in the uint brackets
				// second * is for telling the compiler to go grab 4bytes and 
				// make it into a number and give it to us(proste dereference)
				uint32_t offset = *(uint32_t*)(buffer_rx + 4);

				int dataLength = receivedLength - 8;

				if (outputFile.is_open()) {
					//seek - finds the cursor, and we also need to give the offset, 
					//because the message could be received not in order
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
	// ----- END OF RECEIVER EDITING -----





		//**********************************************************************

	getchar(); //wait for press Enter
	return 0;
}




Takze takhle myslis kompletne dobry ?
#pragma comment(lib, "ws2_32.lib")

#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>



// IMPORTED LIBRABRIES:
// ten kod v tomto je takzvany header only, takze chilluju
#include "crc.h"

#define TARGET_IP "10.1.3.239"

#define BUFFERS_LEN 1024
#define HEADER_LENGTH 13
#define WAIT_TIMER 10000

#define SENDER
// #define RECEIVER

#ifdef SENDER
#define TARGET_PORT 5111
#define LOCAL_PORT 5222
#endif // SENDER

#ifdef RECEIVER
#define TARGET_PORT 5222
#define LOCAL_PORT 5111
#endif RECEIVER


void createPacketWithoutCRC(char* buffer_tx, std::string identifier, uint32_t offset,
	char alternatingBit, const char* data, int dataLength);
void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength);
void sendPacket(std::string identifier, int dataLength, uint32_t(&table)[256],
	char* alternatingBit, const char* data, SOCKET socketS,
	sockaddr_in addrDest, uint32_t offset);
bool checkReceivedAcknowledge(char* buffer_rx, char alternatingBit,
	uint32_t(&table)[256], int packetLength);
bool checkBufferForCRC(char* buffer_rx, uint32_t(&table)[256], int packetLength);


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
	// TOHLE JE TAKY LEPSI MIT VE SVOJI FUNKCI
	// struct sockaddr_in from;
	// int fromlen = sizeof(from);

	local.sin_family = AF_INET;
	local.sin_port = htons(LOCAL_PORT);
	local.sin_addr.s_addr = INADDR_ANY;


	socketS = socket(AF_INET, SOCK_DGRAM, 0);
	if (bind(socketS, (sockaddr*)&local, sizeof(local)) != 0) {
		printf("Binding error!\n");
		getchar();
		return 1;
	}
	DWORD timeoutMs = 10000;
	// SOL_SOCKET rika ze menime nastaveni na systemove urovni socketu
	// SO_RCVTIMEO rika ze zapiname schopnost received timeout, 
	setsockopt(socketS, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeoutMs, sizeof(timeoutMs));

	// BUFFERY BUDE LEPSI KDYZ SI VYTVORI KAZDY SAM TAM, KDE JE POTREBUJE
	// char buffer_rx[BUFFERS_LEN];
	// char buffer_tx[BUFFERS_LEN];

	// vygeneruju si tu crc tabulku z te pomocne funkce
	uint32_t table[256];
	crc32::generate_table(table);

#ifdef SENDER
	// ---------------- ONDRA COMMENTS - TO BE DELETED ---------------
	// na SEQUENCE NUMBER (jestli prisel tento nebo ten predchozi paket) mi staci 1 bit takzvany alternating bit
	//	- poslu prvni paket s bajtem 0
	//		- kdyz nedostanu potvrzeni, ze si to precetl, budu posilat dal ten samy paket s bajtem 0
	//		- kdyz dostanu potvrzeni, jsem happy a poslu dalsi s bajtem 1
	// - pak pokracuju to same, akorat s bitem 1, pak zas 0 atd


	// paket bude slozeny nasledovne:
	// 4byte - "DATA"
	// 4byte - offset
	// 4byte - CRC
	// 1byte - sequence number
	// 1011byte - data of the picture


	// CRC je generovano pro nuly na indexech 8-11 v paketu
	//  - pak si Vojti budes muset nejdriv vytahnout CRC z indexu 8-11, ty indexy pak vynulovat a pro cely ten paket vcetne vynulovanych indexu zavolat CRC




	// ZVAZ VYMAZANI SLEEPU POKUD TO FUNGUJE I BEZ NICH
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

	char alternatingBit = 0;

	// -------------------------------- HEADER SENDING ---------------------------------
	// takto v c++ spojuju stringy
	std::cout << "The file NAME is: " << fileName << "\n";
	std::cout << "I'm sending the file NAME!\n";
	std::cout << "*********************************************\n";
	sendPacket("NAME", fileName.length(), table, &alternatingBit,
		fileName.c_str(), socketS, addrDest, 0);


	std::string fileSizeString = std::to_string(fileSize);
	std::cout << "The file SIZE is: " << fileSizeString << "\n";
	std::cout << "I'm sending the file SIZE!\n";
	std::cout << "*********************************************\n";
	sendPacket("SIZE", fileSizeString.length(), table, &alternatingBit,
		fileSizeString.c_str(), socketS, addrDest, 0);


	std::cout << "I'm sending the START signal!\n";
	std::cout << "*********************************************\n";
	// nullptr je null pointer coz je kdyz nechci nic odeslat
	sendPacket("STRT", 0, table, &alternatingBit, nullptr, socketS, addrDest, 0);
	// ------------------------------------------------------------------------------


	uint32_t offsetNum = 0;
	int packetNum = 1;


	while (true) {
		char data[BUFFERS_LEN - HEADER_LENGTH] = { 0 };
		file.read(data, BUFFERS_LEN - HEADER_LENGTH);
		int fileReadLen = file.gcount();
		if (fileReadLen == 0) {
			break;
		}
		// to ze funkce bere const char a ja poslu jenom char je uplne vpohode
		// rika mi to ze to uz nebude menit v te funkci
		sendPacket("DATA", fileReadLen, table, &alternatingBit, data, socketS,
			addrDest, offsetNum);
		std::cout << "Sent packet number: " << packetNum << "\n";
		offsetNum += fileReadLen;
		packetNum++;
	}


	std::cout << "All packets were succesfully send!\n";
	std::cout << "*********************************************\n";
	std::cout << "I'm sending the STOP signal!\n";
	sendPacket("STOP", 0, table, &alternatingBit, nullptr,
		socketS, addrDest, 0);

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

		memset(buffer_rx, 0, sizeof(buffer_rx));

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

					std::cout << "Received packet number: " << receivedPackets << std::endl;

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
	getchar();
	return 0;
}

void createPacketWithoutCRC(char* buffer_tx, std::string identifier, uint32_t offset,
	char alternatingBit, const char* data, int dataLength) {
	// nejdriv si buffer hezky vunuluju
	// musim poslat buffers_len protoze sizeof buffer_tx je tady 8 pac pointer
	memset(buffer_tx, 0, BUFFERS_LEN);
	// .c_str() mi zajisti, ze to vezme opravdu jen string, protoze jinak v c++ je
	// string objekt a takto bych tam kopiroval i milion hodnot toho objektu 
	// protoze posilam adresu pouzivam -> 
	memcpy(buffer_tx, identifier.c_str(), 4);
	// nactu offset
	memcpy(buffer_tx + 4, &offset, 4);
	// nactu ze zacatku nulove CRC
	memset(buffer_tx + 8, 0, 4);
	// nactu alternating bit
	memcpy(buffer_tx + 12, &alternatingBit, 1);
	// nactu data:
	memcpy(buffer_tx + 13, data, dataLength);
}

// ten table mam takto protoze funkce crc32::update striktně ocekava
// "odkaz na pole o 256 prvcích"
void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength) {
	uint32_t crc = crc32::update(table, 0, buffer_tx, packetLength);
	memcpy(buffer_tx + 8, &crc, 4);
}

void sendPacket(std::string identifier, int dataLength, uint32_t(&table)[256],
	char* alternatingBit, const char* data, SOCKET socketS,
	sockaddr_in addrDest, uint32_t offset) {
	char buffer_tx[BUFFERS_LEN];
	char buffer_rx[BUFFERS_LEN];
	int packetLength = dataLength + HEADER_LENGTH;
	struct sockaddr_in from;
	int fromlen = sizeof(from);
	createPacketWithoutCRC(buffer_tx, identifier, offset, *alternatingBit, data, dataLength);
	addCRCToPacket(buffer_tx, table, packetLength);
	bool correctMessageReceived = false;
	while (!correctMessageReceived) {
		sendto(socketS, buffer_tx, packetLength, 0, (sockaddr*)&addrDest, sizeof(addrDest));
		Sleep(1);
		int recVal = recvfrom(socketS, buffer_rx, BUFFERS_LEN, 0, (sockaddr*)&from, &fromlen);
		if (recVal == -1) {
			std::cout << "ERROR_TIMEOUT: Resending the packet!\n";
			continue;
		}
		if (!checkReceivedAcknowledge(buffer_rx, *alternatingBit, table, recVal)) {
			std::cout << "ERROR_ACK: Sent acknowlegement does NOT correspond with the expected!\n";
			continue;
		}
		correctMessageReceived = true;
	}
	*alternatingBit = (*alternatingBit == 0) ? 1 : 0;
}

bool checkReceivedAcknowledge(char* buffer_rx, char alternatingBit,
	uint32_t(&table)[256], int packetLength) {
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

bool checkBufferForCRC(char* buffer_rx, uint32_t(&table)[256], int packetLength) {
	char CRC[4];
	memcpy(CRC, buffer_rx + 8, 4);
	memset(buffer_rx + 8, 0, 4);
	uint32_t crc = crc32::update(table, 0, buffer_rx, packetLength);
	uint32_t receivedCRC;
	memcpy(&receivedCRC, CRC, sizeof(uint32_t));
	return receivedCRC == crc;
}