// UDP_Communication_Framework.cpp : Defines the entry point for the console application.
//
#pragma comment(lib, "ws2_32.lib")
#include "stdafx.h"
#include <winsock2.h>
#include "ws2tcpip.h"


// TODO: add system libraries for reading 
#include <iostream>
#include <fstream>
#include <vector>
#include <string>



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
		/*
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


						else if (strncmp(buffer_rx, "START", 5) == 0) {
							std::cout << "We have received START and opening the file!\n";
							//opening the file in binary
							outputFile.open(filename, std::ios::binary);
							if (!outputFile.is_open()) {
								std::cout << "We could not create the file!\n";
							}
						}
		*/
		// ---------------------- END OF PREVIOUS VOJTA'S CODE ----------------------
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


