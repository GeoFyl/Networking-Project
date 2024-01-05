#ifndef _SCENE_APP_H
#define _SCENE_APP_H
#define _WINSOCKAPI_
#define NOMINMAX

#pragma once
#include <Windows.h>
#include "NetworkServer.h"
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
	void AddPlayer(int playerID);
	void RemovePlayer(int playerID);
	int GetAvailableID();
	void SetInput(int playerID, InputUpdateMessage& input);
	std::map<int, PlayerValues>* GetPlayerValues();
private:
	void InitFont();
	void CleanUpFont();
	void DrawHUD();
	void SetupLights();
	void initPhysics();
	void updatePhysics(float dt);
	Player* addPlayer(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int playerID);
	void renderPlayers();

	NetworkServer network_;

	gef::InputManager* input_;
    
	gef::SpriteRenderer* sprite_renderer_;
	gef::Font* font_;
	gef::Renderer3D* renderer_3d_;

	PrimitiveBuilder* primitive_builder_;

	std::unique_ptr<Player> players_[5];
	std::mutex playersMutex_;

	std::unique_ptr<InputUpdateMessage> playerInputs_[5];
	std::mutex inputMutex_;

	std::map<int, PlayerValues> playerValues_;

	GameObject ground_;
	GameObject block_;

	physx::PxDefaultAllocator		gAllocator;
	physx::PxDefaultErrorCallback	gErrorCallback;
	physx::PxFoundation* gFoundation = NULL;
	physx::PxPhysics* gPhysics = NULL;
	physx::PxDefaultCpuDispatcher* gDispatcher = NULL;
	physx::PxScene* gScene = NULL;
	float accumulator_ = 0.0f;
	float stepSize_ = 1.0f / 60.0f;

	float fps_;
};

#endif // _SCENE_APP_H
