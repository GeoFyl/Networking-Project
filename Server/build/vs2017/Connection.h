#pragma once
#define NOMINMAX
#include <WinSock2.h>
#include "Messages.h"
#include <string>
#include <vector>
#include <queue>
#include <mutex>

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

class NetworkServer;

class Connection {
public:
	// Constructor.
	// sock: the socket that we've accepted the client connection on.
	Connection(SOCKET sock, WSAEVENT eventTCP, int playerID, NetworkServer* server);

	// Destructor.
	~Connection();

	// Return the client's socket.
	SOCKET getSocketTCP() { return socketTCP_; };

	// Call this when the socket is ready to read.
	bool Read();

	// Call this when the socket is ready to write.
	int Write(std::string msg);

	void CreateServerAcceptMessage();
	void CreateServerFullMessage();
	void CreateJoinMessage(std::vector<Connection*> &clients);
	void CreateChatMessage(const char* chatMsg, int id);
	void CreateNewPlayerMessage(int id);
	void CreatePlayerQuitMessage(int id);
	void AddMessage(uint16_t& msgLen, MessageType& msgType, std::vector<uint8_t>& msgData);
	void AddMessage(uint16_t& msgLen, const char* buffer);
	int SendMessages();
	void setWriteable(bool b) { writeableTCP_ = b; }
	bool isWriteable() { return writeableTCP_; }

	int getPlayerID() { return playerID_; }
	sockaddr_in* getAddressUDP() { return addressUDP_.get(); }
	void setAddressUDP(sockaddr_in addr) { addressUDP_ = std::make_unique<sockaddr_in>(addr); }
	void setInput(std::map<int, float>& input) { playerInputs_ = input; }

	int* LastUpdateTime() { return &lastUpdateTime_; }

private:
	NetworkServer* server_;

	int playerID_;

	std::map<int, float> playerInputs_;

	std::unique_ptr<sockaddr_in> addressUDP_;
	int lastUpdateTime_ = 0;

	// This client's TCP socket.
	SOCKET socketTCP_;
	
	std::queue<std::string> msgsTCP_;
	std::mutex msgsMutexTCP_;

	// The data we've read from the client.
	int readCountTCP_ = 0;
	char readBufferTCP_[500];

	// The data for writing to the client.
	int writeCountTCP_ = 0;
	char writeBufferTCP_[500];

	// Socket can currently be written to?
	bool writeableTCP_ = false;

	WSAEVENT eventTCP_;
};

