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
void createPacketWithoutCRC(char* buffer_tx, packetData sendData, char alternatingBit);
void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength);
void sendPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256]);
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
	//  - pak si Vojti budes muset nejdriv vytahnout CRC z indexu 8-11, ty indexy pak vynulovat a pro cely ten paket vcetne vynulovanych indexu zavolat CRC


	// ZVAZ VYMAZANI SLEEPU POKUD TO FUNGUJE I BEZ NICH
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	// vytvorim si tady ten muj struct
	senderInfo myContext;
	myContext.socketS = socketS;
	myContext.addrDest = addrDest;
	myContext.alternatingBit = 0;

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
	int nameStrigLength = fileName.length();
	packetData nameStruct = { "NAME", fileName.c_str(), nameStrigLength, 0 };
	sendPacket(nameStruct, myContext, table);

	std::string fileSizeString = std::to_string(fileSize);
	std::cout << "The file SIZE is: " << fileSizeString << "\n";
	std::cout << "I'm sending the file SIZE!\n";
	std::cout << "*********************************************\n";
	int sizeStringLength = fileSizeString.length();
	packetData sizeStruct = { "SIZE", fileSizeString.c_str(), sizeStringLength, 0 };
	sendPacket(sizeStruct, myContext, table);

	std::cout << "I'm sending the START signal!\n";
	std::cout << "*********************************************\n";
	// nullptr je null pointer coz je kdyz nechci nic odeslat
	packetData startStruct = { "STRT", nullptr, 0, 0 };
	sendPacket(startStruct, myContext, table);
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
		packetData dataStruct = { "DATA", data, fileReadLen, offsetNum };
		sendPacket(dataStruct, myContext, table);
		std::cout << "Sent packet number: " << packetNum << "\n";
		offsetNum += fileReadLen;
		packetNum++;
	}


	std::cout << "All packets were succesfully send!\n";
	std::cout << "*********************************************\n";
	std::cout << "I'm sending the STOP signal!\n";
	packetData stopStruct = { "STOP", nullptr, 0, 0 };
	sendPacket(stopStruct, myContext, table);

	file.close();

	closesocket(socketS);
#endif











#ifdef RECEIVER
	char buffer_tx[BUFFERS_LEN];
	char buffer_rx[BUFFERS_LEN];

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















/**
 * @brief Sestaví síťový paket v bufferu bez výpočtu kontrolního součtu (CRC).
 *
 * Funkce postupně serializuje identifikátor, offset, nulové CRC, alternating bit
 * a samotná data do předem alokovaného pole.
 *
 * @param buffer_tx      Ukazatel na cílový buffer, kam se paket sestaví.
 * @param sendData       Struktura obsahující data k odeslání (ID, offset, délka, data).
 * @param alternatingBit Hodnota bitu pro řízení logiky potvrzování (0 nebo 1).
 */
void createPacketWithoutCRC(char* buffer_tx, packetData sendData, char alternatingBit) {
	// nejdriv si buffer hezky vunuluju
	// musim poslat buffers_len protoze sizeof buffer_tx je tady 8 pac pointer
	memset(buffer_tx, 0, BUFFERS_LEN);
	// .c_str() mi zajisti, ze to vezme opravdu jen string, protoze jinak v c++ je
	// string objekt a takto bych tam kopiroval i milion hodnot toho objektu 
	// protoze posilam adresu pouzivam -> 
	memcpy(buffer_tx, sendData.identifier.c_str(), 4);
	// nactu offset
	memcpy(buffer_tx + 4, &sendData.offset, 4);
	// nactu ze zacatku nulove CRC
	memset(buffer_tx + 8, 0, 4);
	// nactu alternating bit
	memcpy(buffer_tx + 12, &alternatingBit, 1);
	// nactu data:
	// tohle kontroluju protoze memcpy ve spolupraci s nullptr by mi failnul
	if (sendData.length > 0) {
		memcpy(buffer_tx + 13, sendData.data, sendData.length);
	}
}


/**
 * @brief Vypočítá CRC32 pro celý paket a zapíše jej na vyhrazenou pozici v bufferu.
 * * Funkce využívá externí knihovnu/třídu crc32 k výpočtu kontrolního součtu z dat
 * v bufferu a následně výsledek uloží na 8. až 11. bajt paketu.
 * * @param buffer_tx    Ukazatel na buffer s již sestaveným paketem (předpokládá offset 8 pro CRC).
 * @param table        Reference na vyhledávací tabulku (lookup table) pro algoritmus CRC32 o 256 prvcích.
 * @param packetLength Celková délka paketu v bajtech, ze kterých se má CRC počítat.
 */
// ten table mam takto protoze funkce crc32::update striktně ocekava
// "odkaz na pole o 256 prvcích"
void addCRCToPacket(char *buffer_tx, uint32_t(&table)[256], int packetLength) {
	uint32_t crc = crc32::update(table, 0, buffer_tx, packetLength);
	memcpy(buffer_tx + 8, &crc, 4);
}


/**
 * @brief Zajišťuje spolehlivé odeslání paketu pomocí mechanismu Stop-and-Wait.
 *
 * Funkce sestaví paket, vypočítá CRC a v cyklu jej odesílá na cílovou adresu, dokud
 * neobdrží validní potvrzení (ACK). Implementuje timeout a kontrolu integrity
 * přijatého potvrzení. Po úspěšném doručení změní hodnotu alternating bitu.
 *
 * @param sendData  Struktura s daty, která mají být odeslána.
 * @param info      Odkaz na strukturu s informacemi o odesílateli (socket, adresa, stavový bit).
 * @param table     Reference na vyhledávací tabulku pro výpočet CRC32.
 */
// info musim poslat jako odkaz pac upravuju alternating bit
void sendPacket(packetData sendData, senderInfo& info, uint32_t(&table)[256]) {
	// vytvorim si tady svoje vlastni buffery
	char buffer_tx[BUFFERS_LEN];
	char buffer_rx[BUFFERS_LEN];
	int packetLength = sendData.length + HEADER_LENGTH;
	// i tu socket adresu je lepsi vytvorit vlastni uvnitr funkce
	struct sockaddr_in from;
	int fromlen = sizeof(from);

	createPacketWithoutCRC(buffer_tx, sendData, info.alternatingBit);
	addCRCToPacket(buffer_tx, table, packetLength);

	// udelam loop ktery bude posilat tak dlouho, dokud nedostane spravnou message
	bool correctMessageReceived = false;
	while (!correctMessageReceived) {
		sendto(info.socketS, buffer_tx, packetLength, 0, (sockaddr*)&info.addrDest, sizeof(info.addrDest));
		Sleep(1);
		int recVal = recvfrom(info.socketS, buffer_rx, BUFFERS_LEN, 0, (sockaddr*)&from, &fromlen);
		if (recVal == -1) {
			std::cout << "ERROR_TIMEOUT: Haven't received a response in time!\n";
			std::cout << "Resending the packet!\n";
		
			continue;
		}
		if (!checkReceivedAcknowledge(buffer_rx, info.alternatingBit, table, recVal)) {
			std::cout << "ERROR_ACK: Received acknowlegement does NOT correspond with the expected!\n";
			std::cout << "Resending the packet!\n";
			continue;
		}
		correctMessageReceived = true;
	}
	// pozmenim si ten svuj bit na novy
	info.alternatingBit = (info.alternatingBit == 0) ? 1 : 0;
}


/**
 * @brief Prověřuje, zda přijatý paket je validním potvrzením (ACK).
 *
 * Kontroluje tři aspekty: zda zpráva začíná identifikátorem "ACK ", zda se shoduje
 * přijatý alternating bit s očekávaným a zda souhlasí kontrolní součet CRC.
 *
 * @param buffer_rx      Ukazatel na buffer s přijatými daty.
 * @param alternatingBit Očekávaná hodnota stavového bitu.
 * @param table          Reference na vyhledávací tabulku pro výpočet CRC32.
 * @param packetLength   Délka přijatého paketu.
 * @return true          Pokud je potvrzení v pořádku a odpovídá očekávání.
 * @return false         V případě chyby v datech, bitu nebo kontrolním součtu.
 */
bool checkReceivedAcknowledge(char* buffer_rx, char alternatingBit,
							  uint32_t(&table)[256], int packetLength) {
	// zkontroluju ze sedi message zpetne poslana ACK
	if (memcmp(buffer_rx, "ACK ", 4) != 0) {
		return false;
	// taky ze sedi alternating bit
	} else if (buffer_rx[12] != alternatingBit) {
		return false;
	// a taky ze sedi CRC
	} else if (!checkBufferForCRC(buffer_rx, table, packetLength)) {
		return false;
	} else {
		return true;
	}
}


/**
 * @brief Verifikuje integritu dat v bufferu pomocí CRC32.
 *
 * Funkce dočasně vyjme přijaté CRC z bufferu, místo něj vloží nuly a vypočítá
 * nový kontrolní součet z obsahu bufferu. Ten následně porovná s původním
 * přijatým CRC.
 *
 * @param buffer_rx    Ukazatel na buffer s daty k prověření.
 * @param table        Reference na vyhledávací tabulku pro výpočet CRC32.
 * @param packetLength Celková délka paketu v bufferu.
 * @return true        Pokud vypočítaný CRC souhlasí s přijatým.
 * @return false       Pokud došlo k poškození dat při přenosu.
 */
bool checkBufferForCRC(char* buffer_rx, uint32_t(&table)[256], int packetLength) {
	char CRC[4];
	memcpy(CRC, buffer_rx + 8, 4);
	memset(buffer_rx + 8, 0, 4);
	uint32_t crc = crc32::update(table, 0, buffer_rx, packetLength);
	uint32_t receivedCRC;
	memcpy(&receivedCRC, CRC, sizeof(uint32_t));
	return receivedCRC == crc;
}

