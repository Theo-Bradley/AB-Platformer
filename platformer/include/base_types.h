#include "functions.h"

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 uv;
};

struct MaterialProperties
{
	float staticFriction;
	float dynamicFriction;
	float restitution;
};

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
	glm::vec3 forward;

	glm::mat4 LookAt(glm::vec3 target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		glm::vec3 cameraPos = glm::vec3(distance * glm::sin(angle), distance * glm::sin(inclination), distance * glm::cos(angle));
		glm::mat4 matrix = glm::lookAt(target + cameraPos, target, glm::vec3(0.00f, 1.00f, 0.00f));
		forward = matrix * glm::vec4(0.00f, 0.00f, 1.00f, 0.00f);
		return matrix;
	}

	glm::mat4 LookAt(Object target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		glm::vec3 cameraPos = glm::vec3(distance * glm::sin(angle), distance * glm::sin(inclination), distance * glm::cos(angle));
		glm::mat4 matrix = glm::lookAt(target.GetPosition() + cameraPos, target.GetPosition(), glm::vec3(0.00f, 1.00f, 0.00f));
		forward = matrix * glm::vec4(0.00f, 0.00f, 1.00f, 0.00f);
		return matrix;
	}

	glm::mat4 Project()
	{
		return glm::perspective(glm::radians(45.f), screenWidth / screenHeight, 0.1f, 15.00f);
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

	glm::vec3 GetForward()
	{
		return forward;
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
		constexpr int numLights = 1;
		glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&oldProgram)); //get the active program
		glUseProgram(program); //use our prog
		glUniformMatrix4fv(glGetUniformLocation(program, "matrix"), 1, false, glm::value_ptr(mainCamera->GetCombinedMatrix())); //set the uniforms
		glUniform3f(glGetUniformLocation(program, "camDir"), mainCamera->GetForward().x, -mainCamera->GetForward().y, mainCamera->GetForward().z);
		glUniform1i(glGetUniformLocation(program, "shadowMap"), 2);
		glUniform1i(glGetUniformLocation(program, "normalMap"), 3);
		glUniform1i(glGetUniformLocation(program, "depthMap"), 4);

		for (int i = 0; i < numLights; i++)
		{
			std::string uniformName = "lights[" + std::to_string(i) + "]";
			glUniform3f(glGetUniformLocation(program, (uniformName + ".position").c_str()), 0.5f, 1.5f, 0.5f);
			glUniform3f(glGetUniformLocation(program, (uniformName + ".color").c_str()), 1.0f, 0.2f, 0.2f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".constant").c_str()), 1.0f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".linear").c_str()), 0.12f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".quadratic").c_str()), 0.032f);
		}
		glUseProgram(oldProgram); //activate the previously active prog
	}

	void SetUniforms(glm::mat4 sunMatrix, glm::vec3 sunPos)
	{
		glm::uint oldProgram = 0;
		glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<int*>(&oldProgram)); //get the active program
		glUseProgram(program); //use our prog
		SetUniforms();
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

	Texture(int _width, int _height, GLenum internalFormat, GLenum format, GLenum type, bool multisample = false)
	{
		width = _width;
		height = _height;
		unsigned int oldTexture = 0;

		if (multisample)
		{
			glGetIntegerv(GL_ACTIVE_TEXTURE, reinterpret_cast<int*>(&oldTexture));
			glActiveTexture(GL_TEXTURE8); //select texture unit 8 (we have this reserved for creating textures)
			glGenTextures(1, &texture); //gen empty tex
			glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texture); //bind it
			glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0); //disable mip mapping
			glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, internalFormat, width, height, false); //allocate memory
			glActiveTexture(oldTexture);
			GLFormat = internalFormat;
			successful = true;
		}
		else
		{
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
		}
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

	GLFramebuffer(int _width, int _height, bool multisample = false)
	{
		unsigned int oldFramebuffer = 0;
		if (multisample)
		{
			width = _width;
			height = _height;
			glGenFramebuffers(1, &framebuffer);
			color = new Texture(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, true);
			depth = new Texture(width, height, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true);
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
		else
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
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Vertex::normal));
		glEnableVertexAttribArray(1);
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