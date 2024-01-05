#pragma once
#include "scene_app.h"
#include <system/platform.h>
#include <platform/d3d11/system/platform_d3d11.h>
#include <graphics/sprite_renderer.h>
#include <graphics/font.h>
#include <system/debug_log.h>
#include <graphics/renderer_3d.h>
#include <maths/math_utils.h>
#include <input/input_manager.h>
#include <iostream>

SceneApp::SceneApp(gef::Platform& platform) :
	Application(platform),
	sprite_renderer_(NULL),
	renderer_3d_(NULL),
	primitive_builder_(NULL),
	font_(NULL)
{
}

void SceneApp::Init()
{
	input_ = gef::InputManager::Create(platform_);

	sprite_renderer_ = gef::SpriteRenderer::Create(platform_);

	// create the renderer for draw 3D geometry
	renderer_3d_ = gef::Renderer3D::Create(platform_);

	// initialise primitive builder to make create some 3D geometry easier
	primitive_builder_ = new PrimitiveBuilder(platform_);

	//Initialise PhysX
	initPhysics();

	network_.StartConnection(this);

	ground_.set_mesh(primitive_builder_->CreateBoxMesh(gef::Vector4(30.f, 0.5f, 30.f)));
	ground_.InitPhysx(physx::PxVec3(30.f, 0.5f, 30.f), physx::PxVec3(0, 0, 0), gScene, gPhysics);

	block_.set_mesh(primitive_builder_->CreateBoxMesh(gef::Vector4(0.5, 0.5, 0.5)));
	block_.InitPhysx(physx::PxVec3(0.5, 0.5, 0.5), physx::PxVec3(2, 1, 0), gScene, gPhysics);


	InitFont();
	SetupLights();
}

void SceneApp::initPhysics()
{
	using namespace physx;
	gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

	gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true);

	PxSceneDesc sceneDesc(gPhysics->getTolerancesScale());
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f);
	gDispatcher = PxDefaultCpuDispatcherCreate(2);
	sceneDesc.cpuDispatcher = gDispatcher;
	sceneDesc.filterShader = PxDefaultSimulationFilterShader;
	gScene = gPhysics->createScene(sceneDesc);

	
}

void SceneApp::updatePhysics(float dt)
{
	accumulator_ += dt;
	if (accumulator_ < stepSize_)
		return;

	accumulator_ -= stepSize_;

	gScene->simulate(stepSize_);
	gScene->fetchResults(true);
	return;
}

void SceneApp::AddPlayer(int playerID) {
	playersMutex_.lock();
	addPlayer(primitive_builder_, gScene, gPhysics, playerID);
	playersMutex_.unlock();
}

void SceneApp::RemovePlayer(int playerID) {
	if (playerID > -1) {
		playersMutex_.lock();
		players_[playerID].reset();
		playersMutex_.unlock();
	}
}

int SceneApp::GetAvailableID()
{
	for (int i = 0; i < 5; i++) {
		if (!players_[i]) return i;
	}
	return -1;
}

void SceneApp::SetInput(int playerID, InputUpdateMessage& input) {
	inputMutex_.lock();
	// Keep jump input until processed in simulation
	if (!playerInputs_[playerID]) playerInputs_[playerID] = std::make_unique<InputUpdateMessage>();
	if (playerInputs_[playerID]->jump == true) input.jump = true;
	*playerInputs_[playerID].get() = input;
	inputMutex_.unlock();
}

std::map<int, PlayerValues>* SceneApp::GetPlayerValues()
{
	playerValues_.clear();
	playersMutex_.lock();
	for (auto& player : players_) {
		if (player) {
			physx::PxVec3 velocity = player->GetPxBody()->getLinearVelocity();
			playerValues_[player->getID()].velocity.push_back(velocity.x);
			playerValues_[player->getID()].velocity.push_back(velocity.y);
			playerValues_[player->getID()].velocity.push_back(velocity.z);
			
			physx::PxVec3 position = player->getPosition();
			playerValues_[player->getID()].position.push_back(position.x);
			playerValues_[player->getID()].position.push_back(position.y);
			playerValues_[player->getID()].position.push_back(position.z);

			playerValues_[player->getID()].rotation = player->getRotation();
		}
	}
	playersMutex_.unlock();
	
	return &playerValues_;
}

Player* SceneApp::addPlayer(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int playerID) {
	players_[playerID] = std::make_unique<Player>();
	//physxMutex_.lock();
	players_[playerID]->Init(primitive_builder_, gScene, gPhysics, playerID);
	//physxMutex_.unlock();
	return players_[playerID].get();
}

void SceneApp::CleanUp()
{
	CleanUpFont();

	delete primitive_builder_;
	primitive_builder_ = NULL;

	delete renderer_3d_;
	renderer_3d_ = NULL;

	delete sprite_renderer_;
	sprite_renderer_ = NULL;

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool SceneApp::Update(float frame_time)
{
	network_.UpdateTime();

	input_->Update();
	gef::Keyboard* keyInput = input_->keyboard();
	if (keyInput->IsKeyPressed(gef::Keyboard::KC_ESCAPE)) {
		return false;
	}

	fps_ = 1.0f / frame_time;

	inputMutex_.lock();
	playersMutex_.lock();
	for (int i = 0; i < 5; i++) {
		if (playerInputs_[i]) {
			if (playerInputs_[i]->jump) {
				players_[i]->GetPxBody()->addForce(physx::PxVec3(0, 7, 0), physx::PxForceMode::eIMPULSE);
			}

			players_[i]->setRotation(playerInputs_[i]->rotation);

			physx::PxVec3 newVelocity(playerInputs_[i]->velocity[0], 0, playerInputs_[i]->velocity[1]);
				
			newVelocity.normalize();
			newVelocity = newVelocity * 3;
			newVelocity.y = players_[i]->GetPxBody()->getLinearVelocity().y;                                                                                  
			players_[i]->setVelocity(newVelocity);
		
			playerInputs_[i].reset();
		}
	}
	inputMutex_.unlock();

	updatePhysics(frame_time);

	for (auto &player : players_) {
		if (player) {
			player->UpdatePhysx();
			physx::PxVec3 currentPos = player->getPosition();
			printf("time: %d, x: %f, y: %f\n", network_.GetTime(), currentPos.x, currentPos.y);
		}
	}

	playersMutex_.unlock();


	return true;
}

void SceneApp::Render()
{

	// setup camera

	// projection
	float fov = gef::DegToRad(45.0f);
	float aspect_ratio = (float)platform_.width() / (float)platform_.height();
	gef::Matrix44 projection_matrix;
	projection_matrix = platform_.PerspectiveProjectionFov(fov, aspect_ratio, 0.1f, 100.0f);
	renderer_3d_->set_projection_matrix(projection_matrix);

	// view
	gef::Vector4 camera_eye(0.0f, 3.0f, 10.0f);
	gef::Vector4 camera_lookat(0.0f, 0.0f, 0.0f);
	gef::Vector4 camera_up(0.0f, 1.0f, 0.0f);
	gef::Matrix44 view_matrix;
	view_matrix.LookAt(camera_eye, camera_lookat, camera_up);
	renderer_3d_->set_view_matrix(view_matrix);

	// draw 3d geometry
	renderer_3d_->Begin();

	renderer_3d_->set_override_material(NULL);
	renderer_3d_->DrawMesh(ground_);
	renderPlayers();
	renderer_3d_->set_override_material(&primitive_builder_->blue_material());
	renderer_3d_->DrawMesh(block_);

	renderer_3d_->set_override_material(NULL);

	renderer_3d_->End();

	// start drawing sprites, but don't clear the frame buffer
	sprite_renderer_->Begin(false);
	DrawHUD();
	sprite_renderer_->End();
}

void SceneApp::renderPlayers() {
	for(int i = 0; i < 5; i++) {
		if (players_[i]) {
			switch (i)
			{
			case 0:
				renderer_3d_->set_override_material(&primitive_builder_->red_material());
				break;
			case 1:
				renderer_3d_->set_override_material(&primitive_builder_->blue_material());
				break;
			case 2:
				renderer_3d_->set_override_material(&primitive_builder_->green_material());
				break;
			case 3:
				renderer_3d_->set_override_material(&primitive_builder_->orange_material());
				break;
			case 4:
				renderer_3d_->set_override_material(&primitive_builder_->pink_material());
				break;
			default:
				break;
			}
			renderer_3d_->DrawMesh(*players_[i].get());
		}
	}
}

void SceneApp::InitFont()
{
	font_ = new gef::Font(platform_);
	font_->Load("comic_sans");
}

void SceneApp::CleanUpFont()
{
	delete font_;
	font_ = NULL;
}

void SceneApp::DrawHUD()
{
	if(font_)
	{
		// display frame rate
		font_->RenderText(sprite_renderer_, gef::Vector4(850.0f, 510.0f, -0.9f), 1.0f, 0xffffffff, gef::TJ_LEFT, "FPS: %.1f", fps_);
	}
}

void SceneApp::SetupLights()
{
	// grab the data for the default shader used for rendering 3D geometry
	gef::Default3DShaderData& default_shader_data = renderer_3d_->default_shader_data();

	// set the ambient light
	default_shader_data.set_ambient_light_colour(gef::Colour(0.25f, 0.25f, 0.25f, 1.0f));

	// add a point light that is almost white, but with a blue tinge
	// the position of the light is set far away so it acts light a directional light
	gef::PointLight default_point_light;
	default_point_light.set_colour(gef::Colour(0.7f, 0.7f, 1.0f, 1.0f));
	default_point_light.set_position(gef::Vector4(-500.0f, 400.0f, 700.0f));
	default_shader_data.AddPointLight(default_point_light);
}
