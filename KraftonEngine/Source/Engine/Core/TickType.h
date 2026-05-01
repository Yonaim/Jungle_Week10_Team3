#pragma once

// TickType.h
// TG_PrePhysics : Input process, Game logic, AI etc.. 
// TG_DuringPhysics : Physics simulation
// TG_PostPhysics : excute after physics
// TG_PostUpdateWork  : Camera update, Animation update , etc.....
enum class ETickType
{
	TG_PrePhysics,
	TG_DuringPhysics,
	TG_PostPhysics,
	TG_PostUpdateWork,
};