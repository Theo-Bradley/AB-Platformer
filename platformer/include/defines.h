#ifndef _myDefines
#define _myDefines

#ifdef _WIN32
#define exit(code) exit(##code##)
#endif

#define PI glm::pi<float>()
#define DEFAULT_COLOR glm::vec3(0.899f, 0.745f, 0.369f)

#define Path(asset) (std::string(SDL_GetBasePath() + std::string("assets\\") + std::string(##asset##))).c_str()

#define key_Forward SDLK_W
#define key_Left SDLK_A
#define key_Backward SDLK_S
#define key_Right SDLK_D
#define key_PlatformToggle SDLK_Q
#define key_Jump SDLK_SPACE

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

float screenWidth, screenHeight;
float sensitivity = 0.66f;
unsigned long long int eTime = 0;
unsigned long long dTime = 1;

class Shader;
Shader* errorShader = nullptr;

class Camera;
Camera* mainCamera;

class Platform;
std::vector<Platform*> APlatforms;
std::vector<Platform*> BPlatforms;

class Model;
std::vector<Model*> drawModels;

class PhysicsObject;
std::vector<PhysicsObject*> pObjects;
bool platformToggle = false;

std::vector<std::string> playerFrames = { Path("models/robot_idle.obj"), Path("models/robot_moving.obj") };
#endif

using namespace physx;