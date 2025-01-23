#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"
#include "GL/glew.h"
#include "SDL3/SDL_opengl.h"
#include <iostream>
#include <fstream>
#include <filesystem>


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
"	color = vec4(243.0/255.0, 223.0/255.0, 162.0/255.0, 1.0);\n"
"}\n"
};

int quit(int code);

SDL_Window* window;
SDL_GLContext glContext;

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
	GLuint buffer = 0;

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

	GLuint const GetBuffer()
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
	GLuint object = 0;

public:

	GLObject(void* attribData, unsigned int size)
	{
		attribBuffer = new GLBuffer(attribData, size); //create vertex attribute buffer
		indexBuffer = nullptr; //no index buffer
		triCount = size / sizeof(float);
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
		triCount = attribSize / sizeof(float);
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
	GLuint program = 0;

public:
	Shader(const char* vertexPath, const char* fragmentPath)
	{
		File* vertexFile = new File(vertexPath);
		File* fragmentFile = new File(fragmentPath);
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); //init empty shader
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); //..
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
		}
		delete[] log;

		logSize = 0;
		log = new char[1024];
		glGetShaderInfoLog(fragmentShader, 1024, &logSize, log);
		if (logSize > 0)
		{
			std::cout << "Fragment Errors:\n" << log << "\n";
		}
		delete[] log;

		glDeleteShader(vertexShader); //cleanup shaders as they are contained in the program
		glDeleteShader(fragmentShader); //..
	}

	Shader()
	{
		program = 0;
	}

	GLuint GetProgram()
	{
		return program;
	}

		/*Shader(const char* _vertPath, const char* _fragPath)
		{
			LoadShaderFromFile(_vertPath, _fragPath);
		}

		Shader(std::string _vertPath, std::string _fragPath)
		{
			LoadShaderFromFile(_vertPath.c_str(), _fragPath.c_str());
		}*/

		/*bool LoadShaderFromFile(const char* vertPath, const char* fragPath)
		{
			//load source from file
			std::ifstream file;
			std::string currLine;
			file.open(vertPath, std::ios::in); //load file to read as binary at the end
			std::string vertSource; //create string to hold source
			while (std::getline(file, currLine)) //load file
			{
				vertSource.append(currLine); //write in each line
				vertSource.append("\n"); //append the newline that got eaten by getline
			}
			file.close(); //close file
			file.open(fragPath, std::ios::in); //..
			std::string fragSource; //..
			while (std::getline(file, currLine)) //load file
			{
				fragSource.append(currLine); //..
				fragSource.append("\n"); //..
			}
			file.close(); //..

			const char* cstrVertSource = vertSource.c_str(); //store the c string for 
			const char* cstrFragSource = fragSource.c_str();

			GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER); //init empty shader
			GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER); //..
			glShaderSource(vertexShader, 1, &cstrVertSource, NULL); //load shader source
			glCompileShader(vertexShader); //compile shader source
			glShaderSource(fragmentShader, 1, &cstrFragSource, NULL); //..
			glCompileShader(fragmentShader); //..

			GLint compStatus = GL_FALSE; //init
			glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &compStatus); //get GL_LINK_STATUS

			/*Get shader errors
			int logsize;
			char* log = new char[4096];
			glGetShaderInfoLog(fragmentShader, 4096, &logsize, log);
			if (logsize > 0)
				bool breakHere = true; //*/
			/*program = glCreateProgram(); //init empty program
			glAttachShader(program, vertexShader); //attach shaders
			glAttachShader(program, fragmentShader); //..
			glLinkProgram(program); //link program

			GLint linkStatus = GL_FALSE; //init
			glGetProgramiv(program, GL_LINK_STATUS, &linkStatus); //get GL_LINK_STATUS
			if (linkStatus == GL_FALSE) //if linking failed
			{
				program = errShader; //use the error shader
			}

			glDeleteShader(vertexShader); //cleanup shaders as they are contained in the program
			glDeleteShader(fragmentShader); //..

			isLoaded = true; //loaded shader

			return !(program == errShader); //return true if succeeded, false if the program is the errShader (failed)
		}

		~Shader()
		{
			if (isLoaded)
				glDeleteProgram(program);
		}*/
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