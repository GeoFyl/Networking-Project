#include "NetworkClient.h"
#include "scene_app.h"
#include <iostream>
#include <fstream>
#include "msgpack.hpp"
#include <chrono>
//#include <numeric>
#pragma comment(lib, "ws2_32.lib")

NetworkClient::~NetworkClient() {
	running_ = false;
	WSASetEvent(eventTCP_);
	WSASetEvent(eventUDP_);
	connectionThreadTCP_->join();
	connectionThreadUDP_->join();
}

void NetworkClient::StartConnection(SceneApp* scene) {
	scene_ = scene;

	StartWinSock();

	std::ifstream serverIpFile;
	serverIpFile.open("Server IP.txt");
	std::getline(serverIpFile, serverIP_);
	serverIpFile.close();

	ConnectTCP();
	ConnectUDP();

	connectionThreadTCP_ = new std::thread(&NetworkClient::ConnectionLoopTCP, this);
	connectionThreadUDP_ = new std::thread(&NetworkClient::ConnectionLoopUDP, this);
}

void NetworkClient::ConnectTCP() {
	// Create a TCP socket that we'll connect to the server
	socketTCP_ = socket(AF_INET, SOCK_STREAM, 0);
	if (socketTCP_ == INVALID_SOCKET) {
		die("socket failed");
	}

	// Create a new event for checking TCP socket activity
	eventTCP_ = WSACreateEvent();
	if (eventTCP_ == WSA_INVALID_EVENT) {
		die("Server event creation failed");
	}
	//Assosciate this event with the socket types we're interested in
	WSAEventSelect(socketTCP_, eventTCP_, FD_CONNECT | FD_CLOSE | FD_READ | FD_WRITE);

	// Fill out a sockaddr_in structure with the address that we want to connect to.
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVERPORT_TCP); // htons converts the port number to network byte order (big-endian).
	addr.sin_addr.s_addr = inet_addr(serverIP_.c_str());

	printf("IP address to connect to: %s\n", inet_ntoa(addr.sin_addr)); // inet_ntoa formats an IP address as a string.
	printf("Port number to connect to: %d\n\n", ntohs(addr.sin_port)); // ntohs does the opposite of htons.

	// Connect the socket to the server.
	if (connect(socketTCP_, (const sockaddr*)&addr, sizeof addr) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		die("TCP connect failed");
	}
	DWORD returnVal = WSAWaitForMultipleEvents(1, &eventTCP_, false, WSA_INFINITE, false);
	// Since socket is non blocking, must now check if connection actually succeeded
	if (WSAEnumNetworkEvents(socketTCP_, eventTCP_, &networkEventsTCP_) == SOCKET_ERROR) {
		die("Retrieving event information failed");
	}
	if (networkEventsTCP_.lNetworkEvents & FD_CONNECT) {
		printf("TCP connected to server\n");
	}
	else if (networkEventsTCP_.iErrorCode[FD_CONNECT_BIT] != 0) {
		printf("error code: %d", networkEventsTCP_.iErrorCode[FD_CONNECT_BIT]);
		fprintf(stderr, "\nError: (WSAGetLastError() = %d)", WSAGetLastError());
		die("Failed to actually connect.");
	}
	if (networkEventsTCP_.lNetworkEvents & FD_WRITE) {
		writeableTCP_ = true;
	}
}

void NetworkClient::ConnectUDP() {
	// Create a UDP socket that we'll connect to the server
	socketUDP_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketUDP_ == INVALID_SOCKET) {
		die("socket failed");
	}

	// Create a new event for checking socket activity
	eventUDP_ = WSACreateEvent();
	if (eventUDP_ == WSA_INVALID_EVENT) {
		die("Server event creation failed");
	}
	//Assosciate this event with the socket types we're interested in
	WSAEventSelect(socketUDP_, eventUDP_, FD_READ | FD_WRITE);

	// Fill out a sockaddr_in structure with the address that we want to connect to.
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVERPORT_UDP); // htons converts the port number to network byte order (big-endian).
	addr.sin_addr.s_addr = inet_addr(serverIP_.c_str());

	printf("IP address to connect to: %s\n", inet_ntoa(addr.sin_addr)); // inet_ntoa formats an IP address as a string.
	printf("Port number to connect to: %d\n\n", ntohs(addr.sin_port)); // ntohs does the opposite of htons.

	// Connect the socket to the server (this sets the default destination address)
	if (connect(socketUDP_, (const sockaddr*)&addr, sizeof addr) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		die("UDP connect failed");
	}
	// Since socket is non blocking, must now check if connection actually succeeded
	if (WSAEnumNetworkEvents(socketUDP_, eventUDP_, &networkEventsUDP_) == SOCKET_ERROR) {
		die("Retrieving event information failed");
	}
	if (networkEventsUDP_.lNetworkEvents & FD_WRITE) {
		writeableUDP_ = true;
	}

	printf("UDP socket ready\n");
}

void NetworkClient::ConnectionLoopTCP() {
	while (running_) {
		if (eventTCP_) {
			//Wait forever till an event is signalled - efficient since this is run on a separate thread.
			DWORD returnVal = WSAWaitForMultipleEvents(1, &eventTCP_, false, WSA_INFINITE, false);

			if ((returnVal != WSA_WAIT_TIMEOUT) && (returnVal != WSA_WAIT_FAILED)) {
				int eventIndex = returnVal - WSA_WAIT_EVENT_0; //In practice, eventIndex will equal returnVal, but this is here for compatability

				//Message received from server
				if (eventIndex == 0) {
					if (WSAEnumNetworkEvents(socketTCP_, eventTCP_, &networkEventsTCP_) == SOCKET_ERROR) {
						die("Retrieving event information failed");
					}
					if (networkEventsTCP_.lNetworkEvents & FD_CLOSE)
					{
						//Ignore the error if the server just force quit
						if (networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] != 0 && networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] != 10053)
						{
							printf("FD_CLOSE with error %d\n", networkEventsTCP_.iErrorCode[FD_CLOSE_BIT]);
						}
						else if (networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] == 10053) {
							printf("Server closed connection.\n");
						}
						CleanupSocket();
					}
					if (networkEventsTCP_.lNetworkEvents & FD_READ) {
						//Reading message from server
						ReadTCP();
					}
					if (networkEventsTCP_.lNetworkEvents & FD_WRITE) {
						writeableTCP_ = true;
					
					}
					if (writeableTCP_) {
						SendMessages();
					}
				}
			}
			else if (returnVal == WSA_WAIT_TIMEOUT) {
				//All good, we just have no activity
			}
			else if (returnVal == WSA_WAIT_FAILED) {
				die("WSAWaitForMultipleEvents failed!");
			}
		}
		
	}
}

void NetworkClient::ConnectionLoopUDP() {

	while (running_) {
		DWORD returnVal = WSAWaitForMultipleEvents(1, &eventUDP_, false, WSA_INFINITE, false);

		if (returnVal != WSA_WAIT_FAILED) {

			if (WSAEnumNetworkEvents(socketUDP_, eventUDP_, &networkEventsUDP_) == SOCKET_ERROR) {
				die("Retrieving UDP event information failed");
			}
			if (networkEventsUDP_.lNetworkEvents & FD_WRITE) {
				writeableUDP_ = true;
			}
			if (networkEventsUDP_.lNetworkEvents & FD_READ) {
				ReadUDP();
			}
			if (writeableUDP_) {
				//printf("sp: %d, t: %d, pt: %d\n", sendPlayerInputUDP_, time_, prevInputSendTime_);
				if (sendPlayerInputUDP_){ 
					if (time_ > prevInputSendTime_ + 1) { //Limit input sends to one every two milliseconds
						prevInputSendTime_ = time_;
						SendInputMessage();
						mutexUDP_.lock();
						sendPlayerInputUDP_ = false;
						mutexUDP_.unlock();
					}
				}
			}

		}
		else if (returnVal == WSA_WAIT_FAILED) {
			die("UDP WSAWaitForMultipleEvents failed!");
		}
	}
}

void NetworkClient::UpdateTime() {
	time_ = std::chrono::duration_cast<std::chrono::milliseconds>(ClientClock::now() - timeStart_).count() + serverTimeDelta_;
	//printf("time: %d\n", time_);
}

void NetworkClient::CreateClientInfoMessage() {
	//Get address of the UDP socket
	sockaddr_in addr;
	int addrLen = sizeof(addr);
	getsockname(socketUDP_, (sockaddr*)&addr, &addrLen);

	printf("UDP port: %d\n", addr.sin_port);

	//Create message
	ClientInfoMessage msg;
	MessageType msgType = MessageType::CLIENTINFO;
	msg.portUDP = addr.sin_port;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void NetworkClient::CreateChatMessage(const char* chatMsg, int playerID) {
	//Create message
	ChatMessage msg;
	MessageType msgType = MessageType::CHAT;
	msg.playerID = playerID;
	msg.chatStr = chatMsg;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void NetworkClient::AddMessage(uint16_t &msgLen, MessageType &msgType, std::vector<uint8_t> &msgData) {
	memcpy(writeBufferTCP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferTCP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);
	memcpy(writeBufferTCP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	std::string msgStr(writeBufferTCP_, msgLen);

	clientMsgsMutexTCP_.lock();
	clientMsgsTCP_.push(msgStr);
	clientMsgsMutexTCP_.unlock();

	WSASetEvent(eventTCP_); //Signal that there is a new message to be sent
}

bool NetworkClient::ReadTCP() {
	// Receive the length of the incoming message if we haven't already
	if (readCountTCP_ < HeaderLenFieldSize) {
		int count = recv(socketTCP_, readBufferTCP_ + readCountTCP_, HeaderLenFieldSize - readCountTCP_, 0); 
		if (count == SOCKET_ERROR) {
			printf("TCP Receive failed\n");
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				return false;
			}
			else die("TCP connection closed or broken");
		}
		printf("TCP Received %d bytes\n", count);
		readCountTCP_ += count;
		if (count < HeaderLenFieldSize) return false;
	}

	// Get length of message
	uint16_t msgLength = (uint16_t)readBufferTCP_[0];

	int count = recv(socketTCP_, readBufferTCP_ + readCountTCP_, msgLength - readCountTCP_, 0);
	if (count == SOCKET_ERROR) {
		printf("TCP Receive failed\n");
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			return false;
		}
		else die("TCP Connection closed or broken");
	}
	printf("TCP Received %d bytes\n", count);
	readCountTCP_ += count;

	if (readCountTCP_ < msgLength) {
		// ... but we've not received a complete message yet.
		// So we can't do anything until we receive some more.
		return false;
	}

	// We've got a complete message.
	printf("TCP Received message from the server: '");
	fwrite(readBufferTCP_, 1, msgLength, stdout);
	printf("'\n\n");

	HandleMessage(msgLength, readBufferTCP_);

	// Clear the buffer, ready for the next message.
	readCountTCP_ = 0;
	return true;
}

bool NetworkClient::ReadUDP() {
	int count = recv(socketUDP_, readBufferUDP_, sizeof(readBufferUDP_), 0);
	if (count == SOCKET_ERROR) {
		printf("Receive failed\n");
		if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAENETRESET || WSAGetLastError() == WSAEMSGSIZE) {
			return true;
		}
		else {
			printf("UDP socket closed or broken\n");
			return false;
		}
	}

	uint16_t msgLength = (uint16_t)readBufferUDP_[0];
	if (count == msgLength) {

		HandleMessage(msgLength, readBufferUDP_);
	}
	else {
		printf("UDP datagram wrong length - discarding.\n");
	}
}

bool NetworkClient::WriteTCP() {
	// Get first message in the queue
	clientMsgsMutexTCP_.lock();
	if (clientMsgsTCP_.empty()) {
		clientMsgsMutexTCP_.unlock();
		return false;
	}
	std::string msg = clientMsgsTCP_.front();
	clientMsgsMutexTCP_.unlock();

	// Try to send as much as is left to send
	int msgLength = msg.length();
	int messageLeft = (msgLength)-writeCountTCP_;
	int count = send(socketTCP_, msg.data() + writeCountTCP_, messageLeft, 0);
	if (count == SOCKET_ERROR)
	{
		printf("Send failed\n");
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			writeableTCP_ = false;
			return false;
		}
		else die("Connection closed or broken");
	}

	// We've written some data to the socket
	printf("Sent %d bytes to the server.\n", count);
	writeCountTCP_ += count;

	if (writeCountTCP_ < msgLength) {
		printf("Not all sent\n");
		// but not written the whole message. Stop writing for now.
		writeableTCP_ = false;
		return false;
	}

	// Written a complete message.
	printf("Sent message to the server: '");
	fwrite(msg.data(), 1, msgLength, stdout);
	printf("'\n\n");

	// Remove message from queue
	clientMsgsMutexTCP_.lock();
	clientMsgsTCP_.pop();
	clientMsgsMutexTCP_.unlock();
	writeCountTCP_ = 0;
	return true;
}

bool NetworkClient::WriteUDP(uint16_t& msgLen) {
	int count = send(socketUDP_, writeBufferUDP_, msgLen, 0);
	if (count == SOCKET_ERROR) {
		if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAENOBUFS) {
			writeableUDP_ = false;
			return false;
		}
		else {
			die("UDP error writing"); //TODO: restart udp listening
		}
	}
	return true;
}

void NetworkClient::SendPingMessage() {
	MessageType msgType = MessageType::PING;
	uint16_t msgLen = HeaderSize;

	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);

	if (writeableUDP_) WriteUDP(msgLen);
}

void NetworkClient::SendTimeReqMessage() {
	//Create message
	TimeRequestMessage msg;
	MessageType msgType = MessageType::TIMEREQUEST;
	msg.clientTime = time_;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);
	memcpy(writeBufferUDP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	if (writeableUDP_) {
		WriteUDP(msgLen);
	}
}

void NetworkClient::SendInput(InputUpdateMessage& input) {

	inputsMutex_.lock();
	playerInputs_ = input;
	inputsMutex_.unlock();
	mutexUDP_.lock();
	sendPlayerInputUDP_ = true;
	mutexUDP_.unlock();

	WSASetEvent(eventUDP_); //Signal that there is a new message to be sent
}

void NetworkClient::SendInputMessage() {
	//Create message
	InputUpdateMessage msg;
	MessageType msgType = MessageType::INPUTUPDATE;
	inputsMutex_.lock();
	msg = playerInputs_;
	inputsMutex_.unlock();
	msg.time = time_;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);
	memcpy(writeBufferUDP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	if (writeableUDP_) WriteUDP(msgLen);
}

void NetworkClient::SendMessages()
{
	// Keep sending messages from the queue until something stops us.
	while (true) {
		if (!WriteTCP()) return;
	}
}

void NetworkClient::SyncTimeSend() { //TODO: add a spawn feature - after joining game, wont be able to spawn till time is synced.
	while (timeSynced_ < 10) {
		for(int i = 0; i < 15; i++) {
			SendTimeReqMessage();
			std::this_thread::sleep_for(std::chrono::milliseconds(300));
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

void NetworkClient::SyncTimeReceive(TimeRequestMessage& msg) {
	printf("Syncing...\n");
		int latency = (time_ - msg.clientTime) / 2;
		//printf("latency: %d\n", latency);
		if (latency < latency_) {
			latency_ = latency;
			// Sync the time
			timeStart_ = ClientClock::now();
			serverTimeDelta_ = msg.serverTime + latency_;
		}

		timeSynced_++;

		printf("latency: %d\n", latency);
		if (timeSynced_ == 10) printf("synced\n");
}

void NetworkClient::HandleMessage(uint16_t msgLength, const char* buffer) {
	MessageType type = (MessageType)(buffer[HeaderLenFieldSize]);
	switch (type)
	{
	case MessageType::PLAYERSUPDATE :
	{
		PlayersUpdateMessage msg = msgpack::unpack<PlayersUpdateMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		//printf("vel: %f,%f,%f pos: %f,%f,%f rot: %f\n", msg.playerValues[0].velocity[0], msg.playerValues[0].velocity[1], msg.playerValues[0].velocity[2], msg.playerValues[0].position[0], msg.playerValues[0].position[1], msg.playerValues[0].position[2], msg.playerValues[0].rotation);
		//As UDP packets can arrive out of order, only want most up to date values
		if (msg.time > prevServerPlayerValTime) {
			prevServerPlayerValTime = msg.time;
			scene_->SetServerPlayerVals(msg.playerValues, prevServerPlayerValTime);
		}
	}
	break;
	case MessageType::TIMEREQUEST:
	{
		//printf("syncing\n");
		TimeRequestMessage msg = msgpack::unpack<TimeRequestMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		SyncTimeReceive(msg);
	}
		break;
	case MessageType::SERVERACCEPT:
		CreateClientInfoMessage();
		std::thread(&NetworkClient::SyncTimeSend, this).detach();
		break;
	case MessageType::SERVERFULL:
		printf("Server full.\n");
		break;
	case MessageType::JOINGAME:
	{
		if (scene_->GetMyPlayer() == nullptr) {
			JoinGameMessage msg = msgpack::unpack<JoinGameMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
			scene_->AddMyPlayer(msg);
		}
	}
	break;
	case MessageType::NEWPLAYER:
	{
		NewPlayerMessage msg = msgpack::unpack<NewPlayerMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		scene_->AddPlayer(msg.playerID);
	}
	break;
	case MessageType::PLAYERQUIT:
	{
		PlayerQuitMessage msg = msgpack::unpack<PlayerQuitMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		scene_->RemovePlayer(msg.playerID);
	}
	break;
	case MessageType::CHAT:
	{
		ChatMessage msg = msgpack::unpack<ChatMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		scene_->TextToChat(msg.playerID, msg.chatStr.c_str());
	}
	break;
	default:
		break;
	}
}



// ---------- StartWinSock() and die() taken from lab 4 -------------

void NetworkClient::StartWinSock() {
	// We want version 2.2.
	WSADATA w;
	int error = WSAStartup(0x0202, &w);
	if (error != 0)
	{
		die("WSAStartup failed");
	}
	if (w.wVersion != 0x0202)
	{
		WSACleanup();
		die("Wrong WinSock version");
	}
}

void NetworkClient::die(const char* message) {
	fprintf(stderr, "\nError: %s (WSAGetLastError() = %d)", message, WSAGetLastError());
	WSACleanup();
#ifdef _DEBUG
	// Debug build -- drop the program into the debugger.
	abort();
#else
	exit(1);
#endif
}

void NetworkClient::CleanupSocket() {
	printf("Closing connection\n");
	closesocket(socketTCP_);
}

