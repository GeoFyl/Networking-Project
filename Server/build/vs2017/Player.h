#pragma once
#include "GameObject.h"
#include "primitive_builder.h"
#include <PxPhysicsAPI.h>

class Player : public GameObject {
public:
	void Init(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int ID);
	void setID(int ID) { playerID = ID; }
	int getID() { return playerID; }
	void rotate(float angle);
	void setRotation(float angle);
	float getRotation();
	void setVelocity(physx::PxVec3 newVelocity);
	void setPosition(physx::PxVec3 pos);
	physx::PxVec3 getVelocity() { return GetPxBody()->getLinearVelocity(); }
	physx::PxVec3 getPosition() { return GetPxBody()->getGlobalPose().p; }
	physx::PxVec3 getForwardVec();
	physx::PxVec3 getRightVec();
	physx::PxRigidDynamic* GetPxBody() override;

protected:
	int playerID;
	physx::PxVec3 velocity = physx::PxVec3(0, 0, 0);
	physx::PxVec3 forwardVec = physx::PxVec3(0, 0, -1);
	physx::PxVec3 rightVec = physx::PxVec3(1, 0, 0);
};

