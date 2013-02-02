/*
 * EMotion FX 2 - Character Animation System
 * Copyright (c) 2001-2004 Mystic Game Development - http://www.mysticgd.com
 * All Rights Reserved.
 */

#include "NodeCollisionSystem.h"


namespace EMotionFX
{

// constructor
NodeCollisionSystem::NodeCollisionSystem(Node* node)
{
	assert(node != NULL);
	mNode = node;
}


// destructor
NodeCollisionSystem::~NodeCollisionSystem()
{
}


} // namespace EMotionFX