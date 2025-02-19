#include "types.h"

int init();
void HandleEvents();
void Draw();

Shader* outlineBufferShader;
Shader* outlineShader;
Shader* animatedOutlineShader;
Shader* animatedShadowShader;
Shader* animatedOutlineBufferShader;
File* testFile;
Model* testModel;
AnimatedModel* animModel;

Sun* sun;

int main(int argc, char** argv)
{
	bool running = true;
	init();

	LoadLevelTest();
	std::vector < std::string > paths;
	paths.push_back(Path("models/cube.obj"));
	paths.push_back(Path("models/cube1.obj"));

	animModel = new AnimatedModel(paths, 2, glm::vec3(0.00f, 1.50f, 0.00f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.00f));

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

		std::for_each(pObjects.begin(), pObjects.end(), [&](PhysicsObject* pObject) { pObject->Update(); });
		if (player != nullptr)
			player->Update(fTime);
		animModel->SetFac(glm::abs(glm::sin(0.001f * eTime)));

		mainCamera->Follow(player->GetPosition());
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
		case SDL_EVENT_MOUSE_MOTION:
			MouseMoved();
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			MouseWheel(event.wheel);
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
	SDL_SetWindowRelativeMouseMode(window, true); //capture the mouse
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
	glEnable(GL_CULL_FACE);
	errorShader = new Shader(true);
	errorShader->Use();
	shadowShader = new Shader(Path("shadow.vert"), Path("basic.frag"));
	animatedShadowShader = new Shader(Path("shadow_animated.vert"), Path("basic.frag"));
	outlineShader = new Shader(Path("outline.vert"), Path("outline.frag"));
	animatedOutlineShader = new Shader(Path("outline_animated.vert"), Path("outline.frag"));
	outlineBufferShader = new Shader(Path("outline_buffer.vert"), Path("outline_buffer.frag"));
	animatedOutlineBufferShader = new Shader(Path("outline_buffer_animated.vert"), Path("outline_buffer.frag"));
	mainCamera = new Camera(glm::vec3(0.00f), glm::radians(90.00f));
	depthBuffer = new GLFramebuffer();
	depthBuffer->GetColor()->Use(3);
	depthBuffer->GetDepth()->Use(4);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	player = new Player(glm::vec3(0.00f, 1.00f, 0.00f), glm::quat(glm::vec3(0.00f, glm::radians(12.00f), 0.00f)), Path("models/cube.obj"));
	sun = new Sun(glm::vec3(-5.00f, 4.00f, -1.00f));

	glClearColor(0.529f, 0.808f, 0.922f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	return 0;
}

void Draw()
{
	depthBuffer->Use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	outlineBufferShader->Use();
	outlineBufferShader->SetUniforms();
	player->Draw();
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); });
	groundPlane->Draw();
	animatedOutlineBufferShader->Use();
	animatedOutlineBufferShader->SetUniforms();
	animModel->Draw(animatedOutlineBufferShader);

	glEnable(GL_MULTISAMPLE);
	sun->StartShadowPass(shadowShader);
	player->Draw();
	groundPlane->Draw();
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); });
	animatedShadowShader->Use();
	animatedShadowShader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	animModel->Draw(animatedShadowShader);
	sun->EndShadowPass();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	outlineShader->Use();
	outlineShader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	groundPlane->Draw();
	if (player != nullptr)
		player->Draw();
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); });

	animatedOutlineShader->Use();
	animatedOutlineShader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	animModel->Draw(animatedOutlineShader);
	PrintGLErrors();

	SDL_GL_SwapWindow(window);
}