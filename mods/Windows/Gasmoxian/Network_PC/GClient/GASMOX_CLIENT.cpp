#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Psapi.h>
#include <time.h>
#include <atomic>
//#include <chrono>
//#include <thread>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#define WINDOWS_INCLUDE
#include "../../../../../decompile/General/AltMods/Gasmoxian/global.h"
#include <enet/enet.h>

clock_t timeStart;
clock_t warpclockdelay;
clock_t  squaredelay [MAX_NUM_PLAYERS];

char* pBuf;
OnlineCTR* octr;

int buttonPrev[8] = { 0 };
unsigned char name[100];

ENetHost* clientHost;
ENetPeer* serverPeer;

#ifdef __WINDOWS__
void usleep(__int64 usec);
#endif

std::atomic<bool> lockengineandcharacter(false);
int prev_warpclock = -1;
int prev_special = -1;
int prev_finishtimer = -1;
int alreadysended = 0;
int sendwarpclock = 0;
int extralaps = 0;



struct Gamepad
{
	short unk_0;
	short unk_1;
	short stickLX;
	short stickLY;
	short stickLX_dontUse1;
	short stickLY_dontUse1;
	short stickRX;
	short stickRY;
	int buttonsHeldCurrFrame;
	int buttonsTapped;
	int buttonsReleased;
	int buttonsHeldPrevFrame;
};

void sendToHostUnreliable(const void* data, size_t size) {
	ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_UNSEQUENCED);
	enet_peer_send(serverPeer, 0, packet); // To do: have a look at the channels, maybe we want to use them better to categorize messages
}

void sendToHostReliable(const void* data, size_t size) {
	ENetPacket* packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
	enet_peer_send(serverPeer, 0, packet); // To do: have a look at the channels, maybe we want to use them better to categorize messages
}

void ProcessReceiveEvent(ENetPacket* packet)
{
	SG_Header* recvBuf = reinterpret_cast<SG_Header*>(packet->data);
	int slot;

	//printf("received packet with type %i\n",((struct SG_Header*)recvBuf)->type);
	// switch will compile into a jmp table, no funcPtrs needed
	switch (recvBuf->type)
	{
		case SG_ROOMS:
		{
			SG_MessageRooms* r = reinterpret_cast<SG_MessageRooms*>(recvBuf);

			octr->ver_pc = GASMOXIAN_VER;
			octr->ver_server = r->version;

				if (r->version != GASMOXIAN_VER)
				{
					octr->CurrState = LAUNCH_ERROR;
					return;
				}

			if (octr->ver_psx != GASMOXIAN_VER)
			{
				octr->CurrState = LAUNCH_ERROR;
				return;
			}


			// reopen the room menu,
			// either first time getting rooms,
			// or refresh after joining refused
			octr->serverLockIn2 = 0;

			octr->numRooms = r->numRooms;

			octr->clientCount[0x0] = r->numClients01;
			octr->clientCount[0x1] = r->numClients02;
			octr->clientCount[0x2] = r->numClients03;
			octr->clientCount[0x3] = r->numClients04;
			octr->clientCount[0x4] = r->numClients05;
			octr->clientCount[0x5] = r->numClients06;
			octr->clientCount[0x6] = r->numClients07;
			octr->clientCount[0x7] = r->numClients08;
			octr->clientCount[0x8] = r->numClients09;
			octr->clientCount[0x9] = r->numClients10;
			octr->clientCount[0xa] = r->numClients11;
			octr->clientCount[0xb] = r->numClients12;
			octr->clientCount[0xc] = r->numClients13;
			octr->clientCount[0xd] = r->numClients14;
			octr->clientCount[0xe] = r->numClients15;
			octr->clientCount[0xf] = r->numClients16;
			break;
		}

		// Assigned to room
		case SG_NEWCLIENT:
		{
			SG_MessageClientStatus* r = reinterpret_cast<SG_MessageClientStatus*>(recvBuf);


			octr->DriverID = r->clientID;
			octr->NumDrivers = r->numClientsTotal;

			// offset 0x8
			octr->boolLockedInLap = 0;
			octr->boolLockedInLevel = 0;
			octr->boolLockedInEngine = 0;
			octr->boolLockedInSpecial = 0;
			octr->lapID = 0;
			octr->special = 0;
			octr->levelID = 0;
			octr->boolLockedInCharacter = 0;
			octr->numDriversEnded = 0;
			octr->finishracetimer = 0;
			octr->warpclock = 0;
			lockengineandcharacter = false;
			alreadysended = 0;
			sendwarpclock = 0;
			prev_warpclock = octr->warpclock;
			prev_special = -1;
			prev_finishtimer = -1;
			extralaps = 0;



			if (octr->serverRoom == 15)
			{
				printf("\n EASTER EGG: SAFFI FIRE UNLOCKED IN THIS ROOM \n");
			}

			memset(&octr->boolLockedInCharacters[0], 0, sizeof(octr->boolLockedInCharacters));
			memset(&squaredelay[0], 0, sizeof(squaredelay));
			memset(&octr->boolLockedInEnginee[0], 0, sizeof(octr->boolLockedInEnginee));
			memset(&octr->nameBuffer[0], 0, sizeof(octr->nameBuffer));
			memset(&octr->raceStats[0], 0, sizeof(octr->raceStats));

			// reply to server with your name
			memcpy(&octr->nameBuffer[0], &name, NAME_LEN);
			octr->nameBuffer[0][NAME_LEN] = '\0';

			CG_MessageName m = { 0 };
			m.type = CG_NAME;
			memcpy(&m.name[0], &name[0], sizeof(m.name));
			sendToHostReliable(&m, sizeof(CG_MessageName));

			// choose to get host menu or guest menu
			octr->CurrState = LOBBY_ASSIGN_ROLE;
			break;
		}

		case SG_NAME:
		{
			SG_MessageName* r = reinterpret_cast<SG_MessageName*>(recvBuf);

			int clientID = r->clientID;
			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;

			octr->NumDrivers = r->numClientsTotal;

			r->name[NAME_LEN] = '\0';

			memcpy(&octr->nameBuffer[slot], &r->name[0], NAME_LEN);

			// handle disconnection //if not in race all players should hold square to avoid bugs
			if (r->name[0] == 0 || octr->CurrState <= LOBBY_WAIT_FOR_LOADING)
			{
				// make this player hold SQUARE
				Gamepad* gamepad = (Gamepad*)&pBuf[(0x80096804 + (slot * 0x50)) & 0xffffff];
				gamepad->buttonsHeldCurrFrame = 0x20;
				gamepad->buttonsTapped = 0;
				gamepad->buttonsReleased = 0;
				gamepad->buttonsHeldPrevFrame = 0x20;
			}

			break;
		}

		case SG_TRACK:
		{
			SG_MessageTrack* r = reinterpret_cast<SG_MessageTrack*>(recvBuf);


			unsigned char numLaps = (r->lapID * 2) + 1;

			if (r->lapID >= 8)
			{
				extralaps = 1;
			}

			int lapValues[] = { 10, 15, 20, 25, 30, 35, 40, 50, 69, 80, 90, 127 };

			if (r->lapID >= 4 && r->lapID <= 15) {
				numLaps = lapValues[r->lapID - 4];
			}



			unsigned char* numLapsV = (unsigned char*)&pBuf[(0x80096b20 + 0x1d33) & 0xffffff];
			*numLapsV = numLaps;

			octr->levelID = r->trackID;


			octr->CurrState = LOBBY_SPECIALPICK;


			break;
		}

		case SG_SPECIAL:
		{
			SG_MessageSpecial* r = reinterpret_cast<SG_MessageSpecial*>(recvBuf);


			// default, disable cheats
//ps1ptr<int> cheats = pBuf.at<int>(0x80096b28);
			int* cheats = (int*)&pBuf[0x80096b28 & 0xffffff];
			*cheats &= ~(0x100000 | 0x80000 | 0x400 | 0x80000 | 0x400000 | 0x8000000 | 0x10000);

			//pbuf= duckstation shared memory
			*(int*)&pBuf[(0x80096b28) & 0xffffff] = 0x8000000;                  //turbo counter cheat 0x8000000


					// update the value in the client
					octr->special = r->special;

					prev_special = r->special;


					//in the future i will try to replace this with an array

					if (octr->special == 0) {

					}
					// special 1
					else if (octr->special == 1) {
						printf("\033[1;36m\n  MODE: 🪞 MIRROR 🪞 \n\033[0m");

					}
					//special 2
					else if (octr->special == 2) {           
						printf("\033[1;34m\n MODE: ❄️ ICY TRACKS ❄️ \n\033[0m");
						// icy tracks cheat
						*(int*)&pBuf[(0x80096b28) & 0xffffff] = 0x80000;

					}
					//special 3
					else if (octr->special == 3) {      
						printf("\n MODE: 💥 ITEMLESS 💥 \n");

					}
					//special 4
					else if (octr->special == 4) { 
						printf("\033[1;33m\n MODE: 🌙 MOON 🌙 \n\033[0m");
					}
					//special 5
					else if (octr->special == 5) { 
						printf("\033[1;38;5;214m\n MODE: 🔥 RETRO FUELED 🔥 \n\033[0m");

						//superturbo pad cheat
						*(int*)&pBuf[(0x80096b28) & 0xffffff] = 0x100000;
					}
					//special 6
					else if (octr->special == 6) { 
						printf("\033[1;35m\n MODE: ✨ VOID WORLD ✨ \n\033[0m");
					}
					//special 7
					else if (octr->special == 7) { 
						printf("\033[1;31m\n MODE: 👔 BOSS RACE 👔 \n\033[0m");


					}
					else if (octr->special == 8) {
						printf("\033[0;33m\n MODE: 📷 DEMO CAMERA 📷 \n\033[0m");


					}

			octr->CurrState = LOBBY_CHARACTER_PICK;
			break;
		}
		case SG_CHARACTER:
		{

			SG_MessageCharacter* r = reinterpret_cast<SG_MessageCharacter*>(recvBuf);

			unsigned char clientID = r->clientID;
			unsigned char characterID = r->characterID;
			//unsigned char ogEngine = r-> characterID;
			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;

			short* characterIDV = (short*)&pBuf[(0x80086e84 + (2 * slot)) & 0xffffff];
			*characterIDV = characterID;

			//this defines the default engine if enginetype is 4-5, but only for other players.
			//needed because my code in engineaudio overwrittes the value in zdataglobal ._XD
			//anyways it is better than only have the default audio

			//int desiredengines[] = { 0, 0, 2, 1, 1, 2, 3, 3,
			//		  1, 2, 3, 0, 2, 3, 0, 0 };

			//ogEngine = desiredengines[r->characterID];
			//octr->OGengine[slot] = ogEngine;


			octr->boolLockedInCharacters[clientID] = r->boolLockedIn;


			
			break;
		}
		case SG_ENGINE:
		{
			SG_MessageEngine* r = reinterpret_cast<SG_MessageEngine*>(recvBuf);

			unsigned char clientID = r->clientID;
			unsigned char engineID = r->enginetype;


			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;
			
			//it works i love onlinectr

			octr->enginetype[slot] = (engineID > 3) ? 3 : engineID;

			octr->boolLockedInEnginee[clientID] = r->boolLockedIn;


			
			break;
		}

		case SG_STARTLOADING:
		{
			// variable reuse, wait a few frames,
			// so screen updates with green names
			octr->CountPressX = 0;
			octr->CurrState = LOBBY_START_LOADING;

			break;
		}

		case SG_STARTRACE:
		{
			octr->CurrState = GAME_START_RACE;

			break;
		}

		case SG_RACEDATA:
		{

			if (octr->CurrState < GAME_WAIT_FOR_RACE)
				break;

			int sdata_Loading_stage =
				*(int*)&pBuf[0x8008d0f8 & 0xffffff];

			if (sdata_Loading_stage != -1)
				break;

			SG_EverythingKart* r = reinterpret_cast<SG_EverythingKart*>(recvBuf);

			int clientID = r->clientID;
			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;

			//ps1ptr<Gamepad> gamepad = pBuf.at<Gamepad>(0x80096804 + (slot * 0x50), false); //do not prefetch so we can fetch concurrently
			//ps1ptr<int> psxPtr = pBuf.at<int>(0x8009900c + (slot * 4), false); //do not prefetch so we can fetch concurrently

			int curr = r->buttonHold;

			// sneak L1/R1 into one byte,
			// remove Circle/L2

			if ((curr & 0x40) != 0)
			{
				curr &= ~(0x40);
				curr |= 0x400;
			}

			if ((curr & 0x80) != 0)
			{
				curr &= ~(0x80);
				curr |= 0x800;
			}

			int prev = buttonPrev[slot];

			// tapped
			int tap = ~prev & curr;

			// released
			int rel = prev & ~curr;

			Gamepad* gamepad = (Gamepad*)&pBuf[(0x80096804 + (slot * 0x50)) & 0xffffff];
			gamepad->buttonsHeldCurrFrame = curr;
			gamepad->buttonsTapped = tap;
			gamepad->buttonsReleased = rel;
			gamepad->buttonsHeldPrevFrame = prev;

			// In this order: Up, Down, Left, Right
			if ((gamepad->buttonsHeldCurrFrame & 1) != 0) gamepad->stickLY = 0;
			else if ((gamepad->buttonsHeldCurrFrame & 2) != 0) gamepad->stickLY = 0xFF;
			else gamepad->stickLY = 0x80;

			if ((gamepad->buttonsHeldCurrFrame & 4) != 0) gamepad->stickLX = 0;
			else if ((gamepad->buttonsHeldCurrFrame & 8) != 0) gamepad->stickLX = 0xFF;
			else gamepad->stickLX = 0x80;

			buttonPrev[slot] = curr;

			int psxPtr = *(int*)&pBuf[(0x8009900c + (slot * 4)) & 0xffffff];
			psxPtr &= 0xffffff;

			// lossless compression, bottom byte is never used,
			// cause psx renders with 3 bytes, and top byte
			// is never used due to world scale (just pure luck)
			int* x = (int*)&pBuf[psxPtr + 0x2d4];
			int* y = (int*)&pBuf[psxPtr + 0x2d8];
			int* z = (int*)&pBuf[psxPtr + 0x2dc];
			*x = ((int)r->posX) * 256;
			*y = ((int)r->posY) * 256;
			*z = ((int)r->posZ) * 256;

			int angle =
				(r->kartRot1) |
				(r->kartRot2 << 5);
			angle &= 0xfff;

			short* ang = (short*)&pBuf[psxPtr + 0x39a];
			*ang = (short)angle;

			// keep setting to 200,
			// and if !boolReserves, let it fall to zero
			if (r->boolReserves)
			{
				short* reserv = (short*)&pBuf[psxPtr + 0x3e2];
				*reserv = 200;
			}

			short* wumpa = (short*)&pBuf[psxPtr + 0x30];
			*wumpa = r->wumpa;

			break;
		}


		case SG_WEAPON:
		{
			SG_MessageWeapon* r = reinterpret_cast<SG_MessageWeapon*>(recvBuf);

			int clientID = r->clientID;
			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;

			octr->Shoot[slot].boolNow = 1;
			octr->Shoot[slot].Weapon = r->weapon;
			octr->Shoot[slot].boolJuiced = r->juiced;
			octr->Shoot[slot].flags = r->flags;
			
			

			break;
		}

		case SG_WARPCLOCK:
		{
			SG_MessageWarpclock* r = reinterpret_cast<SG_MessageWarpclock*>(recvBuf);

			//at least it works xd
			prev_warpclock = r->warpclock;

			if (octr->warpclock != prev_warpclock) {
				octr->warpclock = r->warpclock;
			}
			

			break;
		}
		case SG_FINISHTIMER:
		{
			SG_MessageFinishTimer* r = reinterpret_cast<SG_MessageFinishTimer*>(recvBuf);
			if (r->finishracetimer != prev_finishtimer) {
				octr->finishracetimer = r->finishracetimer;
				prev_finishtimer = octr->finishracetimer;
			}
			

			break;
		}
		case SG_ENDRACE:
		{

			SG_MessageEndRace* r = reinterpret_cast<SG_MessageEndRace*>(recvBuf);

			int clientID = r->clientID;
			if (clientID == octr->DriverID) break;
			if (clientID < octr->DriverID) slot = clientID + 1;
			if (clientID > octr->DriverID) slot = clientID;

			
			if (squaredelay[slot] == 0) {
				squaredelay[slot] = clock();
			}

				if (((clock() - squaredelay[slot]) / CLOCKS_PER_SEC) >= 3) {

					// make this player hold SQUARE
					Gamepad* gamepad = (Gamepad*)&pBuf[(0x80096804 + (slot * 0x50)) & 0xffffff];
					gamepad->buttonsHeldCurrFrame = 0x20;
					gamepad->buttonsTapped = 0;
					gamepad->buttonsReleased = 0;
					gamepad->buttonsHeldPrevFrame = 0x20;
					
					
			}

		

			octr->raceStats[octr->numDriversEnded].slot = slot;
			memcpy(&octr->raceStats[octr->numDriversEnded].finalTime, &r->courseTime, sizeof(r->courseTime));
			memcpy(&octr->raceStats[octr->numDriversEnded].bestLap, &r->lapTime, sizeof(r->lapTime));
			octr->numDriversEnded++;

			break;
		}

	default:
		break;
	}

}

void ProcessNewMessages()
{
	ENetEvent event;
	char response = 0;

	if (clientHost == 0)
		return;

	while (enet_host_service(clientHost, &event, 0) > 0)
	{
		switch (event.type)
		{
		case ENET_EVENT_TYPE_RECEIVE:
			ProcessReceiveEvent(event.packet);
			break;

		case ENET_EVENT_TYPE_DISCONNECT:
			// command prompt reset
			system("cls");
			PrintBanner(SHOW_NAME);
			printf("\nClient: Connection Dropped (Server Full or Server Offline)...  ");

			// to go the lobby browser
			octr->CurrState = -1;
			break;

		default:
			break;
		}

		enet_packet_destroy(event.packet);
	}
}

void PrintBanner(char show_name)
{
	printf("\033[0;32m");


	printf("   ____    _    ____  __  __  _____  _____    _    _   _ \n");
	printf("  / ___|  / \\  / ___||  \\/  |/ _ \\ \\/ /_ _|  / \\  | \\ | |\n");
	printf(" | |  _  / _ \\ \\___ \\| |\\/| | | | \\  / | |  / _ \\ |  \\| |\n");
	printf(" | |_| |/ ___ \\ ___) | |  | | |_| /  \\ | | / ___ \\| |\\  |\n");
	printf("  \\____/_/   \\_\\____/|_|  |_|\\___/_/\\_\\___/_/   \\_\\_| \\_|\n");
	printf("                                                          \n");


	printf("\033[0m");

	printf(" Gasmoxian Client (press CTRL + C to quit)\n Build %s (%s)\n\n", __DATE__, __TIME__);

	if (show_name == true) printf(" Welcome to OnlineCTR Gasmoxian: %s!\n", (const char*)name);
}

void StartAnimation()
{
	char spinner_chars[] = "|/-\\";
	static int spinner_length = sizeof(spinner_chars) - 1; // exclude the NULL terminator
	static int delay = 0;
	static int i = 0;

	printf("\b%c", spinner_chars[i]); // '\b' moves the cursor back one position
	fflush(stdout); // ensure the output is printed immediately

	i = (i + 1) % spinner_length;
}

void StopAnimation()
{
	printf("\b \b\n"); // clear the spinner character when done
	fflush(stdout); // ensure the output is printed immediately
}

void DisconSELECT()
{
	int hold = *(int*)&pBuf[(0x80096804 + 0x10) & 0xffffff];

	if ((hold & 0x2000) != 0)
	{
		// Sleep() triggers server timeout
		// just in case client isnt disconnected
		StopAnimation();
		printf("Client: Disconnected (ID: DSELECT)...  ");
		enet_peer_disconnect_now(serverPeer, 0);
		serverPeer = 0;

		// to go the lobby browser
		octr->autoRetryJoinRoomIndex = -1;
		octr->CurrState = -1;
		lockengineandcharacter = false;

		return;
	}
}

void DisconAFK()
{
	StopAnimation();
	printf("Client: Kicked, reason: AFK  ");
	enet_peer_disconnect_now(serverPeer, 0);
	serverPeer = 0;

	// to go the lobby browser
	octr->CurrState = -1;
	lockengineandcharacter = false;
	return;
}
void ClearInputBuffer()
{
	int c;

	while ((c = getchar()) != '\n' && c != EOF);
}

void StatePC_Launch_EnterPID()
{
	// if client connected to DuckStation
	// before game booted, wait for boot
	if (!octr->IsBootedPS1)
		return;

	StopAnimation();
	printf("Client: Waiting to connect to a server...  ");
	octr->CurrState = LAUNCH_PICK_SERVER;
}

void printUntilPeriod(const char* str)
{
	int i = 0;

	// loop through each character in the string
	while (str[i] != '\0')
	{
		// break the loop if a period is found
		if (str[i] == '.') break;

		// print the character
		putchar(toupper(str[i]));
		i++;
	}
}

int StaticServerID = 0;
int StaticRoomID = 0;
void StatePC_Launch_PickServer()
{
	ENetAddress addr;
	static char dns_string[32] = { 0 };
	static char localServer;

	// local server
	char ip[100];
	char portStr[PORT_SIZE];
	int port;

	// quit if disconnected, but not loaded
	// back into the selection screen yet
	int gGT_levelID = *(int*)&pBuf[(0x80096b20 + 0x1a10) & 0xffffff];

	// must be in cutscene level to see country selector
	if (gGT_levelID!= 33)
		return;

	// quit if in loading screen (force-reconnect)
	int sdata_Loading_stage = *(int*)&pBuf[0x8008d0f8 & 0xffffff];

	if (sdata_Loading_stage != -1)
		return;

		if (serverPeer != 0)
		{
			//when it dc's it ends up here. Either this is causing the enet dc or the client is bugged to call this function again when it shouldn't
			printf("Disconnecting from old server...\n");
			enet_peer_disconnect_now(serverPeer, 0);
			serverPeer = 0;
		}
	

	// return now if the server selection hasn't been selected yet
	if (octr->serverLockIn1 == 0)
		return;

	// === Now Selecting Country ===
	//instead of octr, maybe do a separate variable.
	octr->boolClientBusy = 1; //this probably needs to be atomic to avoid race conditions, but I don't know if that's possible
	StaticServerID = octr->serverCountry;

	switch (octr->serverCountry)
	{
		// MEDNAFEN PERU
		case 0:
		{
			strcpy_s(dns_string, sizeof(dns_string), "mednafen-peru2.ddns.net"); 
			enet_address_set_host(&addr, dns_string);
			addr.port = 54321;

			break;
		}
		// GASMOX USA)
		case 1:
		{
			strcpy_s(dns_string, sizeof(dns_string), "medicine-kong.gl.at.ply.gg"); 
			enet_address_set_host(&addr, dns_string);
			addr.port = 63746;

			break;
		}
		// PRIVATE SERVER
		case 2:
		{
			StopAnimation();

			// IP and Port should be inside the .txt
			//IP is the first line in the txt
			//PORT is the 2nd line
			const char* filePath = ".\\data\\host\\host.txt";
			FILE* file;
			errno_t err = fopen_s(&file, filePath, "r");


			if (err != 0)
			{
				printf("\nError: Could not open private server file /data/host/host.txt %s\n", filePath);


			private_server_ip:
				ClearInputBuffer(); 
				printf("\nEnter manually server IP Address: ");

				if (fgets(ip, sizeof(ip), stdin) == NULL)
				{
					printf("\nError: Invalid IP address!\n");
					goto private_server_ip; 
				}

				
				ip[strcspn(ip, "\n")] = '\0';

		
				if (strlen(ip) == 0) strcpy_s(ip, IP_ADDRESS_SIZE, DEFAULT_IP);

			private_server_port:

				printf("Server Port (0-65535): ");

				if (fgets(portStr, sizeof(portStr), stdin) == NULL)
				{
					printf("\nError: Invalid port input!\n");
					goto private_server_port; 
				}


				portStr[strcspn(portStr, "\n")] = '\0';


				if (strlen(portStr) == 0)
				{
					printf("\nError: The port value cannot be empty!\n");
					goto private_server_port; 
				}


				port = atoi(portStr);


				if (port < 0 || port > 65535)
				{
					printf("\nError: Port value out of range!\n");
					goto private_server_port; 
				}


				enet_address_set_host(&addr, ip);
				addr.port = port;

				localServer = true;

				break;
			}


			if (fgets(ip, sizeof(ip), file) == NULL)
			{
				printf("\nError: Could not read IP from host.txt!\n");
				fclose(file);
				break;
			}

			ip[strcspn(ip, "\n")] = '\0'; 


			if (fgets(portStr, sizeof(portStr), file) == NULL)
			{
				printf("\nError: Could not read port from host.txt!\n");
				fclose(file);
				break;
			}

			portStr[strcspn(portStr, "\n")] = '\0'; 


			port = atoi(portStr);


			if (port < 0 || port > 65535)
			{
				printf("\nError: Port value out of range, try a smaller number!\n");
				fclose(file);
				break;
			}


			fclose(file);


			enet_address_set_host(&addr, ip);
			addr.port = port;

			localServer = true;

			break;
		}
} 

	StopAnimation();
	printf("Client: Attempting to connect to \"");
	if (localServer == false) printUntilPeriod(dns_string);
	else printf("%s:%d", ip, addr.port);
	printf("\" (ID: %d)...  ", StaticServerID);

	clientHost = enet_host_create(NULL /* create a client host */,
		1 /* only allow 1 outgoing connection */,
		2 /* allow up 2 channels to be used, 0 and 1 */,
		0 /* assume any amount of incoming bandwidth */,
		0 /* assume any amount of outgoing bandwidth */);

	if (clientHost == NULL)
	{
		fprintf(stderr, "Error: Failed to create an ENet client host!\n");
		exit(EXIT_FAILURE);
	}

	if (serverPeer != 0)
	{
		printf("NON-`null` enet server peer during server connection (case 2), disconnecting from old server...\n");
		lockengineandcharacter = false;
		enet_peer_disconnect_now(serverPeer, 0);
		serverPeer = 0;
	}

	serverPeer = enet_host_connect(clientHost, &addr, 2, 0);

	if (serverPeer == NULL)
	{
		fprintf(stderr, "Error: No available peers for initiating an ENet connection!\n");
		exit(EXIT_FAILURE);
	}

	//fprintf(stderr, "Trying to establish connection with server at %s:%i\n", ip, adress.port);

	ENetEvent event;

	int retryCount = 0;
	char connected = false;
	#define MAX_RETRIES 3

	// retry loop to attempt a reconnection
	while (retryCount < MAX_RETRIES && !connected)
	{
		// wait up to 3 seconds for the connection attempt to succeed
		if (enet_host_service(clientHost, &event, 3000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT)
		{
			StopAnimation();
			printf("Client: Successfully connected!  ");

			connected = true;
		}
		else
		{
			StopAnimation();
			printf("Error: Failed to connect! Attempt %d/%d...  ", retryCount + 1, MAX_RETRIES);

			if (retryCount >= MAX_RETRIES)
			{
				// to go the country select
				octr->CurrState = 1;
				octr->boolClientBusy = 0;
				return;
			}

			retryCount++;
		}
	}

	// 5 seconds
	enet_peer_timeout(serverPeer, 1000000, 1000000, 5000);

	octr->DriverID = -1;
	octr->CurrState = LAUNCH_PICK_ROOM;
	octr->boolClientBusy = 0;
}

void StatePC_Launch_Error()
{
	// do nothing
}

int connAttempt = 0;
int countFrame = 0;
void StatePC_Launch_PickRoom()
{
	countFrame++;
	if (countFrame == 60) //room not updating bug still happens if the number is not 60, i didnt tried 30 anyways
	{
		countFrame = 0;

		// send junk data,
		// this triggers server response
		CG_MessageRoom mr;
		mr.type = CG_JOINROOM;
		mr.room = 0xFF;

		sendToHostReliable(&mr, sizeof(CG_MessageRoom));
	}

	// wait for room to be chosen
	if (!octr->serverLockIn2)
	{
		connAttempt = 0;
		return;
	}

	// dont send CG_JoinRoom twice
	if (connAttempt == 1)
		return;

	connAttempt = 1;

	CG_MessageRoom mr;
	mr.type = CG_JOINROOM;
	mr.room = octr->serverRoom;
	octr->autoRetryJoinRoomIndex = -1;

	sendToHostReliable(&mr, sizeof(CG_MessageRoom));
}

void StatePC_Lobby_AssignRole()
{
	connAttempt = 0;
	countFrame = 0;
}

void StatePC_Lobby_HostTrackPick()
{
	// boolLockedInLap gets set after
	// boolLockedInLevel already sets
	if (!octr->boolLockedInLap) return;

	StopAnimation();
	printf("Client: Sending track to the server...  ");

	CG_MessageTrack mt = { 0 };
	mt.type = CG_TRACK;


	mt.trackID = octr->levelID;
	mt.lapID = octr->lapID;

	unsigned char numLaps = (mt.lapID * 2) + 1;

	int lapValues[] = { 10, 15, 20, 25, 30, 35, 40, 50, 69, 80, 90, 127 };

	if (mt.lapID >= 4 && mt.lapID <= 15) {
		numLaps = lapValues[mt.lapID - 4];
	}

	unsigned char* numLapsV = (unsigned char*)&pBuf[(0x80096b20 + 0x1d33) & 0xffffff];
	*numLapsV = numLaps;


	sendToHostReliable(&mt, sizeof(CG_MessageTrack));



	octr->CurrState = LOBBY_SPECIALPICK;
}

void StatePC_Lobby_SpecialPick() {

	if (!octr->boolLockedInSpecial) return;

	
	
	

	if (octr->special != prev_special)
	{

		StopAnimation();
		printf("Client: Sending gamemode to the server...  ");

		CG_MessageSpecial mt = { 0 };
		mt.type = CG_SPECIAL;
		mt.special = octr->special;

		prev_special = octr->special;


		
		sendToHostReliable(&mt, sizeof(CG_MessageSpecial));

		octr->CurrState = LOBBY_CHARACTER_PICK;
	}
}


int prev_characterID = -1;
int prev_boolLockedIn = -1;
int prev_enginetype = -1;
int prev_boolLockedInEngine = -1;


void StatePC_Lobby_GuestTrackWait()
{
	prev_characterID = -1;
	prev_boolLockedIn = -1;
	prev_enginetype = -1;
	prev_boolLockedInEngine = -1;
	
}

void StatePC_Lobby_CharacterPick()
{

	CG_MessageCharacter mc = { 0 };
	mc.type = CG_CHARACTER;

	// data.characterIDs[0]
	char* characterID = (char*)&pBuf[0x80086e84 & 0xffffff];
	mc.characterID = *characterID;


	mc.boolLockedIn = octr->boolLockedInCharacters[octr->DriverID];

	if (
		(prev_characterID != mc.characterID) ||
		(prev_boolLockedIn != mc.boolLockedIn)
		)
	{
		prev_characterID = mc.characterID;
		prev_boolLockedIn = mc.boolLockedIn;

		sendToHostReliable(&mc, sizeof(CG_MessageCharacter));
	}

	//this defines the default engine if enginetype is 4-5, but only for your player [0].
//needed because my code in engineaudio overwrittes the value in zdataglobal ._XD
//anyways it is better than only have the default audio

	//unsigned char ogEngine = mc.characterID;

	//int desiredengines[] = { 0, 0, 2, 1, 1, 2, 3, 3,
		//  1, 2, 3, 0, 2, 3, 0, 0 };

	//ogEngine = desiredengines[mc.characterID];
	//octr->OGengine[0] = ogEngine;

	if (mc.boolLockedIn== 0)
	{
     lockengineandcharacter = true;
	}
		if (mc.boolLockedIn == 1)
		{
			octr->CurrState = LOBBY_ENGINEPICK;
		}
}
void StatePC_Lobby_EnginePick()
{

	CG_MessageEngine mc = { 0 };
	mc.type = CG_ENGINE;



	mc.enginetype = octr->enginetype[octr->DriverID]; 
	mc.boolLockedIn = octr->boolLockedInEnginee[octr->DriverID];  

	if (
		(prev_enginetype != mc.enginetype) ||
		(prev_boolLockedInEngine != mc.boolLockedIn)
		)
		{
			prev_enginetype = mc.enginetype;
			prev_boolLockedInEngine = mc.boolLockedIn;
			sendToHostReliable(&mc, sizeof(CG_MessageEngine));
		}



		if (mc.boolLockedIn == 1)
		{
			lockengineandcharacter = false;
			octr->CurrState = LOBBY_WAIT_FOR_LOADING;
		}
}
void StatePC_Lobby_WaitForLoading()
{
	// if recv message to start loading,
	// change state to StartLoading,
	// this check happens in ProcessNewMessages
}

int boolAlreadySent_StartRace = 0;
int boolAlreadySent_EndRace = 0;

void StatePC_Lobby_StartLoading()
{
	octr->finishracetimer = 0;
	boolAlreadySent_StartRace = 0;
	boolAlreadySent_EndRace = 0;
}

void SendEverything()
{
	CG_EverythingKart cg = { 0 };
	cg.type = CG_RACEDATA;

	// === Position ===
	int hold = *(int*)&pBuf[(0x80096804 + 0x10) & 0xffffff];

	// lossless compression, bottom byte is never used,
	// cause psx renders with 3 bytes, and top byte
	// is never used due to world scale (just pure luck)

	// ignore Circle/L2
	hold &= ~(0xC0);

	// put L1/R1 into one byte
	if ((hold & 0x400) != 0) hold |= 0x40;
	if ((hold & 0x800) != 0) hold |= 0x80;

	cg.buttonHold = (unsigned char)hold;

	// === Position ===
	int psxPtr = *(int*)&pBuf[0x8009900c & 0xffffff];
	psxPtr &= 0xffffff;

	// lossless compression, bottom byte is never used,
	// cause psx renders with 3 bytes, and top byte
	// is never used due to world scale (just pure luck)
	cg.posX = (short)(*(int*)&pBuf[psxPtr + 0x2d4] / 256);
	cg.posY = (short)(*(int*)&pBuf[psxPtr + 0x2d8] / 256);
	cg.posZ = (short)(*(int*)&pBuf[psxPtr + 0x2dc] / 256);

	// === Direction Faced ===
	// driver->0x39a (direction facing)
	unsigned short angle = *(unsigned short*)&pBuf[psxPtr + 0x39a];
	angle &= 0xfff;

	unsigned char angleBit5 = angle & 0x1f;
	unsigned char angleTop8 = angle >> 5;
	cg.kartRot1 = angleBit5;
	cg.kartRot2 = angleTop8;

	char wumpa = *(unsigned char*)&pBuf[psxPtr + 0x30];
	cg.wumpa = wumpa;

	// must be read as unsigned, even though game uses signed,
	// has to do with infinite reserves when the number is negative
	unsigned short reserves = *(unsigned short*)&pBuf[psxPtr + 0x3e2];
	cg.boolReserves = (reserves > 200);

	// TO DO: No Fire Level yet

	sendToHostUnreliable(&cg, sizeof(CG_EverythingKart));

	if (octr->Shoot[0].boolNow == 1)
	{
		octr->Shoot[0].boolNow = 0;

		CG_MessageWeapon w = { 0 };

		w.type = CG_WEAPON;
		w.weapon = octr->Shoot[0].Weapon;
		w.juiced = octr->Shoot[0].boolJuiced;
		w.flags = octr->Shoot[0].flags;

		sendToHostReliable(&w, sizeof(CG_MessageWeapon));
	}
}

void StatePC_Game_WaitForRace()
{
	int gGT_gameMode1 = *(int*)&pBuf[(0x80096b20 + 0x0) & 0xffffff];
	
	if (
		// only send once
		(!boolAlreadySent_StartRace) &&

		// after camera fly-in is done
		((gGT_gameMode1 & 0x40) == 0)
		)
	{
		StopAnimation();
		printf("Client: Gasmoxian race in progress...  ");
		boolAlreadySent_StartRace = 1;

		CG_Header cg = { 0 };
		cg.type = CG_STARTRACE;

		sendToHostReliable(&cg, sizeof(CG_Header));
	}

	SendEverything();
}
int requiredPlayers = 0;
int disconnectedPlayers = 0;
int activePlayers = 0;

void StatePC_Game_StartRace()
{
	SendEverything();


	int gGT_levelID =
		*(int*)&pBuf[(0x80096b20 + 0x1a10) & 0xffffff];

	//demo camera mode
	if (octr->special == 8) {
		if (gGT_levelID < 18)
			*(short*)&pBuf[(0x80098028) & 0xffffff] = 0x20;
	}





	//stop orb/clock spam
// this one was very problematic to make it possible so i dont want to touch this NEVER again
	if (!sendwarpclock && warpclockdelay == 0) {

		if (octr->warpclock != prev_warpclock) {
			CG_MessageWarpclock w = { 0 };
			w.type = CG_WARPCLOCK;
			w.warpclock = octr->warpclock;

			sendToHostReliable(&w, sizeof(CG_MessageWarpclock));

			sendwarpclock = 1;


			prev_warpclock = octr->warpclock;


			warpclockdelay = clock();
		}
	}

	//set banned time for orb/clock
	if (sendwarpclock) {
		if (((clock() - warpclockdelay) / CLOCKS_PER_SEC) >= 80) {

			octr->warpclock = 0;


			if (octr->warpclock != prev_warpclock) {
				CG_MessageWarpclock w = { 0 };
				w.type = CG_WARPCLOCK;
				w.warpclock = octr->warpclock;


				sendToHostReliable(&w, sizeof(CG_MessageWarpclock));


				prev_warpclock = octr->warpclock;
			}


			sendwarpclock = 0;
			warpclockdelay = 0;
		}
	}












	//calculate disconnected players

	disconnectedPlayers = 0;
	activePlayers = 0;
	for (int i = 0; i < octr->NumDrivers; i++) {
		if (octr->nameBuffer[i][0] == 0) {
			disconnectedPlayers++;
		}
		else {
			activePlayers++;
		}
	}

	requiredPlayers = 0;

	//conditions to end the race
	if (activePlayers >= 4 && !extralaps) {
		requiredPlayers = 3;
	}
	else if ( activePlayers >= 4 && extralaps)
	{
		requiredPlayers = (activePlayers >= 5 ) ? 4 : 3;
	}
	if (activePlayers == 3) {
		requiredPlayers = 2;
	}
	if (activePlayers == 2) {
		requiredPlayers = 1;
	}
	if (activePlayers == 1) {
		requiredPlayers = 0;
	}

	//if not 1 player race then set 30 seconds
	if (octr->numDriversEnded == requiredPlayers && requiredPlayers != 0 && prev_finishtimer != 30) {

		octr->finishracetimer = (extralaps) ? 60 : 30;
		prev_finishtimer = 30;

	}

	//send the timer (visual) to the server
	if (octr->finishracetimer > 0 && !alreadysended) {
		CG_MessageFinishTimer mc = { 0 };
		mc.type = CG_FINISHTIMER;
		mc.finishracetimer = octr->finishracetimer;
		sendToHostReliable(&mc, sizeof(CG_MessageFinishTimer));
		alreadysended = true;
	}





}

//imo all includes should go at the top

void StatePC_Game_EndRace()
{

	if (!boolAlreadySent_EndRace)
	{
		boolAlreadySent_EndRace = 1;

		int psxPtr = *(int*)&pBuf[0x8009900c & 0xffffff];
		psxPtr &= 0xffffff;

		CG_MessageEndRace cg = { 0 };
		cg.type = CG_ENDRACE;


		int courseTime = *(int*)&pBuf[psxPtr + DRIVER_COURSE_OFFSET];
		int bestLapTime = *(int*)&pBuf[psxPtr + DRIVER_BESTLAP_OFFSET];

		memcpy(&cg.courseTime, &courseTime, sizeof(cg.courseTime));
		memcpy(&cg.lapTime, &bestLapTime, sizeof(cg.courseTime));

		sendToHostReliable(&cg, sizeof(CG_MessageEndRace));




		octr->raceStats[octr->numDriversEnded].slot = 0;
		octr->raceStats[octr->numDriversEnded].finalTime = courseTime;
		octr->raceStats[octr->numDriversEnded].bestLap = bestLapTime;
		octr->numDriversEnded++;


	}
	if (boolAlreadySent_EndRace) {
		// if all players finished the race then show the timer


		//1 player race // all players finished
		if (octr->numDriversEnded == activePlayers && prev_finishtimer != 3 && prev_finishtimer != 6) {

			octr->finishracetimer = (activePlayers == 1) ? 6 : 3;
			prev_finishtimer = (activePlayers == 1) ? 6 : 3;
			alreadysended = true;
		}


		if (octr->finishracetimer > 0 && alreadysended && prev_finishtimer == 3	 || alreadysended && prev_finishtimer == 6) {
			CG_MessageFinishTimer mc = { 0 };
			mc.type = CG_FINISHTIMER;
			mc.finishracetimer = octr->finishracetimer;
			sendToHostReliable(&mc, sizeof(CG_MessageFinishTimer));
			alreadysended = false;
		}
	}

	int numDead = 0;

	for (int i = 0; i < octr->NumDrivers; i++)
	{
		if (octr->nameBuffer[i][0] == 0)
			numDead++; //what is this used for? //why are you asking ?? //btw even if this is not used in the code it was helpfully as a template, so ty random dev
	}
}

void (*ClientState[]) () = {
	StatePC_Launch_EnterPID,		// 0
	StatePC_Launch_PickServer,		// 1
	StatePC_Launch_PickRoom,		// 2
	StatePC_Launch_Error,			// 3
	StatePC_Lobby_AssignRole,		// 4
	StatePC_Lobby_HostTrackPick,	// 5
	StatePC_Lobby_SpecialPick,	// 6
	StatePC_Lobby_GuestTrackWait,	// 7
    StatePC_Lobby_CharacterPick,	// 8
	 StatePC_Lobby_EnginePick,	// 8
	StatePC_Lobby_WaitForLoading,	// 9
	StatePC_Lobby_StartLoading,		// 10
	StatePC_Game_WaitForRace,		// 11
	StatePC_Game_StartRace,			// 12
	StatePC_Game_EndRace			// 13
};

#define NUM_PROHIBITED_NAMES 33

// ban bad names
const unsigned char* prohibitedNames[NUM_PROHIBITED_NAMES] = {
	(unsigned char*)"NIGGA", (unsigned char*)"NIGGER", (unsigned char*)"NIGGO", (unsigned char*)"HITLER",
	(unsigned char*)"DICK", (unsigned char*)"PUSSY", (unsigned char*)"GAY", (unsigned char*)"FAGGOT",
	(unsigned char*)"NIGA", (unsigned char*)"NIGER", (unsigned char*)"PORN", (unsigned char*)"ASS",
	(unsigned char*)"BASTARD", (unsigned char*)"BITCH", (unsigned char*)"TITS", (unsigned char*)"TITY",
	(unsigned char*)"FUCK", (unsigned char*)"PENIS", (unsigned char*)"ANAL", (unsigned char*)"SEX",
	(unsigned char*)"BALLS", (unsigned char*)"PISS", (unsigned char*)"ORGASM", (unsigned char*)"MASTURBATE",
	(unsigned char*)"CUM", (unsigned char*)"PUSSIES", (unsigned char*)"BONER", (unsigned char*)"RETARD",
	(unsigned char*)"PUTA", (unsigned char*)"VAGINA", (unsigned char*)"FUCK", (unsigned char*)"PENE", (unsigned char*)"REDHOT"
};

void toLowerCase(unsigned char* str) {
	for (int i = 0; str[i]; i++) {
		str[i] = (unsigned char)tolower(str[i]); 
	}
}

int containsProhibitedNames(const unsigned char* str) {
	unsigned char lowerStr[NAME_LEN + 1];

	
	strncpy_s((char*)lowerStr, sizeof(lowerStr), (const char*)str, NAME_LEN);
	lowerStr[NAME_LEN] = '\0'; 
	toLowerCase(lowerStr);

	for (int i = 0; i < NUM_PROHIBITED_NAMES; i++) {
		unsigned char lowerProhibited[NAME_LEN + 1];

		
		strncpy_s((char*)lowerProhibited, sizeof(lowerProhibited), (const char*)prohibitedNames[i], NAME_LEN);
		lowerProhibited[NAME_LEN] = '\0'; 
		toLowerCase(lowerProhibited);

		
		if (strstr((const char*)lowerStr, (const char*)lowerProhibited) != NULL) {
			return 1; 
		}
	}
	return 0; 
}

// anti afk
void afktimer() {
	if (!lockengineandcharacter)
	{
		timeStart = 0;
	}

	if (lockengineandcharacter) {  

		if (timeStart == 0) {  
			timeStart = clock();  
		}

		
		if (((clock() - timeStart) / CLOCKS_PER_SEC) >= 80) {

			
			if (!lockengineandcharacter) {
				timeStart = 0;
				return;  
			}
			else
			{
				DisconAFK();  
				timeStart = 0;
			}


		}


	}
}
// for EnumProcessModules
#pragma comment(lib, "psapi.lib")

char* progName;

int main(int argc, char* argv[])
{
	progName = argv[0];
	HWND console = GetConsoleWindow();
	RECT r;
	GetWindowRect(console, &r); // stores the console's current dimensions
	MoveWindow(console, r.left, r.top, 800, 480 + 35, TRUE);
	SetConsoleOutputCP(CP_UTF8); // force the output to be unicode (UTF-8)

	if (argc == 2)
	{
		memcpy(name, argv[1], strlen(argv[1]));
	}
	else
	{
		PrintBanner(DONT_SHOW_NAME);
		// ask for the users online identification
		printf("Enter Your Username (11) = ");
		scanf_s("%s", name, (int)sizeof(name));
	}

	if (containsProhibitedNames(name)) {
		printf("\nyour username contains banned words, using default instead: \"gasmoxian\"\n");
		strcpy_s((char*)name, NAME_LEN, "gasmoxian");
	}

	name[NAME_LEN] = 0; // truncate the name (0 based)

	// show a welcome message
	system("cls");
	PrintBanner(SHOW_NAME);
	printf("\n");

	////this call is only good if we're certain duckstation is *running* (and pine is enabled,
	//// but that needs to be done manually by the user or by the INI config).
	//while (!defMemInit()) //returns true if socket is good
	//{
	//	std::this_thread::sleep_for(std::chrono::milliseconds(3000));
	//}

	//pBuf = ps1mem(0);
	//octr = pBuf.at<OnlineCTR>(0x8000C000);

	//// initialize enet
	//if (enet_initialize() != 0)
	//{
	//	fprintf(stderr, "Error: Failed to initialize ENet!\n");

	//	return 1;
	//}

	//atexit(enet_deinitialize);
	//printf("Client: Waiting for the OnlineCTR binary to load...  ");

	//int sleepCount = 5000;
	//int enableDeferredGPU = 1;

	int numDuckInstances = 0;
	const char* duckTemplate = "duckstation";
	int duckPID = -1;

	// copy from
	// https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
	DWORD aProcesses[1024], cbNeeded, cProcesses;
	EnumProcesses(aProcesses, sizeof(aProcesses), &cbNeeded);
	cProcesses = cbNeeded / sizeof(DWORD);

	for (int i = 0; i < cProcesses; i++)
	{
		DWORD processID = aProcesses[i];

		if (processID != 0)
		{
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processID);

			if (NULL != hProcess)
			{
				HMODULE hMod;
				DWORD cbNeeded;

				if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded))
				{
					char szProcessName[MAX_PATH];
					GetModuleBaseNameA(hProcess, hMod, szProcessName, sizeof(szProcessName) / sizeof(TCHAR));

					char* procName = (char*)&szProcessName[0];

					if (
						(*(int*)&procName[0] == *(int*)&duckTemplate[0]) &&
						(*(int*)&procName[4] == *(int*)&duckTemplate[4])
						)
					{
						numDuckInstances++;
						duckPID = processID;
					}
				}
			}
		}
	}

	if (numDuckInstances == 0)
	{
		printf("Error: DuckStation is not running!\n\n");
		system("pause");
		exit(0);
	}
	else printf("Client: DuckStation detected\n");

	char pidStr[16];

	if (numDuckInstances > 1)
	{
		printf("Warning: Multiple DuckStations detected\n");
		printf("Please enter the PID manually\n\n");

		printf("Input.: DuckStation PID: ");
		scanf_s("%s", pidStr, (int)sizeof(pidStr));
	}
	else
	{
		sprintf_s(pidStr, 100, "%d", duckPID);
	}

	char duckName[100];
	sprintf_s(duckName, 100, "duckstation_%s", pidStr);

	TCHAR duckNameT[100];
	swprintf(duckNameT, 100, L"%hs", duckName);

	// 8 MB RAM
	const unsigned int size = 0x800000;
	HANDLE hFile = OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, duckNameT);
	pBuf = (char*)MapViewOfFile(hFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, size);

	if (pBuf == 0)
	{
		printf("Error: Failed to open DuckStation!\n\n");
		system("pause");
		//system("cls");
		//char* newargv[4];
		//newargv[0] = argv[0];
		//if (argc > 1)
		//	newargv[1] = argv[1];
		//else
		//	newargv[1] = NULL;
		//if (argc > 2)
		//	newargv[2] = argv[2];
		//else
		//	newargv[2] = NULL;
		//newargv[3] = NULL;

		//execv(argv[0], newargv);
		exit(EXIT_FAILURE);
	}

	octr = (OnlineCTR*)&pBuf[0x8000C000 & 0xffffff];
	octr->autoRetryJoinRoomIndex = -1;

	// initialize enet
	if (enet_initialize() != 0)
	{
		fprintf(stderr, "Error: Failed to initialize ENet!\n");

		return 1;
	}

	atexit(enet_deinitialize);
	printf("Client: Waiting for the Gasmoxian binary to load...  ");

	while (1)
	{
		// To do: Check for PS1 system clock tick then run the client update

		//technechally windowsClientSync just needs to *change* every frame.
		//perhaps instead of reading, keep a local counter, increment that, and then
		//write it (without needing a blocking read first).
		octr->windowsClientSync++;

		//afk timer only in character and engine selection
		if (octr->CurrState >= LOBBY_CHARACTER_PICK && octr->CurrState < LOBBY_WAIT_FOR_LOADING) {
			afktimer();
		}

		// should rename to room selection
		if (octr->CurrState >= LAUNCH_PICK_ROOM)

			DisconSELECT();
			StartAnimation();

			//send data
			if (octr->CurrState >= 0)
				ClientState[octr->CurrState]();

			// now check for new RECV message
			ProcessNewMessages();

			// Wait for PSX to have P1 data,
			// which is set at octr->sleepControl
			void FrameStall(); FrameStall();

	}

	printf("\n");
	system("pause");
}

#ifdef __WINDOWS__
void usleep(__int64 usec)
{
	HANDLE timer;
	LARGE_INTEGER ft = { 0 };

	ft.QuadPart = -(10 * usec); // Convert to 100 nanosecond interval, negative value indicates relative time

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif



int gGT_timer = 0;
void FrameStall()
{
	// wait for next frame
	while (octr->readyToSend == 0)
	{
		usleep(1);
	}

	octr->readyToSend = 0;
}
