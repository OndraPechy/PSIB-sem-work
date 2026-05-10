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
//x
#include "crc.h"
#include "sha256.h"

#include <stdexcept>
#define TARGET_IP "127.0.0.1"

#define BUFFERS_LEN 1024
#define HEADER_LENGTH 13
#define WAIT_TIMER 500

//#define SENDER
#define RECEIVER

#ifdef SENDER
#define TARGET_PORT 14000
#define LOCAL_PORT 15001
#endif // SENDER

#ifdef RECEIVER
#define LOCAL_PORT 15000
#define TARGET_PORT 14001
#endif // RECEIVER

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

void sendControlPacket(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256],
	const char* id, char alternatingBit);
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
	DWORD timeoutMs = WAIT_TIMER;
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

	// V tuto chvíli už sender odeslal všechny DATA pakety.
// Teď ještě spočítáme SHA-256 hash původního souboru,
// abychom mohli na receiveru ověřit, že se celý soubor přenesl správně.
	std::cout << "All data packets were successfully sent!\n";
	std::cout << "*********************************************\n";

	// Spočítáme SHA-256 hash původního souboru.
	// Funkce calculateSHA256() přečte celý soubor a vrátí hash jako textový řetězec.
	std::string fileHash = calculateSHA256(filePath);

	std::cout << "SHA-256 of original file: " << fileHash << "\n";
	std::cout << "I'm sending the HASH!\n";
	std::cout << "*********************************************\n";

	// Vytvoříme speciální paket typu "HASH".
	// Do datové části paketu vložíme vypočítaný SHA-256 hash.
	// Receiver si tento hash uloží a po přijetí celého souboru ho porovná
	// s hashem, který si sám spočítá z přijatého souboru.
	packetData hashStruct = {
		"HASH",                                  // identifikátor paketu
		fileHash.c_str(),                       // data paketu = SHA-256 hash
		static_cast<int>(fileHash.length()),    // délka hashe v bajtech
		0                                       // offset zde nepotřebujeme
	};

	// HASH paket posíláme stejně jako ostatní pakety,
	// tedy také přes Stop-and-Wait, s CRC a čekáním na ACK.
	sendPacket(hashStruct, myContext, table);

	std::cout << "I'm sending the STOP signal!\n";
	packetData stopStruct = { "STOP", nullptr, 0, 0 };
	sendPacket(stopStruct, myContext, table);

	file.close();

	closesocket(socketS);
#endif











#ifdef RECEIVER
	// musi mit taky vytvorenou svoji vlastni pevnou adresu:
	sockaddr_in addrDest;
	addrDest.sin_family = AF_INET;
	addrDest.sin_port = htons(TARGET_PORT);
	InetPton(AF_INET, _T(TARGET_IP), &addrDest.sin_addr.s_addr);

	// Buffer pro příjem UDP paketů.
	// Do tohoto pole se bude ukládat každý přijatý paket.
	char buffer_rx[BUFFERS_LEN];

	std::cout << "Waiting for data...\n";

	// Výstupní soubor, do kterého bude receiver zapisovat přijatá data.
	std::ofstream outputFile;

	// Pomocná proměnná pro hlavní přijímací smyčku.
	// Dokud je true, receiver stále čeká na další pakety.
	bool isReceiving = true;

	// Název původního souboru, který pošle sender v paketu NAME.
	std::string originalFileName;

	// Název souboru, který receiver vytvoří u sebe.
	// Výchozí hodnota je received.bin, kdyby název od senderu z nějakého důvodu nepřišel.
	std::string outputFileName = "received.bin";

	// Sem se uloží SHA-256 hash, který pošle sender v paketu HASH.
	// Na konci přenosu se porovná s hashem přijatého souboru.
	std::string expectedHash;

	// Očekávaná velikost souboru.
	// Výchozí hodnota -1 znamená, že velikost zatím neznáme.
	long long expectedFileSize = -1;

	// Počítadlo přijatých DATA paketů.
	int receivedPackets = 0;

	// Stop-and-Wait používá alternating bit, tedy střídání 0/1.
	// Receiver tím pozná, jestli přišel nový paket, nebo duplikát starého paketu.
	char expectedBit = 0;

	// Struktura, do které recvfrom() uloží adresu odesílatele.
	// Díky tomu receiver ví, kam má posílat ACK nebo NAK.
	struct sockaddr_in from;

	// Velikost struktury s adresou odesílatele.
	int fromlen = sizeof(from);

	// Hlavní smyčka receiveru.
	// Receiver v ní postupně přijímá pakety, kontroluje CRC,
	// zapisuje data do souboru a posílá ACK/NAK.
	while (isReceiving) {

		// Před každým příjmem buffer vynulujeme,
		// aby v něm nezůstala stará data z předchozího paketu.
		memset(buffer_rx, 0, sizeof(buffer_rx));

		// recvfrom() může změnit hodnotu fromlen,
		// proto ji před každým příjmem nastavíme znovu.
		fromlen = sizeof(from);

		// Čekáme na příjem UDP paketu.
		// Přijatá data se uloží do buffer_rx.
		// Adresa odesílatele se uloží do proměnné from.
		int receivedLength = recvfrom(
			socketS,
			buffer_rx,
			sizeof(buffer_rx),
			0,
			(sockaddr*)&from,
			&fromlen
		);

		// Pokud recvfrom() vrátí SOCKET_ERROR, nastala chyba nebo timeout.
		if (receivedLength == SOCKET_ERROR) {
			int error = WSAGetLastError();

			// Timeout znamená, že receiver zatím nic nepřijal.
			// Není to fatální chyba, takže jen pokračujeme v čekání.
			if (error == WSAETIMEDOUT) {
				continue;
			}

			// Jiná socket chyba už je závažnější,
			// proto ji vypíšeme a ukončíme přijímací smyčku.
			std::cout << "Socket error: " << error << "\n";
			break;
		}

		// Každý náš paket musí mít minimálně hlavičku dlouhou HEADER_LENGTH bajtů.
		// Pokud přišel kratší paket, nemá správný formát a ignorujeme ho.
		if (receivedLength < HEADER_LENGTH) {
			std::cout << "Received packet is too short, ignoring it.\n";
			continue;
		}


		// Sequence number / alternating bit je uložený na indexu 12.
		// Slouží k rozpoznání, jestli přišel nový paket, nebo duplikát.
		char packetBit = buffer_rx[12];

		// První 4 bajty paketu jsou identifikátor typu paketu.
		// Například "NAME", "SIZE", "STRT", "DATA", "HASH", "STOP".
		char packetId[5] = { 0 };
		memcpy(packetId, buffer_rx, 4);

		// Pomocný výpis, abychom v konzoli viděli,
		// jaký paket přišel, s jakým sequence bitem a jakou má délku.
		std::cout << "Packet received: "
			<< packetId
			<< ", seq = "
			<< (int)buffer_rx[12]
			<< ", length = "
			<< receivedLength
			<< " bytes\n";

		// Kontrola CRC celého přijatého paketu.
		// Pokud CRC nesedí, znamená to, že se paket při přenosu poškodil.
		bool crcOk = checkBufferForCRC(buffer_rx, table, receivedLength);

		if (!crcOk) {
			std::cout << "CRC ERROR in packet " << packetId
				<< ", seq = " << (int)packetBit
				<< " -> sending NAK\n";

			// Při chybě CRC pošleme NAK.
			// Sender pak stejný paket odešle znovu.
			sendControlPacket(socketS, addrDest, table, "NAK ", packetBit);
			continue;
		}

		// Pokud přišel paket s jiným bitem, než receiver aktuálně očekává,
		// znamená to nejspíš duplikát už přijatého paketu.
		// Typická situace: receiver už ACK poslal, ale ACK se cestou ztratil.
		// Sender tedy poslal stejný paket znovu.
		if (packetBit != expectedBit) {
			std::cout << "Duplicate packet " << packetId
				<< ", seq = " << (int)packetBit
				<< " -> sending ACK again\n";

			// Duplikát znovu nezapisujeme do souboru.
			// Jen znovu pošleme ACK, aby sender mohl pokračovat.
			sendControlPacket(socketS, addrDest, table, "ACK ", packetBit);
			continue;
		}

		// Délka datové části paketu.
		// Od celkové délky odečteme délku hlavičky.
		int payloadLength = receivedLength - HEADER_LENGTH;

		// Ukazatel na začátek datové části.
		// Data začínají až za 13bajtovou hlavičkou.
		char* payload = buffer_rx + HEADER_LENGTH;

		// Pomocná proměnná určující, jestli se paket podařilo správně zpracovat.
		// Pokud ano, pošleme ACK. Pokud ne, pošleme NAK.
		bool packetProcessedCorrectly = true;

		// Paket NAME obsahuje název původního souboru.
		if (memcmp(buffer_rx, "NAME", 4) == 0) {
			originalFileName.assign(payload, payloadLength);

			// Pokud by název nepřišel nebo byl prázdný,
			// uložíme soubor pod výchozím názvem received.bin.
			if (originalFileName.empty()) {
				outputFileName = "received.bin";
			}
			else {
				// Přijatý soubor ukládáme s prefixem "received_",
				// abychom si nepřepsali původní soubor při testování na jednom PC.
				outputFileName = "received_" + originalFileName;
			}

			std::cout << "File name received: " << originalFileName << "\n";
			std::cout << "Output file will be: " << outputFileName << "\n";
		}

		// Paket SIZE obsahuje velikost původního souboru jako text.
		else if (memcmp(buffer_rx, "SIZE", 4) == 0) {
			std::string sizeString(payload, payloadLength);

			try {
				// Textovou velikost převedeme na číslo.
				expectedFileSize = std::stoll(sizeString);

				std::cout << "File size received: "
					<< expectedFileSize
					<< " bytes\n";
			}
			catch (...) {
				// Pokud převod selže, paket nepovažujeme za správně zpracovaný.
				std::cout << "Could not parse file size.\n";
				packetProcessedCorrectly = false;
			}
		}

		// Paket STRT značí začátek přenosu dat.
		// Receiver si v tu chvíli otevře výstupní soubor.
		else if (memcmp(buffer_rx, "STRT", 4) == 0) {
			std::cout << "START received, opening output file...\n";

			outputFile.open(outputFileName, std::ios::binary);

			if (!outputFile.is_open()) {
				std::cout << "Could not create output file!\n";
				packetProcessedCorrectly = false;
			}
			else {
				// Po úspěšném otevření souboru vynulujeme počítadlo DATA paketů.
				receivedPackets = 0;
				std::cout << "Output file opened successfully.\n";
			}
		}

		// Paket DATA obsahuje část souboru.
		else if (memcmp(buffer_rx, "DATA", 4) == 0) {
			uint32_t offset = 0;

			// Offset je uložený v hlavičce na bajtech 4-7.
			// Říká, na jakou pozici v souboru máme tento blok dat zapsat.
			memcpy(&offset, buffer_rx + 4, sizeof(uint32_t));

			if (!outputFile.is_open()) {
				std::cout << "Output file is not open, DATA packet ignored.\n";
				packetProcessedCorrectly = false;
			}
			else {
				// Posuneme zapisovací pozici v souboru podle offsetu.
				outputFile.seekp(offset, std::ios::beg);

				// Zapíšeme datovou část paketu do souboru.
				outputFile.write(payload, payloadLength);

				if (!outputFile.good()) {
					std::cout << "Error while writing DATA packet to file.\n";
					packetProcessedCorrectly = false;
				}
				else {
					receivedPackets++;

					std::cout << "Received DATA packet number: "
						<< receivedPackets
						<< ", offset: "
						<< offset
						<< ", data length: "
						<< payloadLength
						<< "\n";
				}
			}
		}

		// Paket HASH obsahuje SHA-256 hash původního souboru.
		// Sender ho posílá po všech DATA paketech a před STOP.
		else if (memcmp(buffer_rx, "HASH", 4) == 0) {
			expectedHash.assign(payload, payloadLength);

			std::cout << "Expected SHA-256 received: "
				<< expectedHash
				<< "\n";
		}

		// Paket STOP znamená konec přenosu.
		// Receiver zavře soubor a provede finální kontrolu integrity.
		else if (memcmp(buffer_rx, "STOP", 4) == 0) {
			std::cout << "STOP received, closing output file...\n";

			if (outputFile.is_open()) {
				outputFile.close();
			}

			// Pokud jsme dostali HASH paket, spočítáme SHA-256 přijatého souboru.
			if (!expectedHash.empty()) {
				try {
					std::string receivedHash = calculateSHA256(outputFileName);

					std::cout << "SHA-256 of received file: "
						<< receivedHash
						<< "\n";

					// Porovnání hashe od senderu a hashe nově vytvořeného souboru.
					// Pokud jsou stejné, celý soubor se přenesl správně.
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
					std::cout << "Could not calculate SHA-256: "
						<< e.what()
						<< "\n";
				}
			}
			else {
				// Pokud HASH paket nepřišel, nelze ověřit integritu celého souboru.
				std::cout << "WARNING: HASH packet was not received.\n";
			}

			// Vedle SHA-256 ještě kontrolujeme i velikost přijatého souboru.
			// Hlavní kontrola integrity je ale SHA-256.
			if (expectedFileSize >= 0) {
				try {
					long long receivedFileSize = std::filesystem::file_size(outputFileName);

					std::cout << "Expected file size: "
						<< expectedFileSize
						<< " bytes\n";

					std::cout << "Received file size: "
						<< receivedFileSize
						<< " bytes\n";

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

			// Přenos skončil, ukončíme hlavní přijímací smyčku.
			sendControlPacket(socketS, addrDest, table, "ACK ", packetBit);
			isReceiving = false;
		}

		// Pokud přišel neznámý typ paketu, považujeme ho za chybný.
		else {
			std::cout << "Unknown packet type: " << packetId << "\n";
			packetProcessedCorrectly = false;
		}

		// Pokud byl paket správně zpracován, pošleme ACK.
		// Teprve potom změníme očekávaný alternating bit.
		if (packetProcessedCorrectly) {
			std::cout << "CRC OK / packet processed -> sending ACK, seq = "
				<< (int)packetBit
				<< "\n";

			sendControlPacket(socketS, addrDest, table, "ACK ", packetBit);

			// Po správném paketu očekáváme příště opačný bit.
			expectedBit = (expectedBit == 0) ? 1 : 0;
		}
		else {
			// Pokud paket nešel správně zpracovat,
			// pošleme NAK a sender ho pošle znovu.
			std::cout << "Packet was not processed correctly -> sending NAK, seq = "
				<< (int)packetBit
				<< "\n";

			sendControlPacket(socketS, addrDest, table, "NAK ", packetBit);
		}
	}

	std::cout << "Receiving finished.\n";

	// Zavření socketu po skončení příjmu.
	closesocket(socketS);

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
void addCRCToPacket(char* buffer_tx, uint32_t(&table)[256], int packetLength) {
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

// POZOR V MAIN KODU JE V TETO FUNKCI DULEZITA ZMENA A JE PRIDANA JEDNA FUNKCE
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
		std::cout << "ACK received for packet "
			<< sendData.identifier
			<< ", seq = "
			<< (int)info.alternatingBit
			<< "\n";

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
	if (packetLength < HEADER_LENGTH) {
		return false;
	}
	// zkontroluju ze sedi message zpetne poslana ACK
	if (memcmp(buffer_rx, "ACK ", 4) != 0) {
		return false;
		// taky ze sedi alternating bit
	}
	else if (buffer_rx[12] != alternatingBit) {
		return false;
		// a taky ze sedi CRC
	}
	else if (!checkBufferForCRC(buffer_rx, table, packetLength)) {
		return false;
	}
	else {
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
	uint32_t generatedCRC = crc32::update(table, 0, buffer_rx, packetLength);
	uint32_t receivedCRC;
	memcpy(&receivedCRC, CRC, sizeof(uint32_t));
	return receivedCRC == generatedCRC;
}

/**
 * @brief Odešle řídicí paket typu ACK nebo NAK zpět odesílateli.
 *
 * Funkce vytvoří krátký řídicí paket bez datové části. Používá stejný formát
 * paketu jako ostatní zprávy v protokolu: identifikátor, offset, CRC,
 * alternating bit a případná data. U ACK/NAK paketů ale datová část není potřeba.
 *
 * Funkce nejprve sestaví paket bez CRC pomocí createPacketWithoutCRC(),
 * potom do něj doplní CRC pomocí addCRCToPacket() a nakonec jej odešle přes UDP
 * pomocí sendto().
 *
 * @param socketS Socket, přes který se bude řídicí paket odesílat.
 * @param addrDest Adresa příjemce, tedy většinou adresa původního odesílatele datového paketu.
 * @param table CRC32 lookup tabulka použitá pro výpočet kontrolního součtu.
 * @param id Identifikátor řídicího paketu, typicky "ACK " nebo "NAK ".
 * @param alternatingBit Sequence number / alternating bit potvrzovaného paketu.
 */
void sendControlPacket(SOCKET socketS, sockaddr_in& addrDest, uint32_t(&table)[256], const char* id, char alternatingBit)
{
	char buffer_tx[BUFFERS_LEN];

	packetData controlPacket;
	controlPacket.identifier = std::string(id, 4);
	controlPacket.data = nullptr;
	controlPacket.length = 0;
	controlPacket.offset = 0;

	createPacketWithoutCRC(buffer_tx, controlPacket, alternatingBit);
	addCRCToPacket(buffer_tx, table, HEADER_LENGTH);

	sendto(
		socketS,
		buffer_tx,
		HEADER_LENGTH,
		0,
		(sockaddr*)&addrDest,
		sizeof(addrDest)
	);
}

/**
 * @brief Spočítá SHA-256 hash zadaného souboru.
 *
 * Funkce otevře soubor v binárním režimu a postupně ho čte po blocích.
 * Každý načtený blok předá objektu SHA256 z použité hashovací knihovny.
 * Po přečtení celého souboru vrátí výsledný SHA-256 hash jako textový řetězec.
 *
 * Tato funkce slouží ke kontrole integrity celého souboru. Sender pomocí ní
 * spočítá hash původního souboru a odešle ho v paketu "HASH". Receiver potom
 * stejnou funkcí spočítá hash přijatého souboru a oba hashe porovná.
 *
 * @param filePath Cesta k souboru, ze kterého se má SHA-256 hash vypočítat.
 * @return SHA-256 hash souboru jako textový řetězec.
 *
 * @throws std::runtime_error Pokud se soubor nepodaří otevřít.
 */
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

