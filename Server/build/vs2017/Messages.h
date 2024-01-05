#pragma once
#include <string>
#include <vector>
#include <map>

enum class MessageType { INPUTUPDATE, TIMEREQUEST, PLAYERSUPDATE, PING, SERVERACCEPT, SERVERFULL, CLIENTINFO, JOINGAME, NEWPLAYER, PLAYERQUIT, CHAT };
//enum class PlayerInputs { VELOCITY_X, VELOCITY_Z, ROTATION, JUMP };
//enum class PlayerInfo { VELOCITY_X, VELOCITY_Y, VELOCITY_Z, POSITION_X, POSITION_Y, POSITION_Z, ROTATION  };

struct TimeRequestMessage {
	uint32_t clientTime;
	uint32_t serverTime;

	template<class T>
	void pack(T& pack) {
		pack(clientTime, serverTime);
	}
};

struct InputUpdateMessage {
	uint32_t time;
	std::vector<float> velocity;
	float rotation;
	bool jump;

	template<class T>
	void pack(T& pack) {
		pack(time, velocity, rotation, jump);
	}
};

struct PlayerValues {
	std::vector<float> position;
	std::vector<float> velocity;
	float rotation;

	template<class T>
	void pack(T& pack) {
		pack(position, velocity, rotation);
	}
};

struct PlayersUpdateMessage {
	uint32_t time;
	std::map<int, PlayerValues> playerValues;

	template<class T>
	void pack(T& pack) {
		pack(time, playerValues);
	}
};

struct ClientInfoMessage {
	int portUDP;

	template<class T>
	void pack(T& pack) {
		pack(portUDP);
	}
};

struct JoinGameMessage {
	int playerID;
	std::vector<int> activePlayers;

	template<class T>
	void pack(T& pack) {
		pack(playerID, activePlayers);
	}
};

struct NewPlayerMessage {
	int playerID;

	template<class T>
	void pack(T& pack) {
		pack(playerID);
	}
};

struct PlayerQuitMessage {
	int playerID;

	template<class T>
	void pack(T& pack) {
		pack(playerID);
	}
};

struct ChatMessage {
	int playerID;
	std::string chatStr;

	template<class T>
	void pack(T& pack) {
		pack(playerID, chatStr);
	}
};