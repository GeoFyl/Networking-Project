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
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"
#include <iostream>
#include "NetworkClient.h"

SceneApp::SceneApp(gef::Platform& platform) :
	Application(platform),
	sprite_renderer_(NULL),
	renderer_3d_(NULL),
	primitive_builder_(NULL),
	font_(NULL)
{
	playerInput_.velocity.push_back(0);
	playerInput_.velocity.push_back(0);
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

	// Initialise the scene objects
	ground_.set_mesh(primitive_builder_->CreateBoxMesh(gef::Vector4(30.f, 0.5f, 30.f)));
	ground_.InitPhysx(physx::PxVec3(30.f, 0.5f, 30.f), physx::PxVec3(0, 0, 0), gScene, gPhysics);

	block_.set_mesh(primitive_builder_->CreateBoxMesh(gef::Vector4(0.5, 0.5, 0.5)));
	block_.InitPhysx(physx::PxVec3(0.5, 0.5, 0.5), physx::PxVec3(2, 1, 0), gScene, gPhysics);


	InitFont();
	SetupLights();

	// initialise imgui
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	const gef::PlatformD3D11& platform_d3d = static_cast<const gef::PlatformD3D11&>(platform_);
	ImGui_ImplWin32_Init(platform_d3d.hwnd());
	ImGui_ImplDX11_Init(platform_d3d.device(), platform_d3d.device_context());
}

void SceneApp::initPhysics()
{
	using namespace physx;
	gFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gAllocator, gErrorCallback);

	gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gFoundation, PxTolerancesScale(), true);

	sceneDesc = new PxSceneDesc(gPhysics->getTolerancesScale());
	sceneDesc->gravity = PxVec3(0.0f, -9.81f, 0.0f);
	gDispatcher = PxDefaultCpuDispatcherCreate(2);
	sceneDesc->cpuDispatcher = gDispatcher;
	sceneDesc->filterShader = PxDefaultSimulationFilterShader;
	gScene = gPhysics->createScene(*sceneDesc);
	
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

void SceneApp::AddMyPlayer(JoinGameMessage msg) {
	playersMutex_.lock();
	for(auto playerID : msg.activePlayers) {
		addPlayer(primitive_builder_, gScene, gPhysics, playerID);
	}
	myPlayer_ = players_[msg.playerID].get();
	playersMutex_.unlock();
}

void SceneApp::AddPlayer(int playerID) {
	playersMutex_.lock();
	addPlayer(primitive_builder_, gScene, gPhysics, playerID);
	playersMutex_.unlock();
}

void SceneApp::RemovePlayer(int playerID) {
	playersMutex_.lock();
	players_[playerID].reset();
	playersMutex_.unlock();
}

void SceneApp::addPlayer(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int playerID) {
	players_[playerID] = std::make_unique<Player>();
	players_[playerID]->Init(primitive_builder_, gScene, gPhysics, sceneDesc, playerID);
	players_[playerID]->setID(playerID);
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

	gPhysics->release();
	gFoundation->release();

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

	playersMutex_.lock();
	if (myPlayer_) {
		if (keyInput->IsKeyPressed(gef::Keyboard::KC_SPACE) || keyInput->IsKeyDown(gef::Keyboard::KC_E) || keyInput->IsKeyDown(gef::Keyboard::KC_Q) || keyInput->IsKeyDown(gef::Keyboard::KC_W) || keyInput->IsKeyDown(gef::Keyboard::KC_S) || keyInput->IsKeyDown(gef::Keyboard::KC_A) || keyInput->IsKeyDown(gef::Keyboard::KC_D) || prevSentInputVel != physx::PxVec3(0,0,0)) {
			physx::PxRigidDynamic* playerBody = myPlayer_->GetPxBody();
			physx::PxVec3 oldVelocity = playerBody->getLinearVelocity();
			physx::PxVec3 newVelocity(0, 0, 0);

			playerInput_.jump = false;
			if (keyInput->IsKeyPressed(gef::Keyboard::KC_SPACE)) {
				playerBody->addForce(physx::PxVec3(0, 7, 0), physx::PxForceMode::eIMPULSE); //TODO: stop infinite jumping
				playerInput_.jump = true;
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_E)) {
				myPlayer_->rotate(gef::DegToRad(-90) * frame_time);
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_Q)) {
				myPlayer_->rotate(gef::DegToRad(90) * frame_time);
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_W)) {
				newVelocity += myPlayer_->getForwardVec();
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_S)) {
				newVelocity -= myPlayer_->getForwardVec();
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_A)) {
				newVelocity -= myPlayer_->getRightVec();
			}
			if (keyInput->IsKeyDown(gef::Keyboard::KC_D)) {
				newVelocity += myPlayer_->getRightVec();
			} //Send this to server

			prevSentInputVel = newVelocity;

			playerInput_.velocity[0] = newVelocity.x;
			playerInput_.velocity[1] = newVelocity.z;
			playerInput_.rotation = myPlayer_->getRotation();
		
			network_.SendInput(playerInput_); // Send input to server
			
			newVelocity.normalize();
			newVelocity = newVelocity * 3;
			newVelocity.y = oldVelocity.y;

			myPlayer_->setVelocity(newVelocity);

		}
	}

	updatePhysics(frame_time);

	//================= Interpolation stuff here =======================

	for (auto& player : players_) {
		if (player) {
			int ID = player->getID();
			if (serverPlayerValues_.count(ID)) {
				//Get the position sent by the server and players current position
				physx::PxVec3 pos(0, 0, 0);
				pos.x = serverPlayerValues_[ID].position[0];
				pos.y = serverPlayerValues_[ID].position[1];
				pos.z = serverPlayerValues_[ID].position[2];

				physx::PxVec3 vel(0, 0, 0);
				vel.x = serverPlayerValues_[ID].velocity[0];
				vel.y = serverPlayerValues_[ID].velocity[1];
				vel.z = serverPlayerValues_[ID].velocity[2];

				physx::PxVec3 currentPos = player->getPosition();
				printf("time: %d, x: %f, y: %f\n", network_.GetTime(), currentPos.x, currentPos.y);

				// Do interpolation if player isn't close enough (but also skip if too far, just teleport the player)
				float diff = (pos - currentPos).magnitude();
				if (diff > 0.01 && diff < 0.8) {
					// Run physics simulation on player using the latest server values
					player->SwitchToLocalScene();
					player->setPosition(pos);
					player->setVelocity(vel);
					player->GetPxBody()->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
					float delay = (network_.GetTime() - serverValuesTime_) / 1000;
					player->UpdateLocalScene(serverTick_ + delay); // Simulate from when it was sent till the next tick time (in the future)
		
					// get players future position and work out velocity needed to make it there quick enough
					physx::PxVec3 futurePos = player->getPosition();
					physx::PxVec3 lerpVelocity = (futurePos - currentPos) * TICKRATE;
					
					// go back to incorrect position but with the new interpolation velocity
					player->GetPxBody()->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
					player->SwitchToGlobalScene();
					player->setPosition(currentPos);
					player->setVelocity(lerpVelocity);
				}
				else {
					player->SwitchToGlobalScene();
					player->setPosition(pos);
					player->setVelocity(vel);
					player->GetPxBody()->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
				}

				player->setRotation(serverPlayerValues_[ID].rotation);
			}
			player->UpdatePhysx();

		}
	}

	playersMutex_.unlock();

	serverPlayerValues_.clear();

	gui();

	return true;
}

void SceneApp::gui() {
	ImGuiWindowFlags window_flags = 0;
	window_flags |= ImGuiWindowFlags_NoMove;
	window_flags |= ImGuiWindowFlags_NoResize;
	window_flags |= ImGuiWindowFlags_NoCollapse;

	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::SetNextWindowPos(ImVec2(20, platform_.height() - 200));
	ImGui::SetNextWindowSize(ImVec2(350, 180));
	ImGui::Begin("Chat", NULL, window_flags);
	ImGui::BeginChild("Child1", ImVec2(ImGui::GetWindowContentRegionWidth(), 120));
	ImGui::TextWrapped(chatString_.c_str());
	ImGui::SetScrollHere(1);
	ImGui::EndChild();
	std::string name = "Player " + (myPlayer_ ? std::to_string(myPlayer_->getID()) : "") + ":";
	ImGui::Text(name.c_str());
	ImGui::SameLine();
	ImGui::PushItemWidth(-1);
	if (ImGui::InputText("", ChatBuff_, 100, ImGuiInputTextFlags_EnterReturnsTrue)) { 
		network_.CreateChatMessage(ChatBuff_, myPlayer_->getID());
		memset(ChatBuff_, 0, 100);
	};
	ImGui::End();
	//ImGui::ShowDemoWindow(); // Show demo window! :)
}

void SceneApp::TextToChat(int playerID, const char* chatMsg) {
	chatString_ += "Player " + std::to_string(playerID) + ": " + chatMsg;
	chatString_ += "\n";
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

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void SceneApp::renderPlayers() {
	for (int i = 0; i < 5 ; i++) {
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
