#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "GL/glew.h"
#include "SDL3/SDL_opengl.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/vector_angle.hpp>
#include "PhysX/PxPhysicsAPI.h"
#define STB_IMAGE_IMPLEMENTATION
#ifdef WINDOWS
	#define STBI_WINDOWS_UTF8 //change this have ifdef WINDOWS 
#endif
#include "stb/stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "base_types.h"

class Shader;
class Camera;
class GLFramebuffer;
class Player;
class Platform;

SDL_Window* window;
SDL_GLContext glContext;
PxDefaultAllocator pAlloc;
PhysicsErrorCallback pError;
PxFoundation* pFoundation;
PxPhysics* pPhysics;
PxDefaultCpuDispatcher* pDispatcher;
PxScene* pScene;
PxMaterial* pMaterial;
PxPvd* pPvd;
GLFramebuffer* depthBuffer;
unsigned long long int eTime = 0;
unsigned long long dTime = 1;
Player* player;
Shader* shadowShader;
Platform* groundPlane;

Key k_Forward = Key(key_Forward);
Key k_Left = Key(key_Left);
Key k_Backward = Key(key_Backward);
Key k_Right = Key(key_Right);
Key k_Toggle = Key(key_PlatformToggle);
Key k_Jump = Key(key_Jump);

class GLFramebufferMS : public GLFramebuffer
{
protected:
	GLFramebuffer* multisampleBuffer;

public:
	GLFramebufferMS(int _width, int _height)
		:GLFramebuffer(_width, _height)
	{
		multisampleBuffer = new GLFramebuffer(_width, _height, true);
	}

	void Use()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, multisampleBuffer->GetFramebuffer());
	}

	void Downsample()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, multisampleBuffer->GetFramebuffer());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	}

	Texture* GetColorMS()
	{
		return multisampleBuffer->GetColor();
	}

	Texture* GetDepthMS()
	{
		return multisampleBuffer->GetDepth();
	}

	unsigned int GetFramebufferMS()
	{
		return multisampleBuffer->GetFramebuffer();
	}
};

class DrawableObject : public Object
{
protected:
	GLObject* renderObject;
	glm::mat4 model;

public:
	DrawableObject(glm::vec3 pos, glm::vec3 _rot, glm::vec3 scale, void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize)
		:Object(pos, _rot, scale) {
		renderObject = new GLObject(attribData, attribSize, indexData, indexSize);
	}

	DrawableObject(glm::vec3 pos, glm::vec3 _rot, glm::vec3 scale, void* attribData, unsigned int attribSize)
		:Object(pos, _rot, scale) {
		renderObject = new GLObject(attribData, attribSize);
	}

	DrawableObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize)
		:Object(pos, _rot, scale) {
		renderObject = new GLObject(attribData, attribSize, indexData, indexSize);
	}

	DrawableObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, void* attribData, unsigned int attribSize)
		:Object(pos, _rot, scale) {
		renderObject = new GLObject(attribData, attribSize);
	}

	DrawableObject(const DrawableObject& other)
	{
		renderObject = new GLObject(*other.renderObject);
		pos = other.pos;
		rot = other.rot;
		scal = other.scal;
	}

	DrawableObject()
	{
		renderObject = nullptr;
	}

	void InitializeRenderObject(void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize)
	{
		renderObject = new GLObject(attribData, attribSize, indexData, indexSize);
	}

	void InitializeRenderObject(void* attribData, unsigned int attribSize)
	{
		renderObject = new GLObject(attribData, attribSize);
	}

	glm::mat4 CalculateModel()
	{
		glm::mat4 translate = glm::translate(glm::mat4(1.00f), pos);
		glm::mat4 rotate = glm::mat4_cast(rot);
		glm::mat4 scale = glm::scale(glm::mat4(1.00f), scal);
		return translate * rotate * scale;
	}

	virtual void Draw()
	{
		model = CalculateModel(); //get updated model mat
		unsigned int currentProgram = 0; //init
		glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&currentProgram)); //get currently active program
		glUniformMatrix4fv(glGetUniformLocation(currentProgram, "model"), 1, false, glm::value_ptr(model)); //set that program's model uniform to our model mat
		renderObject->Draw(); //draw
	}

	virtual void Draw(Shader* shader)
	{
		shader->Use(); //make sure the passed shader is active
		model = CalculateModel(); //get the updated model matrix
		glUniformMatrix4fv(glGetUniformLocation(shader->GetProgram(), "model"), 1, false, glm::value_ptr(model)); //update the uniform in the shader to new matrix
		renderObject->Draw(); //draw
	}
};

class Mesh: public DrawableObject
{
protected:
	bool complete = false;
	glm::vec3 parentPosition; //this is a misnomer, as it is actually the sum of the parent positions, same for rotation and scale
	glm::quat parentRotation;
	glm::vec3 parentScale;

public:
	Mesh(aiMesh* mesh, aiMatrix4x4 globalTransform)
	{
		glm::mat4 transform = FromAssimpMat(globalTransform);//get global transform
		aiVector3f globalPos;
		aiQuaternion globalRot;
		aiVector3f globalScale;
		globalTransform.Decompose(globalScale, globalRot, globalPos);
		unsigned int numVerts = mesh->mNumVertices;
		if (numVerts > 0)
		{
			Vertex* vertices = new Vertex[numVerts];
			for (unsigned int vert = 0; vert < numVerts; vert++)
			{
				Vertex vertex = Vertex{ FromAssimpVec(mesh->mVertices[vert]),
					FromAssimpVec(mesh->mNormals[vert]),
					glm::vec2(mesh->mTextureCoords[0][vert].x, mesh->mTextureCoords[0][vert].y) };
				vertices[vert] = vertex;
			}
			unsigned int numFaces = mesh->mNumFaces; //get number of faces
			unsigned int* faces = new unsigned int[numFaces * 3]; //3 indices per face
			for (unsigned int face = 0; face < numFaces; face++) //loop over faces
			{
				aiFace currFace = mesh->mFaces[face];
				if (currFace.mNumIndices == 3) //make sure its a triangle
				{
					faces[face * 3] = currFace.mIndices[0]; //set each index
					faces[face * 3 + 1] = currFace.mIndices[1];
					faces[face * 3 + 2] = currFace.mIndices[2];
				}
				else
				{
					delete[] faces;
					std::cout << "Error: Failed to create mesh indices!: Face not a triangle! \n";
					return; //fail not a triangle
				}
			}
			//init the DrawableObject
			DrawableObject::InitializeRenderObject(vertices, numVerts * sizeof(Vertex), faces, numFaces * 3 * sizeof(unsigned int));
			//init the drawable object pos rot scale
			parentPosition = FromAssimpVec(globalPos);
			parentRotation = FromAssimpQuat(globalRot);
			//parentRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
			parentScale = FromAssimpVec(globalScale);
			DrawableObject::SetPosition(parentPosition); //set position from matrix
			DrawableObject::SetRotation(parentRotation); //set rotation from matrix
			DrawableObject::SetScale(parentScale); //set scale from matrix

			delete[] vertices; //cleanup
			delete[] faces; //cleanup
			complete = true;
		}
		else
		{
			std::cout << "Error: Failed to create mesh! No vertices!\n";
			return; //fail (no verts)
		}
	}
	
	Mesh(const Mesh& other)
		:DrawableObject(other)
	{
		complete = other.complete;
		parentPosition = other.parentPosition;
		parentRotation = other.parentRotation;
		parentScale = other.parentScale;
	}

	bool IsComplete()
	{
		return complete;
	}

	void SetPosition(glm::vec3 position)
	{
		Object::SetPosition(position + parentPosition);
	}

	void SetRotation(glm::quat rotation)
	{
		Object::SetRotation(rotation * parentRotation);
	}

	void SetScale(glm::vec3 scale)
	{
		Object::SetScale(scale * parentScale);
	}
};

class Model: public Object
{
protected:
	Mesh** meshes = nullptr;
	unsigned int numMeshes = 0;

public:
	Model(const Model&) = delete;
	Model(const char* path, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scale)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_CalcTangentSpace | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals); //load scene
		if (scene == nullptr)
		{
			std::cout << "Failed to make Model: Failed to load Scene at " << path << "\n";
			return; //fail
		}
		aiNode* node = scene->mRootNode;
		if (node->mNumChildren > 0) //if root node has some children (ie not empty scene)
		{
			aiMatrix4x4 transform = node->mTransformation;
			glm::mat4 tempTransform = FromAssimpMat(transform);
			tempTransform = glm::translate(tempTransform, _pos) * glm::mat4_cast(_rot) * glm::scale(tempTransform, _scale);
			transform = FromGLMMat(tempTransform);

			numMeshes = scene->mNumMeshes; //get num of scene meshes
			if (numMeshes > 0) //if there are some meshes in the scene
			{
				meshes = new Mesh* [numMeshes]; //allocate
			}
			else
			{
				std::cout << "Failed to make Model: No meshes in scene! " << path << "\n";
				return; //fail no meshes
			}

			TraverseNode(node, scene, node->mTransformation); //start processing the scene
			Model::SetScale(_scale); //apply inital pos rot scale
			Model::SetRotation(_rot);
			Model::SetPosition(_pos);
		}
		else
		{
			std::cout << "Failed to make Model: No child nodes in scene! " << path << "\n";
			return; //fail empty scene
		}
	}

	Model(Model& other, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scale) {
		numMeshes = other.numMeshes;
		meshes = new Mesh* [numMeshes];
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i] = new Mesh(*other.meshes[i]);
		}
		//initally position this model
		Model::SetPosition(_pos);
		Model::SetRotation(_rot);
		Model::SetScale(_scale);
	}

	~Model()
	{
		if (numMeshes > 0 && meshes != nullptr)
		{
			for (unsigned int i = 0; i < numMeshes; i++)
			{
				delete meshes[i];
			}
		}
		delete[] meshes;
	}

	void TraverseNode(aiNode* node, const aiScene* scene, aiMatrix4x4 cumulativeTransform) //depthwise node search
	{
		aiMatrix4x4 transform = node->mTransformation * cumulativeTransform;
		//process node
		for (unsigned int meshNum = 0; meshNum < node->mNumMeshes; meshNum++) //loop over meshes on node
		{
			Mesh* mesh = new Mesh(scene->mMeshes[node->mMeshes[meshNum]], transform); //create a Mesh from the aiMesh
			meshes[meshNum] = mesh; //store
		}

		//traverse children
		for (unsigned int i = 0; i < node->mNumChildren; i++) //depthwise traversal
		{
			TraverseNode(node->mChildren[i], scene, transform); //interate
		}
	}

	void Draw()
	{
		if (meshes != nullptr)
		{
			for (unsigned int i = 0; i < numMeshes; i++)
			{
				meshes[i]->Draw();
			}
		}
	}

	void Draw(Shader* shader)
	{
		if (meshes != nullptr)
		{
			for (unsigned int i = 0; i < numMeshes; i++)
			{
				meshes[i]->Draw(shader);
			}
		}
	}

	Mesh** GetMeshes()
	{
		return meshes;
	}

	const unsigned int GetNumMeshes()
	{
		return numMeshes;
	}

	void Move(glm::vec3 amt)
	{
		Object::Move(amt);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->Move(amt);
		}
	}

	void Scale(glm::vec3 amt)
	{
		Object::Scale(amt);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->Scale(amt);
		}
	}

	void Rotate(glm::quat amt)
	{
		Object::Rotate(amt);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->Rotate(amt);
		}
	}

	void SetRotation(glm::quat val)
	{
		Object::SetRotation(val);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->SetRotation(val);
		}
	}

	void SetPosition(glm::vec3 val)
	{
		Object::SetPosition(val);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->SetPosition(val);
		}
	}

	void SetScale(glm::vec3 val)
	{
		Object::SetScale(val);
		for (unsigned int i = 0; i < numMeshes; i++)
		{
			meshes[i]->SetScale(val);
		}
	}
};

class PhysicsObject: public Model
{
protected:
	PxRigidDynamic* pBody;
	PxMaterial* pMaterial;

	void CreatePBody(MaterialProperties materialProperties)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		PxShape* pShape = pPhysics->createShape(PxBoxGeometry(FromGLMVec(scal) / 2.00f), *pMaterial, true); //create the associated shape
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidDynamic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		PxRigidBodyExt::updateMassAndInertia(*pBody, 10.0f); //update the mass with density and new shape
		pBody->setAngularDamping(0.10f);
		pScene->addActor(*pBody); //add rigid body to scene
		PX_RELEASE(pShape); //the shape is now "contained" by the actor???? or maybe it schedules release and won't release now the ref count is > 0 ??
	}

public:
	PhysicsObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties,
		const char* path)
	:Model(path, pos, _rot, scale) {
		CreatePBody(materialProperties);
	}

	PhysicsObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties,
		Model* otherModel) : Model(*otherModel, pos, _rot, scale)
	{
		CreatePBody(materialProperties);
	}

	~PhysicsObject()
	{
		pScene->removeActor(*pBody); //remove from the scene
		PX_RELEASE(pBody); //free the memory
	}

	void Update()
	{
		PxTransform transform = pBody->getGlobalPose(); //get the current "pose"
		Model::SetPosition(FromPxVec(transform.p)); //update the model position
		Model::SetRotation(glm::quat(transform.q.w, transform.q.x, transform.q.y, transform.q.z)); //apply new rotation
	}

	virtual void Move(glm::vec3 amt)
	{
		Model::Move(amt);
		PxQuat _rot = pBody->getGlobalPose().q;
		pBody->setGlobalPose(PxTransform(FromGLMVec(pos), _rot));
	}

	virtual void Rotate(glm::quat _quat)
	{
		Model::Rotate(_quat);
		PxVec3 _pos = pBody->getGlobalPose().p;
		pBody->setGlobalPose(PxTransform(_pos, FromGLMQuat(rot)));
	}

	virtual void Scale(glm::vec3 amt)
	{
		Model::Scale(amt);
	}
};

class StaticObject : public Model
{
protected:
	PxRigidStatic* pBody;
	PxShape* pShape;
	PxMaterial* pMaterial;

	void CreatePBody(MaterialProperties materialProperties)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pShape = pPhysics->createShape(PxBoxGeometry(FromGLMVec(scal) / 2.00f), *pMaterial, true); //create the associated shape
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidStatic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		pScene->addActor(*pBody); //add rigid body to scene
	}

public:
	StaticObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties,
		const char* path)
		:Model(path, pos, _rot, scale) {
		CreatePBody(materialProperties);
	}

	StaticObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties,
		Model* otherModel) : Model(*otherModel, pos, _rot, scale)
	{
		CreatePBody(materialProperties);
	}

	~StaticObject()
	{
		pScene->removeActor(*pBody); //remove from the scene
		PX_RELEASE(pBody); //free the memory
		PX_RELEASE(pShape);
	}

	virtual void Move(glm::vec3 amt)
	{
		Model::Move(amt);
		PxQuat _rot = pBody->getGlobalPose().q;
		pBody->setGlobalPose(PxTransform(FromGLMVec(pos), _rot));
	}

	virtual void Rotate(glm::quat _quat)
	{
		Model::Rotate(_quat);
		PxVec3 _pos = pBody->getGlobalPose().p;
		pBody->setGlobalPose(PxTransform(_pos, FromGLMQuat(rot)));
	}

	virtual void Scale(glm::vec3 amt)
	{
		Model::Scale(amt);
	}
};

class Player : public PhysicsObject
{
protected:
	glm::vec2 moveDir = glm::vec2(0.00f);
	float moveSpeed = 6.67f; //max movement speed
	float moveTime = 0.50f; //time to get to moveSpeed
	float jumpForce = 666.67f;
	bool shouldJump = false;

public:
	Player(glm::vec3 _pos, glm::quat _rot, Model* model)
		:PhysicsObject(_pos, _rot, glm::vec3(1.00f, 1.00f, 1.00f), MaterialProperties{ 0.50f, 0.40f, 0.30f}, model)
	{
		pBody->setRigidDynamicLockFlags(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);
	}
	Player(glm::vec3 _pos, glm::quat _rot, const char* path)
		:PhysicsObject(_pos, _rot, glm::vec3(1.00f, 1.00f, 1.00f), MaterialProperties{ 0.50f, 0.40f, 0.30f }, path)
	{
		pBody->setRigidDynamicLockFlags(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z);
	}

	void MoveDir(glm::vec2 dir)
	{
		moveDir += dir;
	}

	void Jump()
	{
		shouldJump = true;
	}

	void Update(float stepTime)
	{
		PhysicsObject::Update();
		//move
		glm::vec3 worldV = glm::rotate(glm::quat(0.00f, 0.00f, 0.00f, 1.00f), -mainCamera->GetAngle() + PI, glm::vec3(0.00f, 1.00f, 0.00f))
			* glm::vec3(moveDir.x, 0.00f, moveDir.y); //get the move direction relative to the camera's forward direction
		glm::vec2 v = glm::vec2(worldV.x * moveSpeed, worldV.z * moveSpeed); //get target move speed
		glm::vec3 current = FromPxVec(pBody->getLinearVelocity()); //current move speed
		glm::vec2 u = glm::vec2(current.x, current.z); //get current move speed
		glm::vec2 f = pBody->getMass() * (v - u) / moveTime; //calculate the force
		//rotate
		if (glm::length(moveDir) > 0.00f) //if we are actually moving
		{
			glm::vec3 pForward = rot * glm::vec3(0.00f, 0.00f, 1.00f); //get local forward in global space
			glm::vec2 playerForward = glm::normalize(glm::vec2(pForward.x, pForward.z)); //normalize without y component (removes pitch)
			float angle = glm::orientedAngle(glm::vec3(playerForward.x, 0.00f, playerForward.y), glm::normalize(worldV), glm::vec3(0.00f, 1.00f, 0.00f)); //get angle between global forward and global move direction
			if (glm::abs(angle) > 0.05f) //if the angle to rotate is greater than a tolerance
			{ //this tolerance stops overshooting with the else statement, and also stops infinite forces (A = ../2*angle) where an angle of 0 gives an A of infinity
				constexpr float w = 2.00f * PI; //max angular velocity of 360 deg per sec (250ms for a 90 degree turn) about the y axis
				float w0 = pBody->getAngularVelocity().y; //current angular velocity about the y axis
				float A = (pow(w, 2) - pow(w0, 2)) / 2.00f * angle; //A = (v^2 - u^2)/2s
				pBody->addTorque(PxVec3(0.00f, A * pBody->getMass(), 0.00f)); //F = m*a
			}
			else //if we are (basically) at the right angle
			{
				PxVec3 old = pBody->getAngularVelocity();
				pBody->setAngularVelocity(PxVec3(old.x, 0.00f, old.z)); //stop rotating (this stops rotation from overshooting)
			}
		}
		pBody->addForce(PxVec3(f.x, 0.00f, f.y), PxForceMode::eFORCE); //apply movement force

		if (shouldJump)
		{
			pBody->addForce(PxVec3(0.00f, pBody->getMass() * jumpForce, 0.00f));
			shouldJump = false;
		}
	}
};

class Platform
	: public StaticObject
{
protected:
	bool enabled = true;

public:
	Platform(glm::vec3 _pos, glm::vec3 _scale, Model* copyModel)
		:StaticObject(_pos, glm::quat(1.00f, 0.00f, 0.00f, 0.00f), _scale, MaterialProperties{ 0.5f, 0.4f, 0.5f }, copyModel) {
	}

	void Enable()
	{
		if (!enabled)
		{
			pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
			enabled = true;
		}
	}

	void Disable()
	{
		if (enabled)
		{
			pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			enabled = false;
		}
	}
};

class Light : public Object
{
};

class Sun: public Light
{
protected:
	float strength;
	GLFramebuffer* shadowBuffer;
	const int shadowMapSize = 4096;

public:
	Sun(glm::vec3 _pos)
	{
		pos = _pos;
		strength = 100.00f;
		shadowBuffer = new GLFramebuffer(shadowMapSize, shadowMapSize);
		shadowBuffer->GetDepth()->Use(2);
	}

	glm::mat4 CalculateCombinedMatrix()
	{
		const float nearPlane = 0.10f;
		const float farPlane = 15.00f;
		glm::mat4 projection = glm::ortho(-10.00f, 10.00f, -10.00f, 10.00f, nearPlane, farPlane);
		glm::mat4 view = glm::lookAt(pos, glm::vec3(0.00f), glm::vec3(0.00f, 1.00f, 0.00f));
		return projection * view;
	}

	void StartShadowPass(Shader* shader)
	{
		glCullFace(GL_FRONT);
		//glEnable(GL_POLYGON_OFFSET_FILL); //should fix the shadow aliasing //re-enable if shadow aliasing appears again
		//glPolygonOffset(1.f, 1); //other stuff https://learn.microsoft.com/en-gb/windows/win32/dxtecharts/common-techniques-to-improve-shadow-depth-maps
		shader->Use();
		shader->SetUniforms(CalculateCombinedMatrix(), pos);
		glViewport(0, 0, shadowMapSize, shadowMapSize);
		shadowBuffer->Use();
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void EndShadowPass()
	{
		glCullFace(GL_BACK);
		//glDisable(GL_POLYGON_OFFSET_FILL);
		shadowBuffer->GetDepth()->Use(2);
		glViewport(0, 0, (int)screenWidth, (int)screenHeight);
	}
};

void KeyDown(SDL_KeyboardEvent key)
{
	switch (key.key)
	{
	case (SDLK_ESCAPE):
		quit(0);
		break;
	case(key_Forward):
		if (k_Forward.Press())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(0.00f, 1.00f));
		}
		break;
	case(key_Left):
		if (k_Left.Press())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(-1.00f, 0.00f));
		}
		break;
	case(key_Backward):
		if (k_Backward.Press())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(0.00f, -1.00f));
		}
		break;
	case(key_Right):
		if (k_Right.Press())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(1.00f, 0.00f));
		}
		break;
	case(key_PlatformToggle):
		if (k_Toggle.Press())
		{
			TogglePlatforms();
		}
		break;
	case(key_Jump):
		if (k_Jump.Press())
		{
			if (player != nullptr)
			{
				player->Jump();
			}
		}
		break;
	}
}

void KeyUp(SDL_KeyboardEvent key)
{
	switch (key.key)
	{
	case(key_Forward):
		if (k_Forward.Release())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(0.00f, -1.00f));
		}
		break;
	case(key_Left):
		if (k_Left.Release())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(1.00f, 0.00f));
		}
		break;
	case(key_Backward):
		if (k_Backward.Release())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(0.00f, 1.00f));
		}
		break;
	case(key_Right):
		if (k_Right.Release())
		{
			if (player != nullptr)
				player->MoveDir(glm::vec2(-1.00f, 0.00f));
		}
		break;
	case(key_PlatformToggle):
		k_Toggle.Release();
		break;
	case(key_Jump):
		k_Jump.Release();
		break;
	}
}

void MouseMoved()
{
	const glm::vec2 radPerScreen = sensitivity * PI / glm::vec2(screenWidth, screenHeight);
	float deltaX, deltaY;
	SDL_GetRelativeMouseState(&deltaX, &deltaY);
	if (mainCamera != nullptr)
	{
		mainCamera->Angle(-deltaX * radPerScreen.x);
		mainCamera->Inclination(deltaY * radPerScreen.y);
	}
}

void MouseWheel(SDL_MouseWheelEvent e)
{
	if (mainCamera != nullptr)
	{
		mainCamera->Distance(-e.y * 0.50f);
	}
}

int quit(int code)
{
	SDL_Quit();
	if (pScene != nullptr)
		PX_RELEASE(pScene);
	if (pDispatcher != nullptr)
		PX_RELEASE(pDispatcher);
	if (pPhysics != nullptr)
		PX_RELEASE(pPhysics);
	if (pPvd != nullptr)
		PX_RELEASE(pPvd);
	if (pFoundation != nullptr)
		PX_RELEASE(pFoundation);

	exit(code);
}

void TogglePlatforms()
{
	if (platformToggle)
	{
		std::for_each(APlatforms.begin(), APlatforms.end(), [&](Platform* platform) { platform->Enable(); });
		std::for_each(BPlatforms.begin(), BPlatforms.end(), [&](Platform* platform) { platform->Disable(); });
		platformToggle = false;
	}
	else
	{
		std::for_each(APlatforms.begin(), APlatforms.end(), [&](Platform* platform) { platform->Disable(); });
		std::for_each(BPlatforms.begin(), BPlatforms.end(), [&](Platform* platform) { platform->Enable(); });
		platformToggle = true;
	}
}

void LoadLevelTest()
{

}