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
	bool isTrigger = false;
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
		glm::quat p = rot;
		glm::quat q = _quat;
		glm::quat res;
		float a = 1.00f;
		float b = 0;
		res.w = p.w * q.w - p.x * q.x - p.y * q.y - p.z * q.z;
		if (res.w < 0)
			b = -1.00f;
		else
			b = 1.00f;
		res.w = glm::modf(res.w, a) * b;
		res.x = p.w * q.x + p.x * q.w + p.y * q.z - p.z * q.y;
		if (res.x < 0)
			b = -1.00f;
		else
			b = 1.00f;
		res.x = glm::modf(res.x, a) * b;
		res.y = p.w * q.y + p.y * q.w + p.z * q.x - p.x * q.z;
		if (res.y < 0)
			b = -1.00f;
		else
			b = 1.00f;
		res.y = glm::modf(res.y, a) * b;
		res.z = p.w * q.z + p.z * q.w + p.x * q.y - p.y * q.x;
		if (res.z < 0)
			b = -1.00f;
		else
			b = 1.00f;
		res.z = glm::modf(res.z, a) * b;
		rot = res;
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

	glm::vec3 LocalToWorldPoint(glm::vec3 point)
	{
		glm::mat4 translate = glm::translate(glm::mat4(1.00f), pos);
		glm::mat4 rotate = glm::mat4_cast(rot);
		glm::mat4 scale = glm::scale(glm::mat4(1.00f), scal);
		glm::vec3 val = translate * rotate * scale * glm::vec4(point, 1.00f);
		return val;
	}
};

class Camera
{
protected:
	float angle;
	float inclination;
	float distance;
	glm::vec3 position;
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 forward;

	glm::mat4 LookAt(glm::vec3 target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		position = glm::vec3(distance * glm::sin(angle), distance * glm::sin(inclination), distance * glm::cos(angle));
		glm::mat4 matrix = glm::lookAt(target + position, target, glm::vec3(0.00f, 1.00f, 0.00f));
		forward = matrix * glm::vec4(0.00f, 0.00f, 1.00f, 0.00f);
		return matrix;
	}

	glm::mat4 LookAt(Object target)
	{
		//sin(theta) z coord
		//cos(theta) x coord
		position = glm::vec3(distance * glm::sin(angle), distance * glm::sin(inclination), distance * glm::cos(angle));
		glm::mat4 matrix = glm::lookAt(target.GetPosition() + position, target.GetPosition(), glm::vec3(0.00f, 1.00f, 0.00f));
		forward = matrix * glm::vec4(0.00f, 0.00f, 1.00f, 0.00f);
		return matrix;
	}

	glm::mat4 Project()
	{
		return glm::perspective(glm::radians(45.f), screenWidth / screenHeight, 0.1f, 40.00f);
	}

public:
	Camera(glm::vec3 _target, float _angle)
	{
		angle = _angle;
		inclination = glm::radians(45.00f);
		distance = 3.00f;
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

	glm::vec3 GetPosition()
	{
		return position;
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
		glUniform3f(glGetUniformLocation(program, "camPos"), mainCamera->GetPosition().x, mainCamera->GetPosition().y, mainCamera->GetPosition().z);
		glUniform1i(glGetUniformLocation(program, "shadowMap"), 2);
		glUniform1i(glGetUniformLocation(program, "normalMap"), 3);
		glUniform1i(glGetUniformLocation(program, "depthMap"), 4);

		for (int i = 0; i < numLights; i++)
		{
			float lightIntensity = 0.4f;
			std::string uniformName = "lights[" + std::to_string(i) + "]";
			glUniform3f(glGetUniformLocation(program, (uniformName + ".position").c_str()), 0.5f * lightIntensity, 1.5f * lightIntensity, 0.5f * lightIntensity);
			glUniform3f(glGetUniformLocation(program, (uniformName + ".color").c_str()), 1.0f, 0.2f, 0.2f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".constant").c_str()), 1.0f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".linear").c_str()), 0.12f);
			glUniform1f(glGetUniformLocation(program, (uniformName + ".quadratic").c_str()), 2.00f);
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
	//default construcotr
	//creates a complete buffer the size of the screen
	GLFramebuffer()
	{
		glGenFramebuffers(1, &framebuffer);
		color = new Texture((int)screenWidth, (int)screenHeight, GL_RGB, GL_UNSIGNED_BYTE); //create a texture for the color attachment
		depth = new Texture((int)screenWidth, (int)screenHeight, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT); //create a depth texture
		width = (int)screenWidth;
		height = (int)screenHeight;
		unsigned int oldFramebuffer = 0;
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<int*>(&oldFramebuffer)); //get old framebuffer so we can rebind it after
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color->GetTexture(), 0); //attach textures
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth->GetTexture(), 0);
		int completeness = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (completeness != GL_FRAMEBUFFER_COMPLETE) //check if framebuffer is complete
		{
			std::cout << "Framebuffer Depth Completeness: 0x" << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << "\n";
		}
		glBindFramebuffer(GL_FRAMEBUFFER, oldFramebuffer);
	}

	//create framebuffer of width and height
	GLFramebuffer(int _width, int _height, bool multisample = false)
	{
		unsigned int oldFramebuffer = 0;
		if (multisample) //if we want to multisample create a MS buffer
		{
			width = _width;
			height = _height;
			glGenFramebuffers(1, &framebuffer);
			color = new Texture(width, height, GL_RGB, GL_RGB, GL_UNSIGNED_BYTE, true); //create a texture for the color attachment
			depth = new Texture(width, height, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, true); //create a depth texture
			glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, reinterpret_cast<int*>(&oldFramebuffer)); //get old framebuffer so we can rebind it after
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, color->GetTexture(), 0); //attach textures
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depth->GetTexture(), 0);
			int completeness = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (completeness != GL_FRAMEBUFFER_COMPLETE) //check if framebuffer is complete
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
	GLObject(void* attribData, unsigned int size, int attribOffset = 0)
	{
		attribBuffer = new GLBuffer(attribData, size); //create vertex attribute buffer
		indexBuffer = nullptr; //no index buffer
		triCount = size / sizeof(Vertex); //number of tris in buffer (5 floats per tri)
		indexCount = 0; //not using indexed rendering so doesn't matter
		glGenVertexArrays(1, &object); //generate the VAO
		glBindVertexArray(object); //bind it
		glBindBuffer(GL_ARRAY_BUFFER, attribBuffer->GetBuffer()); //bind the attrib buff to the VAO
		SetupAttributes(attribOffset); //setup the vertex attributes
		glBindVertexArray(0); //unbind for safety
	}

	GLObject(void* attribData, unsigned int attribSize, void* indexData, unsigned int indexSize, int attribOffset = 0)
	{
		attribBuffer = new GLBuffer(attribData, attribSize); //create vertex attribute buffer
		indexBuffer = new GLBuffer(indexData, indexSize); //create vertex index buffer
		triCount = attribSize / sizeof(Vertex);
		indexCount = indexSize / sizeof(unsigned int);
		glGenVertexArrays(1, &object); //..
		glBindVertexArray(object); //..
		glBindBuffer(GL_ARRAY_BUFFER, attribBuffer->GetBuffer()); //..
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer->GetBuffer()); //bind the index buff to VAO
		SetupAttributes(attribOffset); //..
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

	void SetupAttributes(int offset)
	{
		glVertexAttribPointer(0 + offset, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); //index, count, type, normalize?, stride, offset
		glEnableVertexAttribArray(0 + offset); //enable attribute
		glVertexAttribPointer(1 + offset, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Vertex::normal));
		glEnableVertexAttribArray(1 + offset);
	}

	void SetupAttributes(int offset, int buffer)
	{
		glBindBuffer(GL_ARRAY_BUFFER, buffer); //bind buffer
		glVertexAttribPointer(0 + offset, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0); //index, count, type, normalize?, stride, offset
		glEnableVertexAttribArray(0 + offset); //enable attribute
		glVertexAttribPointer(1 + offset, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Vertex::normal));
		glEnableVertexAttribArray(1 + offset);
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

	GLBuffer* GetAttribBuffer()
	{
		return attribBuffer;
	}

	GLBuffer* GetIndexBuffer()
	{
		return indexBuffer;
	}

	unsigned int GetObject()
	{
		return object;
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

	void Reset()
	{
		pressed = false;
	}
};

enum class AnimationLoopType
{
	loop,
	clamp,
	stop
};

template <typename T>
class Animation
{
protected:
	T* frames;
	unsigned int numFrames;
	AnimationLoopType loopType;
	bool isPlaying = false;
	bool isPaused = false;
	unsigned long long startTime = 0; //start time in MS
	unsigned long long pauseTime = 0; //time anim was paused in MS
	float frameTime;
	float time = 0.00f;

public:
	Animation()
	{
		frames = nullptr;
		numFrames = 0;
		loopType = AnimationLoopType::stop;
		frameTime = 0;
	}

	Animation(T* copyFrames, unsigned int _numFrames, float timePerFrame, AnimationLoopType _loopType = AnimationLoopType::stop)
	{
		loopType = _loopType;
		frameTime = timePerFrame;
		numFrames = _numFrames;
		frames = new T[numFrames];

		for (unsigned int i = 0; i < numFrames; i++)
		{
			frames[i] = copyFrames[i];
		}
	}

	~Animation()
	{
		delete[] frames;
	}

	void Start()
	{
		startTime = eTime; //start from begining
		isPlaying = true;
		isPaused = false;
	}

	void Stop()
	{
		isPlaying = false;
		isPaused = false;
	}

	void Play()
	{
		if (isPaused) //if was paused
		{
			startTime = startTime + (eTime - pauseTime); //advance startTime by time elapsed since paused
		}
		else
		{
			if (!isPlaying) //if was stopped
				startTime = eTime; //start from begining
		}
		isPlaying = true;
		isPaused = false;
	}

	void Pause()
	{
		isPlaying = false;
		isPaused = true;
		pauseTime = eTime;
	}

	T GetFrame()
	{
		unsigned int frame = 0;
		if (isPlaying)
		{
			time = (eTime - startTime) / 1000.f;
		}
		if (isPaused)
		{
			time = (pauseTime - startTime) / 1000.f;
		}

		switch (loopType)
		{
		case (AnimationLoopType::clamp):
		{
			if (time >= frameTime * (numFrames - 1)) //if time is greater than animation length
			{
				return frames[numFrames - 1]; //return last frame
			}
			break;
		}
		case (AnimationLoopType::loop):
		{
			if (time >= frameTime * (numFrames - 1)) //if time is greater than animation length
			{
				startTime += (int)glm::round(frameTime * 1000) * (numFrames - 1); //increment start time by one animation length
				time = (eTime - startTime) / 1000.f; //recalc the time
			}
			break;
		}
		case (AnimationLoopType::stop):
		{
			if (time >= frameTime * (numFrames - 1))
			{
				isPlaying = false;
				isPaused = false;
				return frames[numFrames - 1]; //return last frame
			}
			break;
		}
		}
		frame = glm::floor(time / frameTime);
		unsigned int nextFrame = (frame + 1) % numFrames;
		return frames[frame] + (time / frameTime) * (frames[nextFrame] - frames[frame]);
	}

	bool IsPlaying()
	{
		return isPlaying;
	}
};

struct BoxCollider
{
	PxVec3 center;
	PxVec3 size;
};

struct CylinderCollider
{
	PxVec3 center;
	float length;
	float radius;
};

struct PhysicsData
{
	void* pointer;
	bool isGround;
	bool isDynamic;
	bool isCoin;
	bool isPiston;
};