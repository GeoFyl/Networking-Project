#include "Connection.h"
#include <iostream>
#include "NetworkServer.h"
#include "msgpack.hpp"
#include "Player.h"

Connection::Connection(SOCKET sock, WSAEVENT eventTCP, int playerID, NetworkServer* server) {
	socketTCP_ = sock;
	playerID_ = playerID;
	eventTCP_ = eventTCP;
	server_ = server;
}

// Destructor.
Connection::~Connection() {
	printf("Closing connection\n");
	closesocket(socketTCP_);
}

bool Connection::Read() {
	// Receive the length of the incoming message if we haven't already
	if (readCountTCP_ < HeaderLenFieldSize) {
		int count = recv(socketTCP_, readBufferTCP_ + readCountTCP_, HeaderLenFieldSize - readCountTCP_, 0);
		if (count == SOCKET_ERROR) {
			printf("Receive failed\n");
			if (WSAGetLastError() == WSAEWOULDBLOCK) {
				return true;
			}
			else {
				printf("Connection closed or broken\n");
				return false;
			}
		}
		printf("TCP Received %d bytes\n", count);
		readCountTCP_ += count;
		if (count < HeaderLenFieldSize) return false;
	}

	// Get length of message
	uint16_t msgLength = (uint16_t)readBufferTCP_[0];

	int count = recv(socketTCP_, readBufferTCP_ + readCountTCP_, msgLength - readCountTCP_, 0);
	if (count == SOCKET_ERROR) {
		printf("Receive failed\n");
		if (WSAGetLastError() == WSAEWOULDBLOCK) {
			return true;
		}
		else {
			printf("Connection closed or broken\n");
			return false;
		}
	}
	printf("TCP Received %d bytes\n", count);
	readCountTCP_ += count;

	if (readCountTCP_ < msgLength) {
		// ... but we've not received a complete message yet.
		// So we can't do anything until we receive some more.
		return true;
	}

	// We've got a complete message.
	printf("TCP Received message from client: '");
	fwrite(readBufferTCP_, 1, msgLength, stdout);
	printf("'\n\n");

	server_->HandleMessage(playerID_, msgLength, readBufferTCP_);

	// Clear the buffer, ready for the next message.
	readCountTCP_ = 0;
	return true;
}

int Connection::Write(std::string msg) {
	// Try to send as much as is left to send
	int msgLength = msg.length();
	int messageLeft = (msgLength)-writeCountTCP_;
	int count = send(socketTCP_, msg.data() + writeCountTCP_, messageLeft, 0);
	if (count == SOCKET_ERROR)
	{
		printf("Send failed\n");
		if (WSAGetLastError() == WSAEWOULDBLOCK || WSAGetLastError() == WSAENOBUFS) {
			writeableTCP_ = false;
			return false;
		}
		else {
			printf("Connection closed or broken\n");
			fprintf(stderr, "\nError: (WSAGetLastError() = %d)", WSAGetLastError());
			return -1;
		}
	}

	// We've written some data to the socket
	printf("Sent %d bytes to the client.\n", count);
	writeCountTCP_ += count;

	if (writeCountTCP_ < msgLength) {
		printf("Not all sent\n");
		// but not written the whole message. Stop writing for now.
		writeableTCP_ = false;
		//WSAResetEvent(eventsTCP_[1]);
		return false;
	}

	// Written a complete message.
	printf("Sent message to the client: '");
	fwrite(msg.data(), 1, msgLength, stdout);
	printf("'\n\n");

	writeCountTCP_ = 0;
	return true;
}

void Connection::CreateServerAcceptMessage() {
	MessageType msgType = MessageType::SERVERACCEPT;
	uint16_t msgLen = HeaderSize;

	memcpy(writeBufferTCP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferTCP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);

	AddMessage(msgLen, writeBufferTCP_);
}

void Connection::CreateServerFullMessage() {
	MessageType msgType = MessageType::SERVERFULL;
	uint16_t msgLen = HeaderSize;

	memcpy(writeBufferTCP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferTCP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);

	AddMessage(msgLen, writeBufferTCP_);
}

void Connection::CreateJoinMessage(std::vector<Connection*>& clients) {
	JoinGameMessage msg;
	MessageType msgType = MessageType::JOINGAME;
	msg.playerID = playerID_;
	for (auto client : clients) {
		msg.activePlayers.push_back(client->getPlayerID());
	}
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void Connection::CreateChatMessage(const char* chatMsg, int id) {
	//Create message
	ChatMessage msg;
	MessageType msgType = MessageType::CHAT;
	msg.playerID = id;
	msg.chatStr = chatMsg;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void Connection::CreateNewPlayerMessage(int id) {
	//Create message
	NewPlayerMessage msg;
	MessageType msgType = MessageType::NEWPLAYER;
	msg.playerID = id;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void Connection::CreatePlayerQuitMessage(int id) {
	//Create message
	NewPlayerMessage msg;
	MessageType msgType = MessageType::PLAYERQUIT;
	msg.playerID = id;
	std::vector<uint8_t> msgData = msgpack::pack(msg); //Serialize the message struct
	uint16_t msgLen = msgData.size() + HeaderSize;

	//Add the message to the queue of outgoing messages
	AddMessage(msgLen, msgType, msgData);
}

void Connection::AddMessage(uint16_t& msgLen, MessageType& msgType, std::vector<uint8_t>& msgData) {
	memcpy(writeBufferTCP_, &msgLen, HeaderLenFieldSize);
	memcpy(writeBufferTCP_ + HeaderLenFieldSize, &msgType, HeaderTypeFieldSize);
	memcpy(writeBufferTCP_ + HeaderSize, (const char*)msgData.data(), msgData.size());

	std::string msgStr(writeBufferTCP_, msgLen);

	msgsMutexTCP_.lock();
	msgsTCP_.push(msgStr);
	msgsMutexTCP_.unlock();

	WSASetEvent(eventTCP_); //Signal that there is a new message to be sent
}

void Connection::AddMessage(uint16_t& msgLen, const char* buffer) {
	std::string msgStr(buffer, msgLen);

	msgsMutexTCP_.lock();
	msgsTCP_.push(msgStr);
	msgsMutexTCP_.unlock();

	WSASetEvent(eventTCP_); //Signal that there is a new message to be sent
}

int Connection::SendMessages() { //1 - all good, 0 - unwritable, -1 - broken
	while (true) {
		msgsMutexTCP_.lock();
		if (msgsTCP_.empty()) {
			msgsMutexTCP_.unlock();
			return true;
		}
		std::string msg = msgsTCP_.front();
		msgsMutexTCP_.unlock();

		int result = Write(msg);
		if (result == 1) {
			// Remove message from queue
			msgsMutexTCP_.lock();
			msgsTCP_.pop();
			msgsMutexTCP_.unlock();
		}
		else return result;
	}
}
