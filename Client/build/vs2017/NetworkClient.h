#pragma once
#define NOMINMAX
#include <WinSock2.h>
#include <iostream>
#include "Messages.h"
#include <thread>
#include <queue>
#include <mutex>

// The IP address of the server to connect to
//#define SERVERIP serverIP_.c_str()

// The TCP port number on the server to connect to
#define SERVERPORT_TCP 5555

// The UDP port number on the server to connect to
#define SERVERPORT_UDP 4444

#define TICKRATE 8


//Message header format: 
// +--------+--------+--------+
// |      Length     |  Type  |
// +--------+--------+--------+

//Size of the header overall
#define HeaderSize sizeof(uint16_t) + sizeof(uint8_t)
//Size of the Length field in the header
#define HeaderLenFieldSize sizeof(uint16_t)
//Size of Type field in header
#define HeaderTypeFieldSize sizeof(uint8_t)

typedef std::chrono::high_resolution_clock ClientClock;

class SceneApp;

enum class ReadingWriting { READING, WRITING, NONE };

class NetworkClient {
public:
	NetworkClient() {};
	~NetworkClient();

	void StartWinSock();
	void StartConnection(SceneApp* scene);
	void ConnectionLoopTCP();
	void ConnectionLoopUDP();
	void UpdateTime();
	void CreateClientInfoMessage();
	void CreateChatMessage(const char* chatMsg, int playerID);
	void SendInput(InputUpdateMessage& inputs);
	int GetTime() { return time_; }
private:
	void die(const char* message);
	void ConnectTCP();
	void ConnectUDP();
	void CleanupSocket();
	bool ReadTCP();
	bool ReadUDP();
	bool WriteTCP();
	bool WriteUDP(uint16_t& msgLen);
	void SendPingMessage();
	void SendTimeReqMessage();
	void SendInputMessage();
	void AddMessage(uint16_t& msgLen, MessageType& msgType, std::vector<uint8_t>& msgData);
	void SendMessages();
	void SyncTimeSend();
	void SyncTimeReceive(TimeRequestMessage& msg);
	void HandleMessage(uint16_t length, const char* buffer);

	std::string serverIP_;

	ClientClock::time_point timeStart_ = ClientClock::now();
	uint32_t time_ = 0;
	int serverTimeDelta_ = 0;
	int timeSynced_ = 0;
	int latency_ = INT_MAX;

	SceneApp* scene_;
	bool running_ = true;

	std::thread* connectionThreadTCP_;
	std::thread* connectionThreadUDP_;

	//Socket for TCP
	SOCKET socketTCP_;
	int readCountTCP_ = 0;
	char readBufferTCP_[500]; //TODO: determine biggest buffer needed
	int writeCountTCP_ = 0;
	char writeBufferTCP_[65535];
	bool writeableTCP_ = false;
	std::queue<std::string> clientMsgsTCP_;
	std::mutex clientMsgsMutexTCP_;

	SOCKET socketUDP_;
	std::mutex mutexUDP_;
	char readBufferUDP_[500];
	char writeBufferUDP_[500];
	bool writeableUDP_ = false;
	bool sendPlayerInputUDP_ = false;
	bool sendTimeRequestUDP_ = false;
	int prevInputSendTime_ = 0;
	int prevServerPlayerValTime = 0;

	InputUpdateMessage playerInputs_;
	std::mutex inputsMutex_;
 
	//Structure to hold the result from WSAEnumNetworkEvents
	WSANETWORKEVENTS networkEventsTCP_;
	WSANETWORKEVENTS networkEventsUDP_;

	//Handles to event object
	WSAEVENT eventTCP_;
	WSAEVENT eventUDP_;

	
};

