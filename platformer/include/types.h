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
#include "PhysX/PxPhysicsAPI.h"
#define STB_IMAGE_IMPLEMENTATION
#ifdef WINDOWS
	#define STBI_WINDOWS_UTF8 //change this have ifdef WINDOWS 
#endif
#include "stb/stb_image.h"
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"

using namespace physx;

#ifdef _WIN32
#define exit(code) exit(##code##)
#endif

#define PI glm::pi<float>()

#define Path(asset) std::string(SDL_GetBasePath() + std::string("assets\\") +std::string(##asset##))

const char* errorVert = { "#version 450 core\n"
"layout(location = 0) in vec3 position;\n"
"void main()\n"
"{\n"
"	gl_Position = vec4(position.x, position.y, 0.0, 1.0);\n"
"}\n"
};

const char* errorFrag = { "#version 450 core\n"
"out vec4 color;\n"
"void main()\n"
"{\n"
"	color = vec4(1.0, 0.0, 1.0, 1.0);\n"
"}\n"
};

int quit(int code);
void PrintGLErrors();

class Shader;
class Camera;
class GLFramebuffer;
class Player;
class Platform;

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

class Key
{
protected:
	SDL_Keycode keycode;
	bool pressed = false;

public:
	Key(SDL_Keycode _keycode)
	{
		keycode = _keycode;
		pressed = false;
	}

	SDL_Keycode GetCode()
	{
		return keycode;
	}

	bool Press()
	{
		if (!pressed)
		{
			pressed = true;
			return true;
		}
		return false;
	}

	bool Release()
	{
		if (pressed)
		{
			pressed = false;
			return true;
		}
		return false;
	}
};

#define key_Forward SDLK_W
#define key_Left SDLK_A
#define key_Backward SDLK_S
#define key_Right SDLK_D

SDL_Window* window;
SDL_GLContext glContext;
Shader* errorShader = nullptr;
Camera* mainCamera;
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
float screenWidth, screenHeight;
float sensitivity = 0.66f;
Player* player;
Shader* shadowShader;
Platform* groundPlane;

Key k_Forward = Key(key_Forward);
Key k_Left = Key(key_Left);
Key k_Backward = Key(key_Backward);
Key k_Right = Key(key_Right);

float oldDist = 2.00f;

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
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

//effectively transposes the aiMatrix4x4 into a glm::mat4
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

//effectively transposes the glm::mat4 into an aiMatrix4x4
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


class Object
{
protected:
	glm::vec3 pos;
	glm::quat rot = glm::quat(1.00f, 0.00f, 0.00f, 0.00f);
	glm::vec3 scal;

public:
	Object()
	{
		pos = glm::vec3(0.00f);
		rot = glm::quat(1.00f, 0.00f, 0.00f, 0.00f);
		scal = glm::vec3(1.00f);
	}

	Object(glm::vec3 _pos, glm::quat _rot, glm::vec3 scale)
	{
		pos = _pos;
		rot = _rot;
		scal = scale;
	}

	Object(PxVec3 _pos, PxQuat _rot, PxVec3 scale)
	{
		pos = FromPxVec(_pos);
		rot = FromPxQuat(_rot);
		scal = FromPxVec(scale);
	}

	virtual void Move(glm::vec3 amt)
	{
		pos += amt;
	}

	virtual void Scale(glm::vec3 amt)
	{
		scal.x *= amt.x;
		scal.y *= amt.y;
		scal.z *= amt.z;
	}

	virtual void Rotate(glm::quat _quat)
	{
		rot = rot * _quat;
	}

	virtual void SetPosition(glm::vec3 val)
	{
		pos = val;
	}

	virtual void SetRotation(glm::quat val)
	{
		rot = val;
	}

	virtual void SetScale(glm::vec3 val)
	{
		scal.x = val.x;
		scal.y = val.y;
		scal.z = val.z;
	}

	virtual glm::vec3 GetPosition()
	{
		return pos;
	}

	virtual glm::quat GetRotation()
	{
		return rot;
	}

	virtual glm::vec3 GetScale()
	{
		return scal;
	}
};

class Camera
{
protected:
	float angle;
	float inclination;
	float distance;
	glm::mat4 view;
	glm::mat4 projection;

	glm::mat4 LookAt(glm::vec3 target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		glm::vec3 cameraPos = glm::vec3(distance * glm::cos(angle), distance * glm::sin(inclination), distance * -glm::sin(angle));
		return glm::lookAt(target + cameraPos, target, glm::vec3(0.00f, 1.00f, 0.00f));
	}

	glm::mat4 LookAt(Object target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		glm::vec3 cameraPos = glm::vec3(distance * glm::cos(angle), distance * glm::sin(inclination), distance * -glm::sin(angle));
		return glm::lookAt(target.GetPosition() + cameraPos, target.GetPosition(), glm::vec3(0.00f, 1.00f, 0.00f));
	}

	glm::mat4 Project()
	{
		return glm::perspective(glm::radians(45.f), screenWidth/screenHeight, 0.1f, 15.00f);
	}

public:
	Camera(glm::vec3 _target, float _angle)
	{
		angle = _angle;
		inclination = glm::radians(45.00f);
		distance = 2.00f;
		view = LookAt(_target);
		projection = Project();
	}

	glm::mat4 GetCombinedMatrix()
	{
		return projection * view;
	}

	void SetDistance(float val)
	{
		distance = val;
	}

	void SetAngle(float val)
	{
		angle = glm::mod(val, 2 * PI);
	}

	void Angle(float amt)
	{
		angle = glm::mod(angle + amt, 2 * PI);
	}

	void SetInclination(float val)
	{
		inclination = glm::mod(val, 2 * PI);
	}

	void Inclination(float amt)
	{
		inclination = glm::mod(inclination + amt, 2 * PI);
	}

	void Follow(glm::vec3 target)
	{
		view = LookAt(target);
	}

	glm::mat4 GetView()
	{
		return view;
	}

	float GetAngle()
	{
		return angle;
	}

	float GetInclination()
	{
		return inclination;
	}

	void Distance(float amt)
	{
		distance += amt;
	}
};

class File
{
protected:
	char* data = nullptr;
	unsigned long long int size;

public:
	File(const char* path)
	{
		std::filesystem::path filePath{ path }; //create a std::filesystem::path
		size = std::filesystem::file_size(filePath); //properly get the size of the file
		if (size == 0) //if file is empty
			return; //exit
		std::fstream fileStream; //create the fstream
		fileStream.open(path, std::ios::in | std::ios::binary); //load file to read as binary
		data = new char[size + 1]; //create string to hold source
		fileStream.read(data, size); //load file
		data[size] = '\0'; //make sure eof is null terminated (useful for loading text files)
		fileStream.close(); //close file
	}

	~File()
	{
		delete[] data; //delete data
	}

	const char* const* GetDataPointer()
	{
		return &data;
	}

	const char* GetData()
	{
		return data;
	}

	unsigned long long int GetSize()
	{
		return size;
	};
};

class Shader
{
protected:
	glm::uint program = 0;

public:
	Shader(const char* vertexPath, const char* fragmentPath)
	{
		File* vertexFile = new File(vertexPath);
		File* fragmentFile = new File(fragmentPath);
		glm::uint vertexShader = glCreateShader(GL_VERTEX_SHADER); //init empty shader
		glm::uint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); //..
		glShaderSource(vertexShader, 1, vertexFile->GetDataPointer(), NULL); //load shader source
		glCompileShader(vertexShader); //compile shader source
		glShaderSource(fragmentShader, 1, fragmentFile->GetDataPointer(), NULL); //..
		glCompileShader(fragmentShader); //..

		program = glCreateProgram(); //init empty program
		glAttachShader(program, vertexShader); //attach shaders
		glAttachShader(program, fragmentShader); //..
		glLinkProgram(program); //link program

		//log errors:
		int logSize = 0;
		char* log = new char[1024];
		glGetShaderInfoLog(vertexShader, 1024, &logSize, log);
		if (logSize > 0)
		{
			std::cout << "Vertex Errors:\n" << log << "\n";
			if (errorShader != nullptr)
				program = errorShader->GetProgram();
		}
		delete[] log;

		logSize = 0;
		log = new char[1024];
		glGetShaderInfoLog(fragmentShader, 1024, &logSize, log);
		if (logSize > 0)
		{
			std::cout << "Fragment Errors:\n" << log << "\n";
			if (errorShader != nullptr)
				program = errorShader->GetProgram();
		}
		delete[] log;

		glDeleteShader(vertexShader); //cleanup shaders as they are contained in the program
		glDeleteShader(fragmentShader); //..
		delete vertexFile;
		delete fragmentFile;
	}

	Shader(bool isErrorShader)
	{
		if (isErrorShader)
		{
			glm::uint vertexShader = glCreateShader(GL_VERTEX_SHADER); //init empty shader
			glm::uint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); //..
			glShaderSource(vertexShader, 1, &errorVert, NULL); //load shader source
			glCompileShader(vertexShader); //compile shader source
			glShaderSource(fragmentShader, 1, &errorFrag, NULL); //..
			glCompileShader(fragmentShader); //..

			program = glCreateProgram(); //init empty program
			glAttachShader(program, vertexShader); //attach shaders
			glAttachShader(program, fragmentShader); //..
			glLinkProgram(program); //link program

			glDeleteShader(vertexShader); //cleanup shaders as they are contained in the program
			glDeleteShader(fragmentShader); //..
		}
		else
		{
			if (errorShader != nullptr) //if error shader exists
				program = errorShader->GetProgram(); //set our program to the error shader
			else
				program = 0; //set program to 0 to prevent OpenGL errors
		}
	}

	Shader()
	{
		if (errorShader != nullptr) //if error shader exists
			program = errorShader->GetProgram(); //set our program to the error shader
		else
			program = 0; //set program to 0 to prevent OpenGL errors
	}

	void SetUniforms()
	{
		glm::uint oldProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&oldProgram)); //get the active program
		glUseProgram(program); //use our prog
		glUniformMatrix4fv(glGetUniformLocation(program, "matrix"), 1, false, glm::value_ptr(mainCamera->GetCombinedMatrix())); //set the uniforms
		glUniform2f(glGetUniformLocation(program, "screenSize"), glm::floor(screenWidth), glm::floor(screenHeight));
		glUniform1i(glGetUniformLocation(program, "shadowMap"), 2);
		glUniform1i(glGetUniformLocation(program, "depthMap"), 3);
		glUseProgram(oldProgram); //activate the previously active prog
	}

	void SetUniforms(glm::mat4 sunMatrix, glm::vec3 sunPos)
	{
		glm::uint oldProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&oldProgram)); //get the active program
		glUseProgram(program); //use our prog
		glUniformMatrix4fv(glGetUniformLocation(program, "matrix"), 1, false, glm::value_ptr(mainCamera->GetCombinedMatrix())); //set the uniforms
		glUniform2f(glGetUniformLocation(program, "screenSize"), glm::floor(screenWidth), glm::floor(screenHeight));
		glUniform1i(glGetUniformLocation(program, "shadowMap"), 2);
		glUniform1i(glGetUniformLocation(program, "depthMap"), 3);
		glUniformMatrix4fv(glGetUniformLocation(program, "sunMatrix"), 1, false, glm::value_ptr(sunMatrix)); //set the uniforms
		glUniform3f(glGetUniformLocation(program, "sunPos"), sunPos.x, sunPos.y, sunPos.z);

		glUseProgram(oldProgram); //activate the previously active prog
	}

	glm::uint GetProgram()
	{
		return program;
	}

	void Use()
	{
		glUseProgram(program);
	}

	~Shader()
	{
		if (this != errorShader && errorShader != nullptr) //if we aren't deleting the error shader and the error shader exists
		{
			if (program == errorShader->GetProgram()) //if this program is not the error shader 
				glDeleteProgram(program); //we can delete our program
		}
		else //if we are the error shader or the error shader doesn't exist
			glDeleteProgram(program); //we can delete our program
	}
};

class Texture
{
protected:
	GLenum GLFormat;
	unsigned int texture = 0;
	int width, height;
	bool successful = false;

public:
	Texture()
	{
		width = 0;
		height = 0;
		GLFormat = GL_R;
		successful = false;
		texture = 0;
	}

	Texture(const char* path)
	{
		int comp = 0;
		unsigned char* imageData;
		imageData = stbi_load(path, &width, &height, &comp, 0);
		if (imageData != nullptr)
		{
			switch (comp)
			{
			case(1):
				GLFormat = GL_R;
				break;
			case(2):
				GLFormat = GL_RG;
				break;
			case(3):
				GLFormat = GL_RGB;
				break;
			case(4):
				GLFormat = GL_RGBA;
				break;
			default:
				GLFormat = GL_R;
				successful = false;
				return;
			}
			unsigned int oldTexture = 0;
			glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<int*>(&oldTexture));
			glActiveTexture(GL_TEXTURE8); //select texture unit 8 (we have this reserved for creating textures)
			glGenTextures(1, &texture); //gen empty tex
			glBindTexture(GL_TEXTURE_2D, texture); //bind it
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP); //set wrapping values
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP); //..
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); //set filter values
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); //..
			glTexImage2D(GL_TEXTURE_2D, 0, GLFormat, width, height, 0, GLFormat, GL_UNSIGNED_BYTE, imageData); //populate texture
			glGenerateMipmap(GL_TEXTURE_2D); //generate mipmap
			glActiveTexture(oldTexture);
			successful = true;
		}
		else 
		{
			successful = false;
		}
		stbi_image_free(imageData);
	}

	Texture(int _width, int _height, GLenum internalFormat, GLenum format)
	{
		width = _width;
		height = _height;
		unsigned int oldTexture = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<int*>(&oldTexture));
		glActiveTexture(GL_TEXTURE8); //select texture unit 8 (we have this reserved for creating textures)
		glGenTextures(1, &texture); //gen empty tex
		glBindTexture(GL_TEXTURE_2D, texture); //bind it
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); //set wrapping values
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); //..
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, internalFormat, format, (void*)NULL); //allocate memory
		glActiveTexture(oldTexture);
		GLFormat = internalFormat;
		successful = true;
		PrintGLErrors();
	}

	Texture(int _width, int _height, GLenum internalFormat, GLenum format, GLenum type)
	{
		width = _width;
		height = _height;
		unsigned int oldTexture = 0;
		glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<int*>(&oldTexture));
		glActiveTexture(GL_TEXTURE8); //select texture unit 8 (we have this reserved for creating textures)
		glGenTextures(1, &texture); //gen empty tex
		glBindTexture(GL_TEXTURE_2D, texture); //bind it
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); //set wrapping values
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); //..
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0); //disable mipmaps
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, (void*)NULL); //allocate memory
		glActiveTexture(oldTexture);
		GLFormat = internalFormat;
		successful = true;
		PrintGLErrors();
	}

	Texture(int _width, int _height, bool multisample, GLenum internalFormat, GLenum format, GLenum type)
	{
		if (multisample)
		{
			width = _width;
			height = _height;
			unsigned int oldTexture = 0;
			glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<int*>(&oldTexture));
			glActiveTexture(GL_TEXTURE8); //select texture unit 8 (we have this reserved for creating textures)
			glGenTextures(1, &texture); //gen empty tex
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture); //bind it
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0); //disable mip mapping
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, internalFormat, width, height, false); //allocate memory
			glActiveTexture(oldTexture);
			GLFormat = internalFormat;
		}
		else
			Texture::Texture(_width, _height, internalFormat, format, type);
	}

	~Texture()
	{
		glDeleteTextures(1, &texture);
	}

	void Use(int unit)
	{
		glBindTextureUnit(unit, texture);
	}

	unsigned int GetTexture()
	{
		return texture;
	}
};

class GLFramebuffer
{
protected:
	unsigned int framebuffer;
	Texture* color;
	Texture* depth;
	int width, height;

public:
	GLFramebuffer()
	{
		glGenFramebuffers(1, &framebuffer);
		color = new Texture((int)screenWidth, (int)screenHeight, GL_RGB, GL_UNSIGNED_BYTE);
		depth = new Texture((int)screenWidth, (int)screenHeight, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
		width = (int)screenWidth;
		height = (int)screenHeight;
		unsigned int oldFramebuffer = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<int*>(&oldFramebuffer));
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->GetTexture(), 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth->GetTexture(), 0);
		int completeness = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (completeness != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "Framebuffer Depth Completeness: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << "\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
	}

	GLFramebuffer(int _width, int _height)
	{
		width = _width;
		height = _height;
		glGenFramebuffers(1, &framebuffer);
		color = new Texture(width, height, GL_RGB, GL_UNSIGNED_BYTE);
		depth = new Texture(width, height, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
		unsigned int oldFramebuffer = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<int*>(&oldFramebuffer));
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->GetTexture(), 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth->GetTexture(), 0);
		int completeness = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (completeness != GL_FRAMEBUFFER_COMPLETE)
		{
			std::cout << "Framebuffer Depth Completeness: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << "\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
	}

	GLFramebuffer(int width, int height, bool multisample)
	{
		unsigned int oldFramebuffer = 0;
		if (multisample)
		{
			glGenFramebuffers(1, &framebuffer);
			color = new Texture(width, height, true, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE);
			depth = new Texture(width, height, true, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<int*>(&oldFramebuffer));
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, color->GetTexture(), 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth->GetTexture(), 0);
			int completeness = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (completeness != GL_FRAMEBUFFER_COMPLETE)
			{
				std::cout << "Framebuffer Depth Completeness: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << "\n";
			}
		}
		glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
	}

	~GLFramebuffer()
	{
		glDeleteFramebuffers(1, &framebuffer);
	}

	void Use()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	}

	Texture* GetColor()
	{
		return color;
	}

	Texture* GetDepth()
	{
		return depth;
	}

	unsigned int GetFramebuffer()
	{
		return framebuffer;
	}
};

class GLFramebufferMS: public GLFramebuffer
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

class GLBuffer
{
protected:
	glm::uint buffer = 0;
	unsigned int size = 0;

public:
	GLBuffer(void* data, unsigned int _size)
	{
		glCreateBuffers(1, &buffer); //generate and initalize buffer
		glNamedBufferData(buffer, _size, data, GL_STATIC_DRAW); //populate it
		size = _size;
	}

	GLBuffer(const GLBuffer& other)
	{
		unsigned int _size = other.size;
		glCreateBuffers(1, &buffer); //generate and initalize our buffer
		glNamedBufferData(buffer, _size, (void*)0, GL_STATIC_DRAW); //populate our buffer with empty data
		glCopyNamedBufferSubData(other.buffer, buffer, 0, 0, _size); //copy other.buffer into buffer
		size = _size;
	}

	GLBuffer()
	{
		buffer = 0;
		size = 0;
	}

	~GLBuffer()
	{
		glDeleteBuffers(1, &buffer);
	}

	glm::uint const GetBuffer()
	{
		return buffer;
	}

	unsigned int const GetSize()
	{
		return size;
	}
};

class GLObject
{
protected:
	GLBuffer* attribBuffer;
	GLBuffer* indexBuffer;
	unsigned int indexCount = 0;
	unsigned int triCount = 0;
	glm::uint object = 0;

public:
	GLObject(void* attribData, unsigned int size)
	{
		attribBuffer = new GLBuffer(attribData, size); //create vertex attribute buffer
		indexBuffer = nullptr; //no index buffer
		triCount = size / sizeof(Vertex); //number of tris in buffer (5 floats per tri)
		indexCount = 0; //not using indexed rendering so doesn't matter
		glGenVertexArrays(1, &object); //generate the VAO
		glBindVertexArray(object); //bind it
		glBindBuffer(GL_ARRAY_BUFFER, attribBuffer->GetBuffer()); //bind the attrib buff to the VAO
		SetupAttributes(); //setup the vertex attributes
		glBindVertexArray(0); //unbind for safety
	}

	GLObject(void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize)
	{
		attribBuffer = new GLBuffer(attribData, attribSize); //create vertex attribute buffer
		indexBuffer = new GLBuffer(indexData, indexSize); //create vertex index buffer
		triCount = attribSize / sizeof(Vertex);
		indexCount = indexSize / sizeof(unsigned int);
		glGenVertexArrays(1, &object); //..
		glBindVertexArray(object); //..
		glBindBuffer(GL_ARRAY_BUFFER, attribBuffer->GetBuffer()); //..
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetBuffer()); //bind the index buff to VAO
		SetupAttributes(); //..
		glBindVertexArray(0); //..
	}

	GLObject(const GLObject& other)
	{
		attribBuffer = new GLBuffer(*other.attribBuffer); //create vertex attribute buffer
		triCount = other.triCount;
		indexCount = 0;
		if (other.indexBuffer != nullptr)
		{
			indexBuffer = new GLBuffer(*other.indexBuffer); //create vertex index buffer
			indexCount = other.indexCount;
		}
		glGenVertexArrays(1, &object); //..
		glBindVertexArray(object); //..
		glBindBuffer(GL_ARRAY_BUFFER, attribBuffer->GetBuffer()); //..
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetBuffer()); //bind the index buff to VAO
		SetupAttributes(); //..
		glBindVertexArray(0); //..
	}

	~GLObject()
	{
		delete attribBuffer; //delete buffers
		delete indexBuffer; //(delete on nullptr is safe)
		glDeleteVertexArrays(1, &object); //delete VAO
	}

	void SetupAttributes()
	{
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); //index, count, type, normalize?, stride, offset
		glEnableVertexAttribArray(0); //enable attribute
	}

	void Draw()
	{
		glBindVertexArray(object);
		if (indexBuffer != nullptr)
			glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)0); //draw mode, count, typeof index, offset
		else //if not using vertex indices
			glDrawArrays(GL_TRIANGLES, 0, triCount); //regular draw
		glBindVertexArray(0);
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

struct MaterialProperties
{
	float staticFriction;
	float dynamicFriction;
	float restitution;
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

public:
	Player(glm::vec3 _pos, glm::quat _rot, Model* model)
		:PhysicsObject(_pos, _rot, glm::vec3(1.00f), MaterialProperties{ 0.50f, 0.40f, 0.30f}, model)
	{
	}
	Player(glm::vec3 _pos, glm::quat _rot, const char* path)
		:PhysicsObject(_pos, _rot, glm::vec3(1.00f), MaterialProperties{ 0.50f, 0.40f, 0.30f }, path)
	{
	}

	void MoveDir(glm::vec2 dir)
	{
		moveDir += dir;
	}

	void Update()
	{	
		glm::vec4 worldV = glm::rotate(rot, mainCamera->GetAngle() + 0.50f * PI, glm::vec3(0.00f, 1.00f, 0.00f)) * glm::vec4(moveDir.x * moveSpeed, 0.00f, moveDir.y * -moveSpeed, 0.0);
		glm::vec2 v = glm::vec2(worldV.x, worldV.z); //get target move speed
		glm::vec3 current = FromPxVec(pBody->getLinearVelocity());
		glm::vec2 u = glm::vec2(current.x, current.z); //get current move speed
		glm::vec2 f = pBody->getMass() * (v - u) / moveTime;
		pBody->addForce(PxVec3(f.x, 0.00f, f.y), PxForceMode::eFORCE); //apply force
		PhysicsObject::Update();
	}
};

class Platform
	: public StaticObject
{
protected:
	bool enabled;

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
	const int shadowMapSize = 1024;

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
		shader->Use();
		//glUniformMatrix4fv(glGetUniformLocation(shader->GetProgram(), "sunMatrix"), 1, false, glm::value_ptr(CalculateCombinedMatrix()));
		shader->SetUniforms(CalculateCombinedMatrix(), pos);
		glViewport(0, 0, shadowMapSize, shadowMapSize);
		shadowBuffer->Use();
		glClear(GL_DEPTH_BUFFER_BIT);
	}

	void EndShadowPass()
	{
		glCullFace(GL_BACK);
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
	case(SDLK_SPACE):
		groundPlane->Disable();
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
	case(SDLK_SPACE):
		groundPlane->Enable();
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

void PrintGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cout << "OpenGL Error: " << std::hex << "0x" << err << std::dec << "\n";
	}
}