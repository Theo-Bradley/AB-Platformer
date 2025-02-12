#ifdef _WIN32
#define exit(code) exit(##code##)
#endif

#define PI glm::pi<float>()

#define Path(asset) (std::string(SDL_GetBasePath() + std::string("assets\\") + std::string(##asset##))).c_str()

#define key_Forward SDLK_W
#define key_Left SDLK_A
#define key_Backward SDLK_S
#define key_Right SDLK_D

using namespace physx;

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