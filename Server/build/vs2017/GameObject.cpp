#pragma once
#include "GameObject.h"

GameObject::~GameObject()
{
	pxbody_->release();
}

void GameObject::InitPhysx(physx::PxVec3 halfExtent, physx::PxVec3 pos, physx::PxScene* scene, physx::PxPhysics* gPhysics, bool dynamic) {
	physx::PxMaterial* gMaterial = gPhysics->createMaterial(1.f, 0, 0);
	physx::PxShape* shape = gPhysics->createShape(physx::PxBoxGeometry(halfExtent), *gMaterial);

	physx::PxTransform transform(pos);
	if (dynamic) pxbody_ = gPhysics->createRigidDynamic(transform);
	else pxbody_ = gPhysics->createRigidStatic(transform);
	pxbody_->attachShape(*shape);
	if (dynamic) physx::PxRigidBodyExt::updateMassAndInertia(*pxbody_->is<physx::PxRigidDynamic>(), 1.0f);
	scene->addActor(*pxbody_);
	
	shape->release();
	gMaterial->release();

	UpdatePhysx();
}

void GameObject::UpdatePhysx() {
	if (pxbody_) {
		gef::Matrix44 transform(physx::PxMat44(pxbody_->getGlobalPose()).front());
		set_transform(transform);
	}
}