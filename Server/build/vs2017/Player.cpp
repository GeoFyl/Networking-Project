#include "Player.h"

void Player::Init(PrimitiveBuilder* builder, physx::PxScene* scene, physx::PxPhysics* physics, int ID)
{
	playerID = ID;
	set_mesh(builder->CreateBoxMesh(gef::Vector4(0.5f, 0.75f, 0.5f)));
	InitPhysx(physx::PxVec3(0.5f, 0.75f, 0.5f), physx::PxVec3(ID, 1.25f, 2), scene, physics, true);
	GetPxBody()->setRigidDynamicLockFlags(physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y);
}

void Player::rotate(float angle)
{
	physx::PxTransform t = GetPxBody()->getGlobalPose();

	t.q *= physx::PxGetRotYQuat(angle);

	forwardVec = -t.q.getBasisVector2();
	rightVec = t.q.getBasisVector0();

	GetPxBody()->setGlobalPose(t);
}

void Player::setRotation(float angle)
{
	physx::PxTransform t = GetPxBody()->getGlobalPose();

	t.q = physx::PxGetRotYQuat(angle);

	forwardVec = -t.q.getBasisVector2();
	rightVec = t.q.getBasisVector0();

	GetPxBody()->setGlobalPose(t);
}

float Player::getRotation()
{
	physx::PxQuat quat = GetPxBody()->getGlobalPose().q;
	float angle = quat.getAngle();
	if (quat.y >= 0) return angle;
	else return -angle;
}

void Player::setPosition(physx::PxVec3 pos)
{
	physx::PxTransform t = GetPxBody()->getGlobalPose();
	t.p = pos;
	GetPxBody()->setGlobalPose(t);
}

void Player::setVelocity(physx::PxVec3 newVelocity)
{
	GetPxBody()->setLinearVelocity(newVelocity);
}

physx::PxVec3 Player::getForwardVec()
{
	return forwardVec;
}

physx::PxVec3 Player::getRightVec()
{
	return rightVec;
}

physx::PxRigidDynamic* Player::GetPxBody()
{
	return pxbody_->is<physx::PxRigidDynamic>();
}

