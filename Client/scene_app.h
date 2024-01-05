#ifndef _SCENE_APP_H
#define _SCENE_APP_H
#define _WINSOCKAPI_
#define NOMINMAX

#pragma once
#include <Windows.h>
#include "NetworkClient.h"
#include <system/application.h>
#include <maths/vector2.h>
#include "primitive_builder.h"
#include <graphics/mesh_instance.h>
#include "GameObject.h"
#include "Player.h"
#include "Messages.h"
#include <input/keyboard.h>
#include <PxPhysicsAPI.h>
#include <string>
#include <mutex>
#include <map>



// FRAMEWORK FORWARD DECLARATIONS
namespace gef
{
	class Platform;
	class SpriteRenderer;
	class Font;
	class InputManager;
	class Renderer3D;
}

class SceneApp : public gef::Application
{
public:
	SceneApp(gef::Platform& platform);
	void Init();
	void CleanUp();
	bool Update(float frame_time);
	void Render();
	void AddMyPlayer(JoinGameMessage msg);
	void AddPlayer(int playerID);
	void RemovePlayer(int playerID);
	Player* GetMyPlayer() { return myPlayer_; }
	void TextToChat(int playerID, const char* chatMsg);
	void SetServerPlayerVals(std::map<int, PlayerValues>& vals, int time) { serverPlayerValues_ = vals; serverValuesTime_ = time; }
private:
	void InitFont();
	void CleanUpFont();
	void DrawHUD();
	void SetupLights();
	void initPhysics();
	void updatePhysics(float dt);
	void gui();
	void renderPlayers();
	void addPlayer(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int playerID);

	NetworkClient network_;

	gef::InputManager* input_;
    
	gef::SpriteRenderer* sprite_renderer_;
	gef::Font* font_;
	gef::Renderer3D* renderer_3d_;

	PrimitiveBuilder* primitive_builder_;

	std::unique_ptr<Player> players_[5];
	Player* myPlayer_ = nullptr;
	std::mutex playersMutex_;

	InputUpdateMessage playerInput_;
	physx::PxVec3 prevSentInputVel = physx::PxVec3(0, 0, 0);
	std::map<int, PlayerValues> serverPlayerValues_;
	int serverValuesTime_;
	//std::map<int, std::map<int, float>> prevServerPlayerValues_;

	GameObject ground_;
	GameObject block_;

	physx::PxDefaultAllocator		gAllocator;
	physx::PxDefaultErrorCallback	gErrorCallback;
	physx::PxFoundation* gFoundation = NULL;
	physx::PxPhysics* gPhysics = NULL;
	physx::PxDefaultCpuDispatcher* gDispatcher = NULL;
	physx::PxSceneDesc* sceneDesc = NULL;
	physx::PxScene* gScene = NULL;
	float accumulator_ = 0.0f;
	float stepSize_ = 1.0f / 60.0f;
	float serverTick_ = 1.f / TICKRATE;

	char ChatBuff_[100] = "";
	std::string chatString_ = "";

	float fps_;
};

#endif // _SCENE_APP_H
