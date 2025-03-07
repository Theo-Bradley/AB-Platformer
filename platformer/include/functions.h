#include "defines.h"

int quit(int code);
void KeyDown(SDL_KeyboardEvent key);
void KeyUp(SDL_KeyboardEvent key);
void MouseMoved();
void MouseWheel(SDL_MouseWheelEvent e);

void PrintGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cout << "OpenGL Error: " << std::hex << "0x" << err << std::dec << "\n";
	}
}

class PhysicsErrorCallback : public PxErrorCallback
{
public:
	virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		// error processing implementation
		std::cout << std::dec << "PhysX Error: " << code << ": " << message << " in " << file << " at " << line << "\nPress Enter to exit!";
		std::getchar();
		switch (code)
		{
		case PxErrorCode::eABORT:
			exit(-1);
		case PxErrorCode::eINTERNAL_ERROR:
			exit(-1);
		case PxErrorCode::eOUT_OF_MEMORY:
			exit(-1);
		case PxErrorCode::eINVALID_OPERATION:
			exit(-2);
		case PxErrorCode::eINVALID_PARAMETER:
			exit(-2);
		}
	}
};

glm::vec3 FromAssimpVec(aiVector3D _vec)
{
	return glm::vec3(_vec.x, _vec.y, _vec.z);
}

glm::vec2 FromAssimpVec(aiVector2D _vec)
{
	return glm::vec2(_vec.x, _vec.y);
}

glm::vec3 FromPxVec(PxVec3 from)
{
	return glm::vec3(from.x, from.y, from.z);
}

PxVec3 FromGLMVec(glm::vec3 from)
{
	return PxVec3(from.x, from.y, from.z);
}

glm::quat FromPxQuat(PxQuat from)
{
	return glm::quat(from.w, from.x, from.y, from.z);
}

PxQuat FromGLMQuat(glm::quat from)
{
	return PxQuat(from.x, from.y, from.z, from.w);
}

glm::quat FromAssimpQuat(aiQuaternion from)
{
	return glm::quat(from.w, from.x, from.y, from.z);
}

//effectively transposes the aiMatrix4x4 into a glm::mat4 as one is column major the other is row major
glm::mat4 FromAssimpMat(aiMatrix4x4 _mat)
{
	glm::mat4 result = glm::mat4(0.0f);
	result[0][0] = _mat[0][0]; //result[c][r] = _mat[r][c]
	result[0][1] = _mat[1][0];
	result[0][2] = _mat[2][0];
	result[0][3] = _mat[3][0];
	result[1][0] = _mat[0][1];
	result[1][1] = _mat[1][1];
	result[1][2] = _mat[2][1];
	result[1][3] = _mat[3][1];
	result[2][0] = _mat[0][2];
	result[2][1] = _mat[1][2];
	result[2][2] = _mat[2][2];
	result[2][3] = _mat[3][2];
	result[3][0] = _mat[0][3];
	result[3][1] = _mat[1][3];
	result[3][2] = _mat[2][3];
	result[3][3] = _mat[3][3];
	return result;
}

//effectively transposes the glm::mat4 into an aiMatrix4x4 as one is column major the other is row major
aiMatrix4x4 FromGLMMat(glm::mat4 _mat)
{
	aiMatrix4x4 result = aiMatrix4x4();
	result[0][0] = _mat[0][0]; //result[c][r] = _mat[r][c]
	result[0][1] = _mat[1][0];
	result[0][2] = _mat[2][0];
	result[0][3] = _mat[3][0];
	result[1][0] = _mat[0][1];
	result[1][1] = _mat[1][1];
	result[1][2] = _mat[2][1];
	result[1][3] = _mat[3][1];
	result[2][0] = _mat[0][2];
	result[2][1] = _mat[1][2];
	result[2][2] = _mat[2][2];
	result[2][3] = _mat[3][2];
	result[3][0] = _mat[0][3];
	result[3][1] = _mat[1][3];
	result[3][2] = _mat[2][3];
	result[3][3] = _mat[3][3];
	return result;
}

void TogglePlatforms();

PxFilterFlags DefaultFilterShader
(PxFilterObjectAttributes attributes0, PxFilterData filterData0,
	PxFilterObjectAttributes attributes1, PxFilterData filterData1,
	PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
	if (PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1)) //if trigger
	{
		pairFlags = PxPairFlag::eTRIGGER_DEFAULT; //let physx know at least one actor in pair is a trigger
		return PxFilterFlag::eDEFAULT; //allow collision
	}

	pairFlags = PxPairFlag::eCONTACT_DEFAULT; //let physx know to do normal collision things

	pairFlags |= PxPairFlag::eNOTIFY_TOUCH_PERSISTS; //lett physx know we want to run the callback whenever the pair are touching
	return PxFilterFlag::eDEFAULT; //allow collision
}

void IncreaseScore(int amt);

void UnloadMainMenu();
void LoadLevel01();
void UnloadLevel01();

class PhysicsModel;
PhysicsModel* LoadPhysicsModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, const char* colliderPath, glm::vec3 colliderOffset);