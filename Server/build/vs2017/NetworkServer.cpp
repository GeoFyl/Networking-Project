#include "NetworkServer.h"
#include "scene_app.h"
#include <iostream>
#include "msgpack.hpp"
#include "Connection.h"
#include <chrono>
#pragma comment(lib, "ws2_32.lib")

////Message header format: 
//// +--------+--------+--------+
//// |      Length     |  Type  |
//// +--------+--------+--------+
//
////Size of the header overall
//#define HeaderSize sizeof(uint16_t) + sizeof(uint8_t)
////Size of the Length field in the header
//#define HeaderLenFieldSize sizeof(uint16_t)
////Size of Type field in header
//#define HeaderTypeFieldSize sizeof(uint8_t)

NetworkServer::~NetworkServer() {
	connectionThreadTCP_->join();
	connectionThreadUDP_->join();
}

void NetworkServer::DisplayLocalIP() {
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	
	SOCKET testsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (testsock == INVALID_SOCKET) {
		die("socket failed");
	}
	if (connect(testsock, (const sockaddr*)&addr, sizeof addr) == SOCKET_ERROR && WSAGetLastError() != WSAEWOULDBLOCK) {
		die("UDP connect failed");
	}

	sockaddr_in testaddr;
	int testaddrLen = sizeof(testaddr);
	getsockname(testsock, (sockaddr*)&testaddr, &testaddrLen);

	closesocket(testsock);

	printf("LAN IPv4 address: ");
	printf(inet_ntoa(testaddr.sin_addr));
	printf("\n");
}

void NetworkServer::StartListeningTCP() {
	//Build socket address structure for binding the socket
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVERPORT_TCP); // htons converts the port number to network byte order (big-endian).
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//Create TCP server/listen socket
	ListenSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket_ == INVALID_SOCKET) {
		die("socket failed");
	}

	// Bind the server socket to its address.
	if (bind(ListenSocket_, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
		die("bind failed");
	}

	// Create a new event for checking listen socket activity
	eventsTCP_[0] = WSACreateEvent();
	if (eventsTCP_.front() == WSA_INVALID_EVENT) {
		die("tcp listen event creation failed");
	}
	//Assosciate this event with the socket types we're interested in
	//In this case, on the server, we're interested in Accepts and Closes
	WSAEventSelect(ListenSocket_, eventsTCP_.front(), FD_ACCEPT | FD_CLOSE);

	//Start listening for connection requests on the socket
	if (listen(ListenSocket_, 1) == SOCKET_ERROR) {
		die("listen failed");
	}

	printf("Listening for TCP on socket %d...\n", ListenSocket_);
}

void NetworkServer::StartListeningUDP() {
	//Build socket address structure for binding the socket
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVERPORT_UDP); // htons converts the port number to network byte order (big-endian).
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	//Create UDP server/listen socket
	socketUDP_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketUDP_ == INVALID_SOCKET) {
		die("socket failed");
	}

	// Bind the server socket to its address.
	if (bind(socketUDP_, (SOCKADDR*)&addr, sizeof(addr)) != 0) {
		die("bind failed");
	}

	// Create a new event for checking socket activity
	eventUDP_ = WSACreateEvent();
	if (eventUDP_ == WSA_INVALID_EVENT) {
		die("udp event creation failed");
	}
	//Assosciate this event with the socket types we're interested in
	//In this case, on the server, we're interested in Reads and Writes
	WSAEventSelect(socketUDP_, eventUDP_, FD_READ | FD_WRITE);

	printf("UDP ready on socket %d...\n", socketUDP_);
}


void NetworkServer::RestartListeningTCP() {
	printf("Restarting listening\n");
	closesocket(ListenSocket_);
	WSACloseEvent(eventsTCP_.front());

	StartListeningTCP();
}

void NetworkServer::StartConnection(SceneApp* scene) {
	scene_ = scene;

	StartWinSock();
	printf("Server starting\n");

	DisplayLocalIP();
	StartListeningTCP();
	StartListeningUDP();

	connectionThreadTCP_ = new std::thread(&NetworkServer::ConnectionLoopTCP, this);
	connectionThreadUDP_ = new std::thread(&NetworkServer::ConnectionLoopUDP, this);
}

void NetworkServer::ConnectionLoopTCP() {
	while (true) {

		DWORD returnVal = WSAWaitForMultipleEvents(eventsTCP_.size(), eventsTCP_.data(), false, WSA_INFINITE, false);

		if ((returnVal != WSA_WAIT_TIMEOUT) && (returnVal != WSA_WAIT_FAILED)) {
			int eventIndex = returnVal - WSA_WAIT_EVENT_0; //In practice, eventIndex will equal returnVal, but this is here for compatability
			if (eventIndex == 0) { //Listen event

				if (WSAEnumNetworkEvents(ListenSocket_, eventsTCP_.front(), &networkEventsTCP_) == SOCKET_ERROR) {
					printf("Retrieving listen event information failed\n");
					RestartListeningTCP();
					continue;
				}
				if (networkEventsTCP_.lNetworkEvents & FD_ACCEPT)
				{
					if (networkEventsTCP_.iErrorCode[FD_ACCEPT_BIT] != 0) {
						printf("FD_ACCEPT failed with error %d\n", networkEventsTCP_.iErrorCode[FD_ACCEPT_BIT]);
						RestartListeningTCP();
						continue;
					}
					// Accept a new connection, and add it to the socket and event lists
					eventsTCP_.push_back(WSACreateEvent());
					Connection* conn = new Connection(accept(ListenSocket_, NULL, NULL), eventsTCP_.back(), scene_->GetAvailableID(), this);
					connections_.push_back(conn);
					playerIDtoConnection_[conn->getPlayerID()] = conn;
					//eventIndexMap_.push_back(connections_.size() - 1);

					WSAEventSelect(connections_.back()->getSocketTCP(), eventsTCP_.back(), FD_CLOSE | FD_READ | FD_WRITE);

					//Check socket for errors
					if (WSAEnumNetworkEvents(connections_.back()->getSocketTCP(), eventsTCP_.back(), &networkEventsTCP_) == SOCKET_ERROR) {
						printf("Retrieving event information failed\n");
						CleanupSocket(connections_.size() - 1);
						continue;
					}
					if (networkEventsTCP_.lNetworkEvents & FD_WRITE) {
						if (networkEventsTCP_.iErrorCode[FD_WRITE_BIT] != 0) {
							printf("FD_WRITE failed with error %d\n", networkEventsTCP_.iErrorCode[FD_WRITE_BIT]);
							CleanupSocket(connections_.size() - 1);
						}
						conn->setWriteable(true);
					}
					if (connections_.size() <= 5) {
						conn->CreateServerAcceptMessage();

						printf("Socket %d connected\n", connections_.back()->getSocketTCP());
						sockaddr_in addr;
						int addrLen = sizeof(addr);
						getpeername(connections_.back()->getSocketTCP(), (sockaddr*)&addr, &addrLen);

						printf("Client IPv4 address: ");
						printf(inet_ntoa(addr.sin_addr));
						printf("\n");
						printf("Client TCP port: %d\n\n", addr.sin_port);
					}
					else {
						conn->CreateServerFullMessage();
						conn->SendMessages();
						CleanupSocket(connections_.size());
						printf("Server full - client rejected\n");
					}
				}
			}
			else {
				Connection* conn = connections_[eventIndex - 1];

				if (WSAEnumNetworkEvents(conn->getSocketTCP(), eventsTCP_[eventIndex], &networkEventsTCP_) == SOCKET_ERROR) {
					printf("Retrieving event information failed\n");
					CleanupSocket(eventIndex);
					continue;
				}
				if (networkEventsTCP_.lNetworkEvents & FD_CLOSE)
				{
					//Ignore the error if the client just force quit
					if (networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] != 0 && networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] != 10053)
					{
						printf("FD_CLOSE with error %d\n", networkEventsTCP_.iErrorCode[FD_CLOSE_BIT]);
					}
					else if (networkEventsTCP_.iErrorCode[FD_CLOSE_BIT] == 10053) {
						printf("Client closed connection.\n");
					}
					for (auto client : connections_) {
						if (client != conn) client->CreatePlayerQuitMessage(conn->getPlayerID());
					}
					scene_->RemovePlayer(conn->getPlayerID());
					CleanupSocket(eventIndex);
				}
				if (networkEventsTCP_.lNetworkEvents & FD_READ) {
					//Reading message from client
					conn->Read();
				}
				if (networkEventsTCP_.lNetworkEvents & FD_WRITE) {
					conn->setWriteable(true);
				}
				if (conn->isWriteable()) {
					int result = conn->SendMessages();
					if (result == -1) {
						printf("Sending to client failed.\n");
						CleanupSocket(eventIndex);
					}
				}
			}
		}
		else if (returnVal == WSA_WAIT_TIMEOUT) {
			//All good, we just have no activity
		}
		else if (returnVal == WSA_WAIT_FAILED) {
			die("TCP WSAWaitForMultipleEvents failed!");
		}
	}
}

void NetworkServer::ConnectionLoopUDP() {
	int previousTime = time_;
	int deltaTime = 0;

	while (true) {
		DWORD returnVal = WSAWaitForMultipleEvents(1, &eventUDP_, false, server_tick_, false);

		if (returnVal != WSA_WAIT_FAILED) {

			if (WSAEnumNetworkEvents(socketUDP_, eventUDP_, &networkEventsUDP_) == SOCKET_ERROR) {
				die("Retrieving UDP event information failed");
			}
			if (networkEventsUDP_.lNetworkEvents & FD_READ) {
				ReadUDP();
			}
			if (networkEventsUDP_.lNetworkEvents & FD_WRITE) {
				writeableUDP_ = true;
			}
			deltaTime += time_ - previousTime;
			previousTime = time_;

			if (deltaTime >= server_tick_ && writeableUDP_) {
				deltaTime = 0;
				SendUDP();
			}
		}
		else if (returnVal == WSA_WAIT_FAILED) {
			die("UDP WSAWaitForMultipleEvents failed!");
		}
	}
}

void NetworkServer::UpdateTime() {
	time_ = std::chrono::duration_cast<std::chrono::milliseconds>(ServerClock::now() - timeStart_).count();
	//printf("time: %d\n", time_);
}

bool NetworkServer::ReadUDP()
{
	//Reading message from client
	sockaddr_in fromAddr;
	int fromAddrSize = sizeof(fromAddr);
	int count = recvfrom(socketUDP_, readBufferUDP_, sizeof(readBufferUDP_), 0, (sockaddr*)&fromAddr, &fromAddrSize);
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
	//printf("UDP Received %d bytes\n", count);

	try {
		int playerID = addressUDPtoID_.at(fromAddr);

		// Get length of message
		uint16_t msgLength = (uint16_t)readBufferUDP_[0];
		if (count == msgLength) {
			//printf("\nReceived UDP message: '");
			//fwrite(readBufferUDP_, 1, msgLength, stdout);
			//printf("'\n");

			HandleMessage(playerID, msgLength, readBufferUDP_);
		}
		else {
			printf("UDP datagram wrong length - discarding.\n");
		}
	}
	catch (const std::out_of_range& err) {
		printf("UDP unknown source - discarding.\n");
	}

	return true;
}

bool NetworkServer::WriteUDP(sockaddr_in* address, uint16_t& length)
{
	if (address) {
		int count = sendto(socketUDP_, writeBufferUDP_, length, 0, (const sockaddr*)address, sizeof(sockaddr));
		if (count == SOCKET_ERROR) {
			if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAENOBUFS) {
				writeableUDP_ = false;
				return false;
			}
			else {
				die("UDP error writing"); //TODO: restart udp listening
			}
		}
		//printf("Sent UDP message to the client: '");
		//fwrite(writeBufferUDP_, 1, length, stdout);
		//printf("'\n\n");
		return true;
	}
}

void NetworkServer::SendTimeReplyMessage(sockaddr_in* address, TimeRequestMessage& msg) {
	MessageType msgType = MessageType::TIMEREQUEST;
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);

	msg.serverTime = time_;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;
	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	if (writeableUDP_) WriteUDP(address, msgLen);
	//printf("reply time: %d\n", msg.serverTime);
}

bool NetworkServer::SendUDP() {
	uint16_t msgLength = CreatePlayersUpdateMessage();
	//printf("sending update, %d\n", time_);
	for (auto conn : connections_) {
		if (writeableUDP_) WriteUDP(conn->getAddressUDP(), msgLength);
		else return false;
	}
	return true;
}

uint16_t NetworkServer::CreatePingMessage() {

	MessageType msgType = MessageType::PING;
	uint16_t msgLen = HeaderSize;

	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);

	return msgLen;
}

uint16_t NetworkServer::CreatePlayersUpdateMessage()
{
	PlayersUpdateMessage msg;
	msg.time = time_;
	msg.playerValues = *scene_->GetPlayerValues();
	MessageType msgType = MessageType::PLAYERSUPDATE;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;
	memcpy(writeBufferUDP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);
	memcpy(writeBufferUDP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferUDP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	return msgLen;
}

void NetworkServer::HandleMessage(int playerID, uint16_t msgLength, const char* buffer) {
	MessageType type = (MessageType)(buffer[HeaderLenFieldSize]);
	switch (type)
	{
	case MessageType::TIMEREQUEST:
	{
		TimeRequestMessage msg = msgpack::unpack<TimeRequestMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		SendTimeReplyMessage(playerIDtoConnection_[playerID]->getAddressUDP(), msg);
	}
	break;
	case MessageType::INPUTUPDATE:
	{
		Connection* conn = playerIDtoConnection_[playerID];
		InputUpdateMessage msg = msgpack::unpack<InputUpdateMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		//printf("ltime: %d, mtime: %d, x: %f\n", time_, msg.time, msg.input[(int)PlayerInputs::VELOCITY_X]);

		//printf("vel: %f,%f rot: %f, jump: %d\n", msg.velocity[0], msg.velocity[1], msg.rotation, msg.jump);

		//As UDP packets can arrive out of order, only want most up to date inputs
		if (msg.time > *conn->LastUpdateTime()) {
			*conn->LastUpdateTime() = msg.time;
			scene_->SetInput(playerID, msg);
		}
	}
		break;
	case MessageType::PING:
		printf("Client ping\n");
		break;
	case MessageType::CLIENTINFO: 
	{
		printf("recieved client info\n");
		ClientInfoMessage msg = msgpack::unpack<ClientInfoMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);

		Connection* newClient = playerIDtoConnection_[playerID];

		//Build socket address structure
		sockaddr_in addr;
		int addrLen = sizeof(addr);
		getpeername(newClient->getSocketTCP(), (sockaddr*)&addr, &addrLen);
		addr.sin_port = msg.portUDP; 

		printf("Client UDP port: %d\n\n", addr.sin_port);

		addressUDPtoID_[addr] = playerID;

		newClient->setAddressUDP(addr);
		newClient->CreateJoinMessage(connections_);
		for (auto client : connections_) {
			if (client != newClient) client->CreateNewPlayerMessage(playerID);
		}
		scene_->AddPlayer(playerID);
	}
		break;
	case MessageType::CHAT:
	{
		// Recreate the messages server-side to include the true player ID so no hackers can impersonate other players.
		ChatMessage msg = msgpack::unpack<ChatMessage>((uint8_t*)&buffer[HeaderSize], msgLength - HeaderSize);
		for (auto client : connections_) {
			client->CreateChatMessage(msg.chatStr.c_str(), playerID);
		}
	}
		break;
	default:
		break;
	}
}





// ---------- StartWinSock() and die() taken from lab 4 -------------

void NetworkServer::StartWinSock() {
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

void NetworkServer::die(const char* message) {
	fprintf(stderr, "\nError: %s (WSAGetLastError() = %d)", message, WSAGetLastError());
	WSACleanup();
#ifdef _DEBUG
	// Debug build -- drop the program into the debugger.
	abort();
#else
	exit(1);
#endif
}

void NetworkServer::CleanupSocket(int index) {
	addressUDPtoID_.erase(addressUDPtoID_.find(*connections_[index - 1]->getAddressUDP()));
	connections_.erase(connections_.begin() + index - 1);
	WSACloseEvent(eventsTCP_[index]);
	eventsTCP_.erase(eventsTCP_.begin() + index);
}

bool operator==(const sockaddr_in& left, const sockaddr_in& right)
{
	return (left.sin_port == right.sin_port)
		&& (memcmp(&left.sin_addr, &right.sin_addr, sizeof(left.sin_addr)) == 0);
}

bool operator<(const sockaddr_in& left, const sockaddr_in& right)
{
	int c = memcmp(&left.sin_addr, &right.sin_addr, sizeof(left.sin_addr));
	if (c < 0)
		return true;
	else if (c == 0)
		return left.sin_port < right.sin_port;
	else
		return false;
}