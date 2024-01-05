#pragma once
#define NOMINMAX
#include <WinSock2.h>
#include <iostream>
#include "Messages.h"
#include "Connection.h"
#include <thread>
#include <queue>
#include <mutex>
#include <utility>
#include <unordered_map>
#include <map>

// The TCP port number on the server to connect to
#define SERVERPORT_TCP 5555

// The UDP port number on the server to connect to
#define SERVERPORT_UDP 4444

#define TICKRATE 8

typedef std::chrono::high_resolution_clock ServerClock;

class SceneApp;

enum class ReadingWriting { READING, WRITING, NONE };

class NetworkServer {
public:
	NetworkServer() {};
	~NetworkServer();

	void StartWinSock();
	void StartConnection(SceneApp* scene);
	void ConnectionLoopTCP();
	void ConnectionLoopUDP();
	void UpdateTime();
	uint32_t GetTime() { return time_; }
	//void CreateChatMessage(const char* chatMsg, int playerID);
	void HandleMessage(int playerID, uint16_t length, const char* buffer);
private:
	void DisplayLocalIP();
	void StartListeningTCP();
	void StartListeningUDP();
	void RestartListeningTCP();
	void RestartListeningUDP();
	void die(const char* message);
	void CleanupSocket(int index);
	bool ReadUDP();
	bool WriteUDP(sockaddr_in* address, uint16_t& length);
	void SendTimeReplyMessage(sockaddr_in* address, TimeRequestMessage& msg);
	bool SendUDP();
	uint16_t CreatePingMessage();
	uint16_t CreatePlayersUpdateMessage();

	uint32_t time_;
	ServerClock::time_point timeStart_ = ServerClock::now();

	SceneApp* scene_;

	std::thread* connectionThreadTCP_;
	std::thread* connectionThreadUDP_;

	//Structure to hold the result from WSAEnumNetworkEvents
	WSANETWORKEVENTS networkEventsTCP_;
	WSANETWORKEVENTS networkEventsUDP_;

	SOCKET ListenSocket_;
	std::vector<Connection*> connections_;
	std::unordered_map<int, Connection*> playerIDtoConnection_;

	SOCKET socketUDP_;
	//std::vector<sockaddr_in> addressesUDP_;
	std::map<sockaddr_in, int> addressUDPtoID_;

	//WSAEVENT ListenEvent_;
	std::vector<WSAEVENT> eventsTCP_ = std::vector<WSAEVENT>(1);
	WSAEVENT eventUDP_;

	char readBufferUDP_[500];
	char writeBufferUDP_[500];
	bool writeableUDP_ = false;

	int server_tick_ = 1000 / TICKRATE;
};