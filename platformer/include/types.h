#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "GL/glew.h"
#include "SDL3/SDL_opengl.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "PhysX/PxPhysicsAPI.h"

using namespace physx;

#ifdef _WIN32
#define exit(code) exit(##code##)
#endif

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

class Shader;
class Camera;

class PhysicsErrorCallback : public PxErrorCallback
{
public:
	virtual void reportError(PxErrorCode::Enum code, const char* message, const char* file, int line)
	{
		// error processing implementation
		std::cout << std::dec << "PhysX Error: " << code << ": " << message << " in " << file << " at " << line << "\n";
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
unsigned long long int eTime = 0;

float screenWidth, screenHeight;

class Camera
{
protected:
	float angle;
	float height;
	float distance;
	glm::mat4 view;
	glm::mat4 projection;

	glm::mat4 LookAt(glm::vec3 target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		glm::vec3 cameraPos = glm::vec3(distance * glm::cos(angle), height, distance * -glm::sin(angle));
		return glm::lookAt(target + cameraPos, target, glm::vec3(0.00f, 1.00f, 0.00f));
	}

	glm::mat4 Project()
	{
		return glm::perspective(glm::radians(45.f), screenWidth/screenHeight, 0.01f, 10.00f);
	}

public:
	Camera(glm::vec3 _target, float _angle, float _height)
	{
		angle = _angle;
		height = _height;
		distance = 2.00f;
		view = LookAt(_target);
		projection = Project();
	}

	glm::mat4 GetCombinedMatrix()
	{
		return projection * view;
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
		data = new char[size]; //create string to hold source
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

class GLBuffer
{
protected:
	glm::uint buffer = 0;

public:

	GLBuffer(void* data, unsigned int size)
	{
		glCreateBuffers(1, &buffer); //generate and initalize buffer
		glNamedBufferData(buffer, size, data, GL_STATIC_DRAW); //populate it
	}

	GLBuffer()
	{
		buffer = 0;
	}

	~GLBuffer()
	{
		glDeleteBuffers(1, &buffer);
	}

	glm::uint const GetBuffer()
	{
		return buffer;
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
		triCount = size / (sizeof(float) * 5); //number of tris in buffer (5 floats per tri)
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
		triCount = attribSize / (sizeof(float) * 5);
		indexCount = indexSize / sizeof(int);
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
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0); //index, count, type, normalize?, stride, offset
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

void KeyDown(SDL_KeyboardEvent key)
{
	switch (key.key)
	{
	case (SDLK_ESCAPE):
		quit(0);
		break;
	}
}

void KeyUp(SDL_KeyboardEvent key)
{
}

void PrintGLErrors()
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cout << std::hex << err << "\n";
		std::cout << std::dec << std::endl;
	}
}