#include "types.h"

int init();
void HandleEvents();
void Draw();

float plane[] = {
	-0.5f, -0.5f, 0.0f,
	0.0f, 0.0f,
	0.5f, -0.5f, 0.0f,
	1.0f, 0.0f,
	0.5f, 0.5f, 0.0f,
	1.0f, 1.0f,
	-0.5f, 0.5f, 0.0f,
	0.0f, 1.0f };

unsigned int planeIndices[6] = {
	0, 1, 2,
	2, 3, 0
};

float cube[] = {
	-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f, -0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 0.0f,

	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 1.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,

	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,

	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f, -0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f, -0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f, -0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f, -0.5f, -0.5f,  0.0f, 1.0f,

	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f,
	 0.5f,  0.5f, -0.5f,  1.0f, 1.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	 0.5f,  0.5f,  0.5f,  1.0f, 0.0f,
	-0.5f,  0.5f,  0.5f,  0.0f, 0.0f,
	-0.5f,  0.5f, -0.5f,  0.0f, 1.0f
};

PhysicsObject* testObj;
PhysicsObject* test2Obj;
Shader* testShader;
Shader* outlineShader;
File* testFile;
Model* testModel;

int main(int argc, char** argv)
{
	bool running = true;
	init();

	//testObj = new GLObject(cube, sizeof(cube));
	//testObj = new PhysicsObject(glm::vec3(0.00f), glm::vec3(glm::radians(12.00f), glm::radians(45.00f), 0.00f), glm::vec3(1.00f), MaterialProperties{0.50f, 0.40f, 0.30f}, cube, sizeof(cube));
	//test2Obj = new PhysicsObject(glm::vec3(0.00f, 0.00f, -1.50f), glm::vec3(glm::radians(12.00f), glm::radians(45.00f), 0.00f), glm::vec3(1.00f), MaterialProperties{ 0.50f, 0.40f, 0.30f }, cube, sizeof(cube));
	testShader = new Shader(Path("basic.vert").c_str(), Path("basic.frag").c_str());
	testShader->SetUniforms();
	outlineShader = new Shader(Path("outline.vert").c_str(), Path("outline.frag").c_str());
	outlineShader->SetUniforms();

	PxMaterial* planeMat = pPhysics->createMaterial(0.50f, 0.40f, 0.90f);
	PxRigidStatic* plane = PxCreatePlane(*pPhysics, PxPlane(PxVec3(0.00f, -1.00f, 0.00f), PxVec3(0.00f, 1.00f, 0.00f)), *planeMat);
	pScene->addActor(*plane);

	testModel = new Model(Path("models/cube.fbx").c_str());
	
	eTime = SDL_GetTicks();

	while (running)
	{
		unsigned long long oldETime = eTime;
		eTime = SDL_GetTicks();
		dTime = eTime - oldETime;
		HandleEvents();
		float fTime = static_cast<float>(dTime + 1) / 1000.00f; //+1 so that fTime is never 0 (crashes physx)
		pScene->simulate(fTime); //simulate by delta time
		pScene->fetchResults(true); //wait for results

		//testObj->Update();

		Draw();
	}
	return quit(0);
}

void HandleEvents()
{
	SDL_Event event;

	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_EVENT_QUIT:
			quit(0);
			break;
		case SDL_EVENT_KEY_DOWN:
			KeyDown(event.key);
			break;
		case SDL_EVENT_KEY_UP:
			KeyUp(event.key);
			break;
		}
	}
}

int init()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
	{
		return -1; //failed to init SDL
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE); //use OpenGL core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4); //use OpenGL 4.x
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5); //use OpenGL x.5
	screenWidth = 1920.00f;
	screenHeight = 1080.00f;
	window = SDL_CreateWindow("Platformer", (int)screenWidth, (int)screenHeight, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	SDL_GLContext glContext = SDL_GL_CreateContext(window); //Create GL context
	if (!glContext) //if failed to create context
		quit(-1); //close
	GLenum glew = glewInit(); //init glew
	if (glew != GLEW_OK) //if glew didn't work
		quit(-1); //close
	eTime = SDL_GetTicks();

	pFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, pAlloc, pError); //create the "foundation"
	pPvd = PxCreatePvd(*pFoundation); //create a pvd instance
	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10); //set up the pvd transport
	pPvd->connect(*transport, PxPvdInstrumentationFlag::eALL); //start pvd

	pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pFoundation, physx::PxTolerancesScale(), true, pPvd); //create the physics solver

	PxSceneDesc sceneDesc(pPhysics->getTolerancesScale()); //create the scene description
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f); //set gravity to g
	pDispatcher = PxDefaultCpuDispatcherCreate(2); //create the default cpu task dispatcher
	sceneDesc.cpuDispatcher = pDispatcher; //..
	sceneDesc.filterShader = PxDefaultSimulationFilterShader; //create the default shader
	pScene = pPhysics->createScene(sceneDesc); //create the scene

	glEnable(GL_DEPTH_TEST);
	errorShader = new Shader(true);
	errorShader->Use();
	mainCamera = new Camera(glm::vec3(0.00f), glm::radians(-90.00f), 1.00f);
	depthBuffer = new GLFramebuffer();

	glClearColor(0.529f, 0.808f, 0.922f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	return 0;
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

void Draw()
{
	depthBuffer->Use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	testShader->Use();
	//testObj->Draw();
	//test2Obj->Draw();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	outlineShader->Use();
	glUniform1i(glGetUniformLocation(outlineShader->GetProgram(), "depth"), 3);
	glBindTextureUnit(3, depthBuffer->GetDepth()->GetTexture());
	//testObj->Draw();
	//test2Obj->Draw();
	testModel->Draw();

	PrintGLErrors();

	SDL_GL_SwapWindow(window);
}