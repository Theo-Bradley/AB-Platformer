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
#include "glm/gtc/quaternion.hpp"
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
class StaticModel;
class DustCloud;
class StaminaBar;
class Texture;

SDL_Window* window;
SDL_GLContext glContext;
PxDefaultAllocator pAlloc;
PhysicsErrorCallback pError;
PxFoundation* pFoundation;
PxPhysics* pPhysics;
PxDefaultCpuDispatcher* pDispatcher;
PxScene* pScene;
PxMaterial* pMaterial;
GLFramebuffer* depthBuffer;
Shader* shadowShader;
StaticModel* groundPlane;
StaminaBar* stamBar;
DustCloud* playerCloud;
Texture* toggleTexture;
Shader* toggleShader;

Key k_Forward = Key(key_Forward);
Key k_Left = Key(key_Left);
Key k_Backward = Key(key_Backward);
Key k_Right = Key(key_Right);
Key k_Toggle = Key(key_PlatformToggle);
Key k_Jump = Key(key_Jump);
Key k_Sprint = Key(key_Sprint);

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
		//downsample the multisampled buffer into a regular one by blitting from the MS buffer to a regular buffer
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
	glm::vec3 color = DEFAULT_COLOR;

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
		color = other.color;
		pos = other.pos;
		rot = other.rot;
		scal = other.scal;
	}

	DrawableObject()
	{
		renderObject = nullptr;
	}

	void InitializeRenderObject(void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize, int attribOffset = 0)
	{
		renderObject = new GLObject(attribData, attribSize, indexData, indexSize, attribOffset);
	}

	void InitializeRenderObject(void* attribData, unsigned int attribSize, int attribOffset = 0)
	{
		renderObject = new GLObject(attribData, attribSize, attribOffset);
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
		glUniform3fv(glGetUniformLocation(currentProgram, "baseColor"), 1, glm::value_ptr(color)); //set the color in the shader
		renderObject->Draw(); //draw
	}

	virtual void Draw(Shader* shader)
	{
		shader->Use(); //make sure the passed shader is active
		model = CalculateModel(); //get the updated model matrix
		glUniformMatrix4fv(glGetUniformLocation(shader->GetProgram(), "model"), 1, false, glm::value_ptr(model)); //update the uniform in the shader to new matrix
		glUniform3fv(glGetUniformLocation(shader->GetProgram(), "baseColor"), 1, glm::value_ptr(color)); //set the color in the shader
		renderObject->Draw(); //draw
	}

	GLObject* GetGLObject()
	{
		return renderObject;
	}

	void SetColor(glm::vec3 _color)
	{
		color = _color;
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
	Mesh(aiMesh* mesh, aiMaterial* material, aiMatrix4x4 globalTransform)
	{
		aiVector3f globalPos;
		aiQuaternion globalRot;
		aiVector3f globalScale;
		globalTransform.Decompose(globalScale, globalRot, globalPos); //get pos rot and scal from the globalTransform
		unsigned int numVerts = mesh->mNumVertices;
		if (numVerts > 0) //if it has some verts
		{
			Vertex* vertices = new Vertex[numVerts]; //alloc
			for (unsigned int vert = 0; vert < numVerts; vert++) //loop over each vert
			{
				Vertex vertex = Vertex{ FromAssimpVec(mesh->mVertices[vert]),
					FromAssimpVec(mesh->mNormals[vert]),
					glm::vec2(mesh->mTextureCoords[0][vert].x, mesh->mTextureCoords[0][vert].y) }; //extract and store the info in a Vertex struct
				vertices[vert] = vertex; //store on the mesh
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
			if (material != nullptr) //if we have a material
			{
				aiColor3D diffuseCol;
				material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseCol); //get the diffuse color from the material
				color = glm::vec3(diffuseCol.r, diffuseCol.g, diffuseCol.b); //set the shader baseColor to diffuse color
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

	Mesh(aiMesh* mesh, aiMaterial* material, aiMatrix4x4 globalTransform, int attributeOffset)
	{
		aiVector3f globalPos;
		aiQuaternion globalRot;
		aiVector3f globalScale;
		globalTransform.Decompose(globalScale, globalRot, globalPos);
		unsigned int numVerts = mesh->mNumVertices;
		if (numVerts > 0) //if has some verts
		{
			Vertex* vertices = new Vertex[numVerts];
			for (unsigned int vert = 0; vert < numVerts; vert++) //loop over each vertex
			{
				Vertex vertex = Vertex{ FromAssimpVec(mesh->mVertices[vert]),
					FromAssimpVec(mesh->mNormals[vert]),
					glm::vec2(mesh->mTextureCoords[0][vert].x, mesh->mTextureCoords[0][vert].y) }; //extract and store the info in a Vertex struct
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
			if (material != nullptr) //if we have a material
			{
				aiColor3D diffuseCol;
				material->Get(AI_MATKEY_COLOR_DIFFUSE, diffuseCol); //get the diffuse color from the material
				color = glm::vec3(diffuseCol.r, diffuseCol.g, diffuseCol.b); //set the shader baseColor to diffuse color
			}
			//init the DrawableObject
			DrawableObject::InitializeRenderObject(vertices, numVerts * sizeof(Vertex), faces, numFaces * 3 * sizeof(unsigned int), attributeOffset);
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
	Model(const char* path, glm::vec3 _pos = glm::vec3(0.f), glm::quat _rot = glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3 _scale = glm::vec3(1.f))
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_GenSmoothNormals); //load scene
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

	Model(Model& other, glm::vec3 _pos = glm::vec3(0.f), glm::quat _rot = glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3 _scale = glm::vec3(1.f)) {
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
			if (meshes[node->mMeshes[meshNum]] != nullptr && scene->mMaterials != nullptr) //if we didn't already load this mesh
				meshes[node->mMeshes[meshNum]] = new Mesh(scene->mMeshes[node->mMeshes[meshNum]], scene->mMaterials[scene->mMeshes[node->mMeshes[meshNum]]->mMaterialIndex], transform); //create a Mesh from the aiMesh and store it
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

	void SetColor(glm::vec3 color)
	{
		if (meshes != nullptr)
		{
			for (unsigned int i = 0; i < numMeshes; i++)
			{
				meshes[i]->SetColor(color);
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

class TriangleMesh
{
public:
	PxVec3* verts;
	unsigned int size;

public:
	//This function is has no error checking, it assumes I knew what I was doing when creating the mesh and is used for loading single mesh colliders only
	TriangleMesh(const char* path)
	{
		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path,
			aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_GenSmoothNormals); //load scene
		aiMesh* mesh = scene->mMeshes[0]; //get first mesh only
		size = mesh->mNumVertices;
		verts = new PxVec3[size]; //alloc some verts
		for (unsigned int i = 0; i < size; i++) //loop over each vert
		{
			aiVector3D vertex = mesh->mVertices[i];
			verts[i] = PxVec3(vertex.x, vertex.y, vertex.z); //convert and store the vertex position as a PxVec3
		}
	}
};

class PhysicsObject: public Object
{
protected:
	PxRigidDynamic* pBody;
	PxMaterial* pMaterial;
	PxVec3 colliderOffset;
	PhysicsData pData;

	void CreatePBody(MaterialProperties materialProperties)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pData = PhysicsData { this, false, true, false, false};
		PxShape* pShape = pPhysics->createShape(PxBoxGeometry(FromGLMVec(scal) / 2.00f), *pMaterial, true); //create the associated shape
		colliderOffset = PxVec3(0.00f, 0.00f, 0.00f);
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidDynamic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		PxRigidBodyExt::updateMassAndInertia(*pBody, 10.0f); //update the mass with density and new shape
		pBody->setAngularDamping(0.10f);
		pBody->userData = &pData; //set the user data
		pScene->addActor(*pBody); //add rigid body to scene
		PX_RELEASE(pShape); //the shape is now "contained" by the actor???? or maybe it schedules release and won't release now the ref count is > 0 ??
	}

	void CreatePBody(MaterialProperties materialProperties, BoxCollider collider)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pData = PhysicsData{ this, false, true, false, false};
		PxShape* pShape = pPhysics->createShape(PxBoxGeometry(collider.size / 2.00f), *pMaterial, true); //create the associated shape
		colliderOffset = collider.center;
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidDynamic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		PxRigidBodyExt::updateMassAndInertia(*pBody, 10.0f); //update the mass with density and new shape
		pBody->setAngularDamping(0.10f);
		pBody->userData = &pData; //set the user data
		pScene->addActor(*pBody); //add rigid body to scene
		PX_RELEASE(pShape); //the shape is now "contained" by the actor???? or maybe it schedules release and won't release now the ref count is > 0 ??
	}

	void CreatePBody(MaterialProperties materialProperties, PxConvexMeshGeometry collider, glm::vec3 _colliderOffset)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pData = PhysicsData{ this, false, true, false, false};
		PxShape* pShape = pPhysics->createShape(collider, *pMaterial, true); //create the associated shape
		colliderOffset = PxVec3(_colliderOffset.x, _colliderOffset.y, _colliderOffset.z);
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidDynamic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		PxRigidBodyExt::updateMassAndInertia(*pBody, 10.0f); //update the mass with density and new shape
		pBody->setAngularDamping(0.10f);
		pBody->userData = &pData;
		pScene->addActor(*pBody); //add rigid body to scene
		PX_RELEASE(pShape); //the shape is now "contained" by the actor???? or maybe it schedules release and won't release now the ref count is > 0 ??
	}

public:
	PhysicsObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties)
	:Object(pos, _rot, scale) {
		CreatePBody(materialProperties);
	}

	PhysicsObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, BoxCollider collider, MaterialProperties materialProperties)
		:Object(pos, _rot, scale) {
		CreatePBody(materialProperties, collider);
	}

	PhysicsObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, PxConvexMeshGeometry collider, glm::vec3 _colliderOffset, MaterialProperties materialProperties)
		:Object(pos, _rot, scale) {
		CreatePBody(materialProperties, collider, _colliderOffset);
	}

	~PhysicsObject()
	{
		pBody->userData = nullptr;
		pScene->removeActor(*pBody); //remove from the scene
		PX_RELEASE(pBody); //free the memory
	}

	virtual void Update()
	{
		PxTransform transform = pBody->getGlobalPose(); //get the current "pose"
		Object::SetPosition(FromPxVec(transform.p - colliderOffset)); //update the model position
		Object::SetRotation(glm::quat(transform.q.w, transform.q.x, transform.q.y, transform.q.z)); //apply new rotation
	}

	PhysicsData* GetPData()
	{
		return &pData;
	}

	virtual void Move(glm::vec3 amt)
	{
		Object::Move(amt);
		PxQuat _rot = pBody->getGlobalPose().q;
		pBody->setGlobalPose(PxTransform(FromGLMVec(pos) + colliderOffset, _rot));
	}

	virtual void Rotate(glm::quat _quat)
	{
		Object::Rotate(_quat);
		PxVec3 _pos = pBody->getGlobalPose().p;
		pBody->setGlobalPose(PxTransform(_pos, FromGLMQuat(rot)));
	}

	virtual void Scale(glm::vec3 amt)
	{
		Object::Scale(amt);
	}
};

class StaticObject : public Object
{
protected:
	PxRigidStatic* pBody;
	PxShape* pShape;
	PxMaterial* pMaterial;
	PhysicsData pData;

	void CreatePBody(MaterialProperties materialProperties)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pData = PhysicsData{ this, false, false, false, false};
		pShape = pPhysics->createShape(PxBoxGeometry(FromGLMVec(scal) / 2.00f), *pMaterial, true); //create the associated shape
		if (materialProperties.isTrigger)
		{
			pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false); //disable classic collisions
			pShape->setFlag(PxShapeFlag::eTRIGGER_SHAPE, true); //enable trigger collisions
		}
		PxTransform transform = PxTransform(FromGLMVec(pos), FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidStatic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		pBody->userData = &pData; //set the user data
		pScene->addActor(*pBody); //add rigid body to scene
	}

	void CreatePBody(MaterialProperties materialProperties, BoxCollider collider)
	{
		pMaterial = pPhysics->createMaterial(materialProperties.staticFriction, materialProperties.dynamicFriction, materialProperties.restitution); //create our physics mat
		pData = PhysicsData{ this, false, false, false, false};
		pShape = pPhysics->createShape(PxBoxGeometry(collider.size / 2.00f), *pMaterial, true); //create the associated shape
		PxTransform transform = PxTransform(FromGLMVec(pos) + collider.center, FromGLMQuat(rot)); //create the starting transform from the class rot and xyz
		pBody = pPhysics->createRigidStatic(transform); //create the dynamic rigidbody
		pBody->attachShape(*pShape); //attatch the shape to it
		pBody->userData = &pData; //set the user data
		pScene->addActor(*pBody); //add rigid body to scene
		PX_RELEASE(pShape); //the shape is now "contained" by the actor???? or maybe it schedules release and won't release now the ref count is > 0 ??
	}

public:
	StaticObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, MaterialProperties materialProperties)
		:Object(pos, _rot, scale) {
		CreatePBody(materialProperties);
	}

	StaticObject(glm::vec3 pos, glm::quat _rot, glm::vec3 scale, BoxCollider collider, MaterialProperties materialProperties)
		: Object(pos, _rot, scale)
	{
		CreatePBody(materialProperties, collider);
	}

	~StaticObject()
	{
		pBody->userData = nullptr;
		pScene->removeActor(*pBody); //remove from the scene
		PX_RELEASE(pBody); //free the memory
		PX_RELEASE(pShape);
	}

	PhysicsData* GetPData()
	{
		return &pData;
	}

	PxShape* GetPShape()
	{
		return pShape;
	}

	virtual void SetPosition(glm::vec3 val)
	{
		Object::SetPosition(val);
		PxQuat _rot = pBody->getGlobalPose().q;
		pBody->setGlobalPose(PxTransform(FromGLMVec(pos), _rot));
	}

	virtual void SetRotation(glm::quat val)
	{
		Object::SetRotation(val);
		PxVec3 _pos = pBody->getGlobalPose().p;
		pBody->setGlobalPose(PxTransform(_pos, FromGLMQuat(rot)));
	}

	virtual void SetScale(glm::vec3 val)
	{
		Object::SetScale(val);
	}
};

class PhysicsModel : public PhysicsObject, public Model
{
public:
	PhysicsModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, MaterialProperties matProp)
		: PhysicsObject(_pos, _rot, _scal, matProp), Model(copyModel, _pos, _rot, _scal)
	{
	}

	PhysicsModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, BoxCollider collider, MaterialProperties matProp)
		: PhysicsObject(_pos, _rot, _scal, collider, matProp), Model(copyModel, _pos, _rot, _scal)
	{
	}

	PhysicsModel(const char* path, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, BoxCollider collider, MaterialProperties matProp)
		: PhysicsObject(_pos, _rot, _scal, collider, matProp), Model(path, _pos, _rot, _scal)
	{
	}

	PhysicsModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, MaterialProperties matProp, PxConvexMeshGeometry collider, glm::vec3 _colliderOffset)
		: PhysicsObject(_pos, _rot, _scal, collider, _colliderOffset, matProp), Model(copyModel, _pos, _rot, _scal)
	{
	}

	void Update() override
	{
		PhysicsObject::Update();
		Model::SetPosition(Object::pos);
		Model::SetRotation(Object::rot);
	}
};

class StaticModel: public StaticObject, public Model
{
public:
	StaticModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, MaterialProperties matProp)
		: StaticObject(_pos, _rot, _scal, matProp), Model(copyModel, _pos, _rot, _scal)
	{
	}

	StaticModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, BoxCollider collider, MaterialProperties matProp)
		: StaticObject(_pos, _rot, _scal, collider, matProp), Model(copyModel, _pos, _rot, _scal)
	{
	}
};

class AnimatedObject : public Object
{
protected:
	Model** frames;
	unsigned int currentFrame = 0;
	unsigned int nextFrame = 0;
	float startTime = 0.00f;
	unsigned int numFrames;
	Animation<float>* factor;

public:
	AnimatedObject(std::vector<std::string> paths, unsigned int _numFrames, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scale)
		:Object(_pos, _rot, _scale)
	{
		numFrames = _numFrames;
		frames = new Model * [numFrames];
		for (unsigned int i = 0; i < numFrames; i++) //loop over meshes
		{
			Model* frame = new Model(paths[i].c_str(), _pos, _rot, _scale); //create new model for each frame
			frames[i] = frame;
		}
		float factorFrames[2] = { 0.0f, 1.0f };
		factor = new Animation<float>(factorFrames, 2, 0.3f);
	}

	~AnimatedObject()
	{
		for (unsigned int i = 0; i < numFrames; i++)
		{
			delete frames[i];
		}
		delete[] frames;
		delete factor;
	}

	virtual void Draw(Shader* shader)
	{
		glUniform1f(glGetUniformLocation(shader->GetProgram(), "animFac"), factor->GetFrame());
		for (unsigned int i = 0; i < frames[currentFrame]->GetNumMeshes(); i++) //loop over each mesh
		{
			//pass in the next frames verts so the shader can blend them
			glBindVertexArray(frames[currentFrame]->GetMeshes()[i]->GetGLObject()->GetObject());
			frames[currentFrame]->GetMeshes()[i]->GetGLObject()->SetupAttributes(2, frames[nextFrame]->GetMeshes()[i]->GetGLObject()->GetAttribBuffer()->GetBuffer());
			frames[currentFrame]->GetMeshes()[i]->Draw();
		}
	}

	void Start()
	{
		factor->Start();
	}

	void Stop()
	{
		factor->Stop();
	}

	void Play()
	{
		factor->Play();
	}

	void Pause()
	{
		factor->Pause();
	}

	void Move(glm::vec3 amt)
	{
		Object::Move(amt);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Move(amt);
			}
		}
	}

	void Scale(glm::vec3 amt)
	{
		Object::Scale(amt);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Scale(amt);
			}
		}
	}

	void Rotate(glm::quat _quat)
	{
		Object::Rotate(_quat);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Rotate(_quat);
			}
		}
	}

	void SetPosition(glm::vec3 val)
	{
		Object::SetPosition(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetPosition(val);
			}
		}
	}

	void SetRotation(glm::quat val)
	{
		Object::SetRotation(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetRotation(val);
			}
		}
	}

	void SetScale(glm::vec3 val)
	{
		Object::SetScale(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetScale(val);
			}
		}
	}
};

class AnimatedPhysicsObject : public PhysicsObject
{
protected:
	Model** frames;
	unsigned int currentFrame = 0;
	unsigned int nextFrame = 0;
	float startTime = 0.00f;
	unsigned int numFrames;
	Animation<float>* factor;

public:
	AnimatedPhysicsObject(std::vector<std::string> paths, unsigned int _numFrames, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scale, BoxCollider collider)
		:PhysicsObject(_pos, _rot, _scale, collider, MaterialProperties{ 0.50f, 0.40f, 0.30f })
	{
		numFrames = _numFrames;
		frames = new Model* [numFrames];
		for (unsigned int i = 0; i < numFrames; i++) //loop over meshes
		{
			frames[i] = new Model(paths[i].c_str(), _pos, _rot, _scale); //create new model for each frame
		}
		float factorFrames[2] = { 0.0f, 1.0f };
		factor = new Animation<float>(factorFrames, 2, 0.3f);
	}

	~AnimatedPhysicsObject()
	{
		for (unsigned int i = 0; i < numFrames; i++)
		{
			delete frames[i];
		}
		delete[] frames;
		delete factor;
	}

	void Draw(Shader* shader)
	{
		glUniform1f(glGetUniformLocation(shader->GetProgram(), "animFac"), factor->GetFrame());
		for (unsigned int i = 0; i < frames[currentFrame]->GetNumMeshes(); i++) //loop over each mesh
		{
			//pass in the next frames attributes so the shader can blend between them
			glBindVertexArray(frames[currentFrame]->GetMeshes()[i]->GetGLObject()->GetObject());
			frames[currentFrame]->GetMeshes()[i]->GetGLObject()->SetupAttributes(2, frames[nextFrame]->GetMeshes()[i]->GetGLObject()->GetAttribBuffer()->GetBuffer());
			frames[currentFrame]->GetMeshes()[i]->Draw();
		}
	}

	void SetNextFrame(unsigned int val)
	{
		nextFrame = val;
	}

	void SetCurrentFrame(unsigned int val)
	{
		currentFrame = val;
	}

	Model* GetCurrentFrame()
	{
		return frames[currentFrame];
	}

	void Start()
	{
		factor->Start();
	}

	void Stop()
	{
		factor->Stop();
	}

	void Play()
	{
		factor->Play();
	}

	void Pause()
	{
		factor->Pause();
	}

	void Update()
	{
		PhysicsObject::Update();
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetPosition(pos);
				frames[i]->SetRotation(rot);
			}
		}
	}

	virtual void Move(glm::vec3 amt)
	{
		PhysicsObject::Move(amt);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Move(amt);
			}
		}
	}

	virtual void Scale(glm::vec3 amt)
	{
		PhysicsObject::Scale(amt);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Scale(amt);
			}
		}
	}

	virtual void Rotate(glm::quat _quat)
	{
		PhysicsObject::Rotate(_quat);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->Rotate(_quat);
			}
		}
	}

	virtual void SetPosition(glm::vec3 val)
	{
		PhysicsObject::SetPosition(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetPosition(val);
			}
		}
	}

	virtual void SetRotation(glm::quat val)
	{
		PhysicsObject::SetRotation(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetRotation(val);
			}
		}
	}

	virtual void SetScale(glm::vec3 val)
	{
		PhysicsObject::SetScale(val);
		for (unsigned int i = 0; i < numFrames; i++)
		{
			if (frames[i] != nullptr)
			{
				frames[i]->SetScale(val);
			}
		}
	}
};

class Piston : public AnimatedObject
{
protected:
	glm::vec3 extension;
	glm::vec3 initialPos;
	StaticObject* trigger;
	bool enabled;
public:
	Piston(StaticModel* pistonFrame, std::vector<std::string> paths, glm::quat _rot, glm::vec3 _extension, glm::vec3 iOffset, bool initalState)
		: AnimatedObject(paths, 2, pistonFrame->Model::LocalToWorldPoint(glm::vec3(-0.164982f, 0.f, 0.f)), _rot, glm::vec3(1.f))
	{
		extension = _extension + iOffset; //make sure piston still reaches even with the inital offset
		enabled = initalState;
		if (enabled)
		{
			currentFrame = 1;
			nextFrame = 0;
		}
		initialPos = pistonFrame->Model::LocalToWorldPoint(glm::vec3(-0.164982f, 0.f, 0.f)) + iOffset; //set trigger to spawn in the frame

		glm::vec3 startPos;
		if (enabled)
		{
			startPos = initialPos - extension / 2.f;
			trigger = new StaticObject(initialPos, glm::quat(1.f, 0.f, 0.f, 0.f), scal + glm::abs(extension)/2.f, MaterialProperties{ 0.5f, 0.4f, 0.3f, true });
		}
		else
		{
			startPos = initialPos;
			trigger = new StaticObject(initialPos, glm::quat(1.f, 0.f, 0.f, 0.f), scal, MaterialProperties{ 0.5f, 0.4f, 0.3f, true });
		}
		trigger->GetPData()->isPiston = true;
		trigger->SetPosition(startPos);
	}

	~Piston()
	{
		delete trigger;
	}

	void Draw(Shader* shader)
	{
		//make collider move
		if (factor->IsPlaying())
		{
			glm::vec3 newPos;
			glm::vec3 currentExtension;
			if (enabled)
			{
				currentExtension = glm::mix(glm::vec3(0.f), extension / 2.f, factor->GetFrame());
				newPos = glm::mix(initialPos + currentExtension / 2.f, initialPos - currentExtension / 2.f, factor->GetFrame());
			}
			else
			{
				currentExtension = glm::mix(extension / 2.f, glm::vec3(0.f), factor->GetFrame());
				newPos = glm::mix(initialPos - currentExtension / 2.f, initialPos + currentExtension / 2.f, factor->GetFrame());
			}
			trigger->GetPShape()->setGeometry(PxBoxGeometry(FromGLMVec((scal + glm::abs(currentExtension)) / 2.00f)));
			trigger->SetPosition(newPos);
		}
		AnimatedObject::Draw(shader);
	}

	void Toggle()
	{
		if (enabled)
		{
			currentFrame = 1;
			nextFrame = 0;
		}
		else
		{
			currentFrame = 0;
			nextFrame = 1;
		}
		Start();
		enabled = !enabled;
	}
};

class Player : public AnimatedPhysicsObject
{
protected:
	glm::vec2 moveDir = glm::vec2(0.00f);
	glm::vec2 oldMoveDir = glm::vec2(0.00f);
	float moveSpeed = 4.00f; //max movement speed
	float moveTime = 0.50f; //time to get to moveSpeed
	float jumpForce = 6.00f;
	bool shouldJump = false; //communicates the jump keypress from Jump to Update
	bool grounded = false;
	bool sprinting = false;
	bool oldSprinting = false;
	unsigned long long int sprintStartTime = 0;
	unsigned long long int sprintStopTime = 0;
	unsigned long long int maxSprintTime = 2000; //max time sprinting in MS
	long long int currentSprintTime = maxSprintTime; //current sprint stamina left in MS

	void StartSprinting()
	{
		sprintStartTime = eTime;
	}

	void StopSprinting()
	{
		sprintStopTime = eTime;
	}

public:
	Player(glm::vec3 _pos, glm::quat _rot)
		:AnimatedPhysicsObject(playerFrames, 3, _pos, _rot, glm::vec3(1.00f, 1.00f, 1.00f),
			BoxCollider{ PxVec3(0.00f, 0.50f, 0.00f), PxVec3(0.80f, 1.00f, 0.80f) })
	{
		pBody->setRigidDynamicLockFlags(PxRigidDynamicLockFlag::eLOCK_ANGULAR_X | PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z); //stop unwanted rotation
		pBody->setName("player");
	}

	void MoveDir(glm::vec2 dir)
	{
		moveDir += dir;
	}

	void ResetMoveDir()
	{
		moveDir = glm::vec2(0.f);
	}

	void Jump()
	{
		shouldJump = true;
	}

	void Update()
	{
		AnimatedPhysicsObject::Update();

		if (pos.y < -2.f) //if we fell off the map
		{
			dieFlag = true; //die
		}

		if (sprinting) //if sprinting
		{
			currentSprintTime -= dTime; //decrease the time left to sprint
			if (currentSprintTime <= 0) //if stamina is depleted
			{
				SetSprint(false); //stop sprinting
				StopSprinting(); //setup regen
			}
		}
		else //if not sprinting
		{
			currentSprintTime += dTime; //increase time left to sprint
			if (currentSprintTime >= maxSprintTime) //if stamina is over the max
			{
				currentSprintTime = maxSprintTime; //set it to the max
			}
		}

		if (glm::length(moveDir) > 0.00f) //if actually want to move
		{
			//move
			glm::vec2 _moveDir = glm::normalize(moveDir);
			glm::vec3 worldV = glm::rotate(glm::quat(0.00f, 0.00f, 0.00f, 1.00f), -mainCamera->GetAngle() + PI, glm::vec3(0.00f, 1.00f, 0.00f))
				* glm::vec3(_moveDir.x, 0.00f, _moveDir.y); //get the move direction relative to the camera's forward direction
			glm::vec2 v = glm::vec2(worldV.x * moveSpeed, worldV.z * moveSpeed); //get target move speed
			glm::vec3 current = FromPxVec(pBody->getLinearVelocity()); //current move speed
			glm::vec2 u = glm::vec2(current.x, current.z); //get current move speed
			glm::vec2 f = pBody->getMass() * (v - u) / moveTime; //calculate the force

			pBody->addForce(PxVec3(f.x, 0.00f, f.y), PxForceMode::eFORCE); //apply movement force

			//rotate
			glm::vec3 pForward = rot * glm::vec3(0.00f, 0.00f, 1.00f); //get local forward in global space
			glm::vec2 playerForward = glm::normalize(glm::vec2(pForward.x, pForward.z)); //normalize without y component (removes pitch)
			float angle = glm::orientedAngle(glm::vec3(playerForward.x, 0.00f, playerForward.y), glm::normalize(worldV), glm::vec3(0.00f, 1.00f, 0.00f)); //get angle between global forward and global move direction
			if (glm::abs(angle) > 0.05f) //if the angle to rotate is greater than a tolerance
			{ //this tolerance stops overshooting with the else statement, and also stops infinite forces (A = ../2*angle) where an angle of 0 gives an A of infinity
				constexpr float w = 2.00f * PI; //max angular velocity of 360 deg per sec (250ms for a 90 degree turn) about the y axis
				float w0 = pBody->getAngularVelocity().y; //current angular velocity about the y axis
				float A = (pow(w, 2) - pow(w0, 2)) / 2.00f * angle; //A = (v^2 - u^2)/2s
				pBody->addTorque(PxVec3(0.00f, A * pBody->getMass(), 0.00f), PxForceMode::eFORCE); //F = m*a
			}
			else //if we are (basically) at the right angle
			{
				PxVec3 old = pBody->getAngularVelocity();
				pBody->setAngularVelocity(PxVec3(old.x, 0.00f, old.z)); //stop rotating (this stops rotation from overshooting)
			}
		}

		if (shouldJump == true)
		{
			if (grounded)
			{
				pBody->addForce(PxVec3(0.00f, pBody->getMass() * jumpForce, 0.00f)
					, PxForceMode::eIMPULSE);
				grounded = false;
			}
			shouldJump = false;
		}

		if (glm::length(moveDir) > 0 && glm::length(oldMoveDir) == 0) //if moving this frame and not moving last frame (first frame we are moving)
		{
			if (sprinting)
			{
				currentFrame = 0; //set current frame to idle
				nextFrame = 2; //set next frame to running
				StartSprinting();
			}
			else
			{
				currentFrame = 0; //set current frame to idle
				nextFrame = 1; //set next frame to walking
			}
			factor->Start(); //start blending between frames
		}
		if (glm::length(moveDir) > 0 && glm::length(oldMoveDir) > 0)
		{
			if (sprinting && !oldSprinting) //first frame we are sprinting while moving
			{
				currentFrame = 1;
				nextFrame = 2;
				factor->Start();
				StartSprinting();
			}
			if (!sprinting && oldSprinting) //first frame we stop sprinting
			{
				currentFrame = 2;
				nextFrame = 1;
				factor->Start();
				StopSprinting();
			}
		}
		if (glm::length(moveDir) == 0 && glm::length(oldMoveDir) > 0) //if not moving, but was last frame (first frame we stop)
		{
			currentFrame = nextFrame; //set current frame to whatever next frame was
			nextFrame = 0; //set next frame idle
			factor->Start(); //start blending between frames
		}
		oldMoveDir = moveDir;
		oldSprinting = sprinting;
	}

	void SetSprint(bool val)
	{
		sprinting = val;
		if (sprinting)
			moveSpeed = 7.33f;
		else
			moveSpeed = 4.00f;
	}

	float GetStamina()
	{
		return ((float)currentSprintTime) / maxSprintTime;
	}

	void SetGrounded(bool val)
	{
		grounded = val;
	}

	bool GetGrounded()
	{
		return grounded;
	}

	PxRigidDynamic* GetPBody()
	{
		return pBody;
	}
};

//unused
class Platform
	: public StaticObject, public AnimatedObject
{
protected:
	bool enabled = true;

public:
	Platform(glm::vec3 _pos, glm::vec3 _scale, std::vector<std::string> paths)
		:StaticObject(_pos, glm::quat(1.00f, 0.00f, 0.00f, 0.00f), _scale, MaterialProperties{ 0.5f, 0.4f, 0.5f }),
			AnimatedObject(paths, 2, _pos, glm::quat(1.f, 0.f, 0.f, 0.f), _scale)
	{
	}

	void Enable()
	{
		if (!enabled)
		{
			nextFrame = 1;
			currentFrame = 0;
			Start();
			pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, true);
			enabled = true;
		}
	}

	void Disable()
	{
		if (enabled)
		{
			nextFrame = 0;
			currentFrame = 1;
			Start();
			pShape->setFlag(PxShapeFlag::eSIMULATION_SHAPE, false);
			enabled = false;
		}
	}
};

//not implemented
class Light : public Object
{
};

class DustParticle: public Model
{
protected:
	Animation<glm::vec3>* anim;
	bool dead = false;

public:
	DustParticle(Model* copyModel, glm::vec3 position, glm::vec3 initalScale, glm::vec3 finalScale)
		:Model(*copyModel, position, glm::quat(1.00f, 0.00f, 0.00f, 0.00f), initalScale)
	{
		constexpr float lifeTime = 0.67f;
		Rotate(glm::quat(glm::vec3(SDL_randf() * PI, SDL_randf() * PI, SDL_randf() * PI))); //give random inital rotation
		glm::vec3 frames[] = { initalScale, finalScale };
		anim = new Animation(frames, 2, lifeTime); //create animation to smoothly scale the particle
		anim->Start();
	}

	~DustParticle()
	{
		delete anim;
	}

	void Update()
	{
		SetScale(anim->GetFrame());
		if (!anim->IsPlaying()) //if anim is finished
		{
			dead = true;
		}
	}

	bool Dead()
	{
		return dead;
	}
};

class DustCloud: public Object
{
protected:
	Player* target;
	Model* copyModel;
	unsigned long long lastSpawnTime = 0;
	const float minSpawnDelay = 40; //min wait time in ms before we can spawn a new particle
	const unsigned int maxParticles = 100; //max particle count
	DustParticle** particles;
	unsigned int particlePointer = 0;

	public:
	DustCloud(Player* _target, Model* _copyModel) : Object()
	{
		target = _target;
		copyModel = new Model(*copyModel, glm::vec3(0.00f), glm::quat(1.00f, 0.00f, 0.00f, 0.00f), glm::vec3(1.00f));
		particles = new DustParticle* [maxParticles];
		for (unsigned int i = 0; i < maxParticles; i++) //init array
		{
			particles[i] = nullptr;
		}
	}

	DustCloud(Player* _target, const char* path) : Object()
	{
		target = _target;
		copyModel = new Model(path, glm::vec3(0.00f), glm::quat(1.00f, 0.00f, 0.00f, 0.00f), glm::vec3(1.00f));
		particles = new DustParticle* [maxParticles];
		for (unsigned int i = 0; i < maxParticles; i++) //init array
		{
			particles[i] = nullptr;
		}
	}

	~DustCloud()
	{
		delete copyModel;
		delete[] particles;
	}

	void SpawnParticle(glm::vec3 _position)
	{
		particlePointer = (particlePointer + 1) % maxParticles; //increment the pointer
		delete particles[particlePointer]; //delete the old particle (should be nullptr but we delete for safety)
		particles[particlePointer] = new DustParticle(copyModel, _position, glm::vec3(0.15f), glm::vec3(0.5f)); //create new particle
	}

	void Update()
	{
		for (unsigned int i = 0; i < maxParticles; i++) //loop over each particle and check if it should be deleted
		{
			if (particles[i] != nullptr)
			{
				particles[i]->Update();
				if (particles[i]->Dead())
				{
					delete particles[i];
					particles[i] = nullptr;
				}
			}
		}

		PxRigidDynamic* pBody = target->GetPBody();
		glm::vec3 playerSpeed = FromPxVec(pBody->getLinearVelocity());
		playerSpeed.y = 0.00f;
		//if player is moving fast enough on the ground
		if (glm::length(playerSpeed) >= 4.00f * 0.80f && player->GetGrounded())
		{
			//if we last spawned a particle long enough ago to spawn another
			if (eTime - lastSpawnTime >= minSpawnDelay)
			{
				//spawn particle at each track
				SpawnParticle(target->LocalToWorldPoint(glm::vec3(0.33f, 0.08f, -0.33f)));
				SpawnParticle(target->LocalToWorldPoint(glm::vec3(-0.33f, 0.08f, -0.33f)));
				lastSpawnTime = eTime; //reset spawn cooldown
			}
		}
	}

	void Draw()
	{
		for (unsigned int i = 0; i < maxParticles; i++) //loop over each particle
		{
			if (particles[i] != nullptr) //if it exists
			{
				particles[i]->Draw(); //draw it
			}
		}
	}
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

	~Sun()
	{
		delete shadowBuffer;
	}

	glm::mat4 CalculateCombinedMatrix()
	{
		const float nearPlane = 0.10f;
		const float farPlane = 15.00f;
		glm::mat4 projection = glm::ortho(-15.00f, 15.00f, -15.00f, 15.00f, nearPlane, farPlane);
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

class StaminaBar : public Model
{
protected:
	Player* target;
	float stamina = 1.0f;

public:
	StaminaBar(const char* path, Player* _target)
		:Model(path, glm::vec3(0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.3f))
	{
		target = _target;
	}

	void Update()
	{
		stamina = target->GetStamina();
		SetPosition(target->LocalToWorldPoint(glm::vec3(0.00f, 0.15f, -0.275f))); //put the stamina bar on the player
		SetRotation(target->GetRotation());
		glm::vec3 col = glm::mix(glm::vec3(0.8f, 0.0f, 0.0f), glm::vec3(0.0f, 0.8f, 0.0f), stamina); //set the colour to the stamina remaning
		meshes[0]->SetColor(col);
	}

	void SetStamina(float val)
	{
		stamina = val;
	}

	//Use and setup the emissive shader before calling
	void Draw(Shader* currentShader, Shader* diffuseShader) //diffuse shader should be already setup
	{
		meshes[0]->Draw();
		diffuseShader->Use();
		meshes[1]->Draw();
		currentShader->Use();
	}

	void Draw()
	{
		Model::Draw();
	}
};

class Coin : public StaticObject, public Model
{
protected:
	bool collected = false;
	unsigned long long int index;

public:
	Coin(Model* copyModel, glm::vec3 _pos, unsigned long long int _index)
		:StaticObject(_pos, glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.20f), MaterialProperties{ 0.5f, 0.4f, 0.3f, true }),
		Model(*copyModel, _pos, glm::quat(glm::vec3(glm::radians(60.00f), 0.00f, 0.00f)), glm::vec3(0.20f))
	{
		pData.isCoin = true;
		index = _index;
	}

	~Coin()
	{
		pData.pointer = nullptr;
		coins[index] = nullptr;
	}

	void Collect()
	{
		collected = true;
	}

	bool IsCollected()
	{
		return collected;
	}

	void Update()
	{
		constexpr float radPerSec = PI / 2.0f; //90 deg per sec of rotation
		if (collected)
		{
			delete this;
			return;
		}

		glm::vec3 axis = glm::normalize(glm::rotateX(glm::vec3(0.0f, 1.0f, 0.0f), -glm::radians(60.00f))); //convert local y axis to world y axis
		Model::SetRotation(glm::rotate(Model::rot, radPerSec * dTime/1000, axis)); //rotate about the global y
	}

	void Draw()
	{
		Model::Draw();
	}
};

class PistonLight : public Model
{
protected:
	bool initalState;

public:
	PistonLight(Model& copyModel, StaticModel* target, bool initalToggle)
		:Model(copyModel)
	{
		initalState = initalToggle;
		SetPosition(target->Model::LocalToWorldPoint(glm::vec3(-0.12308f, 0.436407f, 0.f)));
		SetRotation(target->Model::GetRotation());
	}

	//Use and setup the emissive shader before calling
	void Draw(Shader* currentShader, Shader* diffuseShader) //diffuse shader should be already setup
	{
		if (platformToggle != initalState) //xor
			meshes[0]->SetColor(glm::vec3(0.f, 0.8f, 0.f));
		else
			meshes[0]->SetColor(glm::vec3(0.8f, 0.f, 0.f));
		meshes[0]->Draw();
		diffuseShader->Use();
	}

	void Draw() //used for drawing to maps
	{
		Model::Draw();
	}
};

class ContactCallback : public PxSimulationEventCallback
{
public:
	virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override
	{
		unsigned int playerIndex = 3;
		unsigned int otherIndex = 3;
		//this code makes sure we know which actor in the pair is the player and which isn't
		if (pairHeader.actors[0]->getName() == "player")
		{
			playerIndex = 0;
			otherIndex = 1;
		}
		else
		{
			if (pairHeader.actors[1]->getName() == "player")
			{
				playerIndex = 1;
				otherIndex = 0;
			}
			else
				return;
		}
		if (pairHeader.actors[otherIndex]->userData != nullptr)
		{
			PhysicsData* otherData = static_cast<PhysicsData*>(pairHeader.actors[otherIndex]->userData);
			if (otherData->isGround == true)
			{
				player->SetGrounded(true);
			}
		}
	}

	virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override {};

	virtual void onWake(PxActor** actors, PxU32 count) override {};

	virtual void onSleep(PxActor** actors, PxU32 count) override {};

	virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) override
	{
		for (PxU32 i = 0; i < count; i++) //loop over each trigger pair
		{
			if (pairs[i].otherActor->getName() == "player") //if the collider that entered the trigger is the player
			{
				if (pairs[i].triggerActor->userData != nullptr) //safety
				{
					PhysicsData* data = static_cast<PhysicsData*>(pairs[i].triggerActor->userData);
					if (data->pointer != nullptr && data->isCoin) //if its a coin
					{
						Coin* coin = static_cast<Coin*>(data->pointer);
						if (!coin->IsCollected()) //if the coin is not already collected (fixes bug with physX sub stepping)
						{
							IncreaseScore(5);
							coin->Collect();
						}
					}

					if (data->isPiston == true) //if we hit a piston
					{
						dieFlag = true; //die (this cannot happen here as we cannot modify the physics state in this callback)
					}
				}
			}
		}
	}

	virtual void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {};
}; ContactCallback pContactCallback;

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
	case(key_Sprint):
		if (k_Sprint.Press())
		{
			player->SetSprint(true);
		}
		break;
	case(key_Start):
		if (isMainMenu)
		{
			UnloadMainMenu();
			LoadLevel01();
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
	case(key_Sprint):
		if (k_Sprint.Release())
		{
			player->SetSprint(false);
		}
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
	if (pFoundation != nullptr)
		PX_RELEASE(pFoundation);

	exit(code);
}

void TogglePlatforms()
{
	if (platformToggle)
	{
		std::for_each(pistons.begin(), pistons.end(), [&](Piston* piston) { piston->Toggle(); });
		platformToggle = false;
		toggleShader->Use();
		glUniform3fv(glGetUniformLocation(toggleShader->GetProgram(), "colour"), 1, glm::value_ptr(glm::vec3(1.f, 0.f, 0.f)));
	}
	else
	{
		std::for_each(pistons.begin(), pistons.end(), [&](Piston* piston) { piston->Toggle(); });
		platformToggle = true;
		toggleShader->Use();
		glUniform3fv(glGetUniformLocation(toggleShader->GetProgram(), "colour"), 1, glm::value_ptr(glm::vec3(0.f, 1.f, 0.f)));
	}
}

void LoadLevel01()
{
	isMainMenu = false;
	dieFlag = false;
	delete player;
	player = new Player(glm::vec3(-9.00f, 1.00f, -4.00f), glm::quat(glm::vec3(0.00f, 90.00f, 0.00f)));
	player->ResetMoveDir();
	k_Forward.Reset();
	k_Left.Reset();
	k_Backward.Reset();
	k_Right.Reset();
	k_Toggle.Reset();
	k_Jump.Reset();
	k_Sprint.Reset();
	platformToggle = false;
	toggleShader->Use();
	glUniform3fv(glGetUniformLocation(toggleShader->GetProgram(), "colour"), 1, glm::value_ptr(glm::vec3(1.f, 0.f, 0.f)));
	delete playerCloud;
	playerCloud = new DustCloud(player, Path("models/ball.obj"));
	delete stamBar;
	stamBar = new StaminaBar(Path("models/stamina_bar.obj"), player);
	toggleTexture->Use(5);

	Model* copyModel = new Model(Path("models/cube.obj"), glm::vec3(0.0f), glm::quat(glm::vec3(0.0f, glm::radians(45.0f), 0.0f)), glm::vec3(1.0f));
	groundPlane = new StaticModel(*copyModel, glm::vec3(0.00f, -0.975f, 0.00f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(2.f * 10.f, 1.00f, 2.f * 10.f),
		MaterialProperties{ 0.5f, 0.4f, 0.3f });
	groundPlane->SetColor(DEFAULT_COLOR);
	groundPlane->GetPData()->isGround = true;
	delete copyModel;


	const glm::vec3 barrelOffset = glm::vec3(0.5f, -0.25f, -0.5f);
	//blender position to gl posiiton (Y, Z, X)

	copyModel = new Model(Path("models/mapping/barrel_container.obj"));
	PhysicsModel* physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(0.062345f, 0.375566f, -0.277947f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(-0.403407f, 0.375566f, -0.606259f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(0.143077f, 0.375566f, -0.861213f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(-0.038688f, 1.12481f, -0.592288f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	//far side pallet stack 3 ontop of 4
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(3.73246f, 0.534695f, 9.49568f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(4.3871f, 0.534695f, 9.52294f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(4.36675f, 0.534695f, 8.92032f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(3.72481f, 0.534695f, 8.91513f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(3.73246f, 1.43093f, 9.49568f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(4.3871f, 1.43093f, 9.52294f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(3.72481f, 1.42238f, 8.91513f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(5.35612f, 0.534695f, 9.45993f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(5.34847f, 0.526144f, 8.87939f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(6.01076f, 0.534695f, 9.4872f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = LoadPhysicsModel(*copyModel, glm::vec3(6.16564f, 0.402007f, 8.1802f) + barrelOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f),
		Path("models/mapping/barrel_container_collider.obj"), glm::vec3(0.f));
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	delete copyModel;
	//pallets
	glm::vec3 palletOffset = glm::vec3(-0.1f, 0.1f, -0.1f);
	copyModel = new Model(Path("models/mapping/pallet.obj"));
	physicsObj = new PhysicsModel(*copyModel, glm::vec3(4.06481f, 0.102454f, 9.19628f) + palletOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f), PxVec3(1.40f, 0.175f, 1.44f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = new PhysicsModel(*copyModel, glm::vec3(4.06481f, 1.00839f, 9.19628f) + palletOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f), PxVec3(1.40f, 0.175f, 1.44f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = new PhysicsModel(*copyModel, glm::vec3(5.68277f, 0.102454f, 9.19628f) + palletOffset, glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f), PxVec3(1.40f, 0.175f, 1.44f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = new PhysicsModel(*copyModel, glm::vec3(-0.163543f, 0.655497f, -1.44676f) + palletOffset, glm::quat(glm::vec3(glm::radians(67.808f), glm::radians(00.f), 0.f)), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f), PxVec3(1.40f, 0.175f, 1.44f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	delete copyModel;
	//conveyor boxes
	copyModel = new Model(Path("models/mapping/box.obj"));
	for (unsigned int i = 0; i < 8; i++)
	{
		physicsObj = new PhysicsModel(*copyModel, glm::vec3(-5.17963f, 1.8f, 2.49429f - (5.f * 0.3f * i)), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
			BoxCollider{ PxVec3(0.f), PxVec3(0.3f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
		drawModels.push_back(physicsObj);
		pObjects.push_back(physicsObj);
	}
	delete copyModel;
	//more conveyor boxes
	physicsObj = new PhysicsModel(Path("models/mapping/box_dented.obj"), glm::vec3(-5.25463f, 0.3f, 3.22109f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f), PxVec3(0.3f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	physicsObj = new PhysicsModel(Path("models/mapping/box_open.obj"), glm::vec3(-5.25463f, 0.3f, 4.11169f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(0.f, -0.05f, 0.f), PxVec3(0.3f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	//big open box
	physicsObj = new PhysicsModel(Path("models/mapping/box_open.obj"), glm::vec3(9.18284f, 1.f, 9.14864f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(7.8f/2),
		BoxCollider{ PxVec3(0.f, -0.2f, 0.f), PxVec3(0.3f * 7.8f/2) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	drawModels.push_back(physicsObj);
	pObjects.push_back(physicsObj);
	//static colliders
	//conveyor belts
	StaticObject* sceneCollider = new StaticObject(glm::vec3(6.0f, 0.1f, -1.71304f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(0.8f, 0.15f, 16.4f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(-5.19f, 0.f, -3.29f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(0.75f, 0.15f, 12.0f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//red containers
	sceneCollider = new StaticObject(glm::vec3(1.65f, 1.31729f, 1.12216f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(5.6f, 3.f, 2.39f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(1.65f, 1.31729f, 1.12216f + 5.6f + 0.25f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(5.6f, 3.f, 2.39f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(1.65f - 2.39f - 0.2f, 1.31729f, 1.12216f + 5.6f + 0.25f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(5.6f, 3.f, 2.39f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(1.65f - (2.39f/2) - 0.5f, 1.31729f, 1.12216f - 5.6f + (2.39f/2) + 0.22f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(5.65f, 3.f, 2.34f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(1.65f - (2.39f / 2) - 0.5f, 1.31729f, 1.12216f - 5.6f - 5.8f + (2.39f / 2) + 0.35f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(5.65f, 3.f, 2.34f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//big boxes
	sceneCollider = new StaticObject(glm::vec3(-4.06237f, 0.1f, 0.791069f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.23f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(-4.06237f + (1.23f * 1.1f), 0.1f, 0.791069f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.23f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(-3.51954f, 0.1f + (1.23f * 1.1f), 0.791069f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.23f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//water containers
	sceneCollider = new StaticObject(glm::vec3(-4.13062f, 0.f, 1.98159f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.075f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(-0.375972f, 0.25f, 3.2367f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.075f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	sceneCollider = new StaticObject(glm::vec3(7.31743f, 0.175f, 9.22745f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.0f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//water container pallet
	sceneCollider = new StaticObject(glm::vec3(7.26648f, 0.175f - 0.5f, 9.19628f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3 (1.40f, 0.175f, 1.44f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//pallet shelf
	sceneCollider = new StaticObject(glm::vec3(-5.04544f, 0.4f, 9.24168f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(1.1f, 2.1f, 1.2f), MaterialProperties{ 0.5f, 0.4f, 0.3f });
	allocatedColliders.push_back(sceneCollider);
	//load pistons
	copyModel = new Model(Path("models/piston_frame.obj"));
	Model* pistonLightCopyModel = new Model(Path("models/piston_light.obj"));
	std::vector<std::string> pistonPaths = { Path("models/piston.obj"), Path("models/piston_extended.obj") };
	StaticModel* pistonFrame = new StaticModel(*copyModel, glm::vec3(0.501424f, 0.4f, 0.803111f), glm::quat(1.f, 0.f, 0.f, 0.f), glm::vec3(1.f),
		BoxCollider{ PxVec3(-0.1f, 0.f, 0.f), PxVec3(0.2f, 1.f, 1.f)}, MaterialProperties{0.5f, 0.4f, 0.3f});
	PistonLight* pistonLight = new PistonLight(*pistonLightCopyModel, pistonFrame, false);
	Piston* piston = new Piston(pistonFrame, pistonPaths, glm::quat(glm::vec3(0.f, glm::radians(180.f), 0.f)), glm::vec3(3.6f, 0.f, 0.f), glm::vec3(0.5f, 0.f, 0.f), false);
	drawModels.push_back(pistonFrame);
	allocatedColliders.push_back((StaticObject*)pistonFrame);
	pistonLights.push_back(pistonLight);
	drawModels.push_back(pistonFrame);
	pistons.push_back(piston);
	pistonFrame = new StaticModel(*copyModel, glm::vec3(-1.86611f, 0.4f, -1.90186f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(1.f),
		BoxCollider{ PxVec3(-0.1f, 0.f, 0.f), PxVec3(0.2f, 1.f, 1.f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	pistonLight = new PistonLight(*pistonLightCopyModel, pistonFrame, false);
	piston = new Piston(pistonFrame, pistonPaths, glm::quat(glm::vec3(0.f, glm::radians(-90.f), 0.f)), glm::vec3(0.f, 0.f, -3.6f), glm::vec3(-0.164982f, 0.f, -0.5f - 0.164982f), false);
	drawModels.push_back(pistonFrame);
	allocatedColliders.push_back((StaticObject*)pistonFrame);
	pistonLights.push_back(pistonLight);
	pistons.push_back(piston);
	pistonFrame = new StaticModel(*copyModel, glm::vec3(-1.49855f, 0.4f, -7.58753f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(1.f),
		BoxCollider{ PxVec3(-0.1f, 0.f, 0.f), PxVec3(0.2f, 1.f, 1.f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	pistonLight = new PistonLight(*pistonLightCopyModel, pistonFrame, false);
	piston = new Piston(pistonFrame, pistonPaths, glm::quat(glm::vec3(0.f, glm::radians(-90.f), 0.f)), glm::vec3(0.f, 0.f, -3.6f), glm::vec3(-0.164982f, 0.f, -0.5f - 0.164982f), false);
	drawModels.push_back(pistonFrame);
	allocatedColliders.push_back((StaticObject*)pistonFrame);
	pistonLights.push_back(pistonLight);
	pistons.push_back(piston);
	pistonFrame = new StaticModel(*copyModel, glm::vec3(1.70567f, 0.4f, -7.54987f), glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(1.f),
		BoxCollider{ PxVec3(-0.1f, 0.f, 0.f), PxVec3(0.2f, 1.f, 1.f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	pistonLight = new PistonLight(*pistonLightCopyModel, pistonFrame, false);
	piston = new Piston(pistonFrame, pistonPaths, glm::quat(glm::vec3(0.f, glm::radians(-90.f), 0.f)), glm::vec3(0.f, 0.f, -3.6f), glm::vec3(-0.164982f, 0.f, -0.5f - 0.164982f), false);
	drawModels.push_back(pistonFrame);
	allocatedColliders.push_back((StaticObject*)pistonFrame);
	pistonLights.push_back(pistonLight);
	pistons.push_back(piston);
	pistonFrame = new StaticModel(*copyModel, glm::vec3(0.f, 0.4f, -4.21222f), glm::quat(glm::vec3(0.f, glm::radians(-90.f), 0.f)), glm::vec3(1.f),
		BoxCollider{ PxVec3(-0.1f, 0.f, 0.f), PxVec3(0.2f, 1.f, 1.f) }, MaterialProperties{ 0.5f, 0.4f, 0.3f });
	pistonLight = new PistonLight(*pistonLightCopyModel, pistonFrame, true);
	piston = new Piston(pistonFrame, pistonPaths, glm::quat(glm::vec3(0.f, glm::radians(90.f), 0.f)), glm::vec3(0.f, 0.f, 3.6f), glm::vec3(-0.164982f, 0.f, 0.5f + 0.164982f), true);
	drawModels.push_back(pistonFrame);
	allocatedColliders.push_back((StaticObject*)pistonFrame);
	pistonLights.push_back(pistonLight);
	pistons.push_back(piston);
	delete copyModel;
	delete pistonLightCopyModel;

	//place "coins"
	Model* nutModel = new Model(Path("models/nut.obj"));
	Model* boltModel = new Model(Path("models/bolt.obj"));

	numCoins = 50;
	coins = new Coin* [numCoins];
	for (unsigned long long int i = 0; i < numCoins; i++)
	{
		coins[i] = nullptr;
	}
	for (int i = 0; i < numCoins; i++)
	{
		float x = SDL_randf() * 20.f - 10.f;
		float z = SDL_randf() * 20.f - 10.f;
		Model* coinModel = nutModel;
		if (i % 2 == 0)
			coinModel = boltModel;
		coins[i] = new Coin(coinModel, glm::vec3(x, 0.f, z), i);
	}
	delete nutModel;
	delete boltModel;
	eTime = SDL_GetTicks();
}

void UnloadLevel01()
{
	//loop over coins
	for (unsigned int i = 0; i < numCoins; i++)
	{
		delete coins[i]; //delete each coin
	}
	delete coins;
	//delete all allocated objects
	std::for_each(pObjects.begin(), pObjects.end(), [&](PhysicsObject* object) { delete object; });
	pObjects.clear();
	std::for_each(allocatedColliders.begin(), allocatedColliders.end(), [&](Object* object) { delete object; });
	allocatedColliders.clear();
	std::for_each(pistonLights.begin(), pistonLights.end(), [&](PistonLight* object) { delete object; });
	pistonLights.clear();
	std::for_each(pistons.begin(), pistons.end(), [&](Piston* object) { delete object; });
	pistons.clear();
	delete groundPlane;
	//empty drawable objects
	drawModels.clear();
}

void IncreaseScore(int amt)
{
	score += amt;
	std::cout << score << "\n";
}

void LoadMainMenu()
{
	Model* plane = new Model(Path("models/plane.obj"), glm::vec3(0.00f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));
	drawModels.push_back(plane);
	isMainMenu = true;
}

void UnloadMainMenu()
{
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { delete drawModel; });
	glEnable(GL_CULL_FACE);
	drawModels.clear();
}

PhysicsModel* LoadPhysicsModel(Model& copyModel, glm::vec3 _pos, glm::quat _rot, glm::vec3 _scal, const char* colliderPath, glm::vec3 colliderOffset)
{
	TriangleMesh* colliderTris = new TriangleMesh(colliderPath);
	PxConvexMeshDesc colliderDesc;
	colliderDesc.points.count = colliderTris->size;
	colliderDesc.points.stride = sizeof(PxVec3);
	colliderDesc.points.data = colliderTris->verts;
	colliderDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX;

	PxCookingParams params(pPhysics->getTolerancesScale());

	PxDefaultMemoryOutputStream cookingBuffer;
	PxConvexMeshCookingResult::Enum result;
	PxCookConvexMesh(params, colliderDesc, cookingBuffer, &result);
	PxDefaultMemoryInputData input(cookingBuffer.getData(), cookingBuffer.getSize());
	PxConvexMesh* convexMesh = pPhysics->createConvexMesh(input);
	return new PhysicsModel(copyModel, _pos, _rot, _scal, MaterialProperties {0.5f, 0.4f, 0.3f}, convexMesh, colliderOffset);
}