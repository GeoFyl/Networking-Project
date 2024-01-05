#pragma once
#include "graphics/mesh_instance.h"
#include <PxPhysicsAPI.h>
#include <memory>

class GameObject : public gef::MeshInstance {
public:
	~GameObject();
	void InitPhysx(physx::PxVec3 halfExtent, physx::PxVec3 pos, physx::PxScene* scene, physx::PxPhysics* gPhysics, bool dynamic = false);
	void UpdatePhysx();
	virtual physx::PxRigidActor* GetPxBody() { return pxbody_; }

protected:
	physx::PxRigidActor* pxbody_;
};

