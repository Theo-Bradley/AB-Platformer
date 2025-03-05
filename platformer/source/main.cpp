#include "types.h"

int init();
void HandleEvents();
void Draw();

Shader* outlineBufferShader;
Shader* outlineShader;
Shader* animatedOutlineShader;
Shader* emissiveOutlineShader;
Shader* animatedShadowShader;
Shader* animatedOutlineBufferShader;
Shader* fullScreenShader;
Texture* mainMenuTexture;
File* testFile;
Model* testModel;
Sun* sun;
Model* levelTestModel;
Model* toggle;

int main(int argc, char** argv)
{
	bool running = true;
	init();

	LoadMainMenu();

	levelTestModel = new Model(Path("models/level_01_static.obj"), glm::vec3(0.0f, -0.50f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(1.0f));

	eTime = SDL_GetTicks();

	while (running)
	{
		unsigned long long oldETime = eTime;
		eTime = SDL_GetTicks();
		dTime = eTime - oldETime;
		HandleEvents(); //process inputs
		if (isMainMenu)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
			fullScreenShader->Use();
			drawModels[0]->Draw();
			glDisable(GL_CULL_FACE);
			SDL_GL_SwapWindow(window);
		}
		else
		{
			if (dieFlag)
			{
				UnloadLevel01();
				LoadLevel01();
				continue;
			}
			player->SetGrounded(false); //before pContactCallback set player.isGrounded to false (pContactCallback will set it to true if grounded)
			float fTime = static_cast<float>(dTime + 1) / 1000.00f; //+1 so that fTime is never 0 (crashes physx)
			pScene->simulate(fTime); //simulate by delta time
			pScene->fetchResults(true); //wait for results

			std::for_each(pObjects.begin(), pObjects.end(), [&](PhysicsObject* pObject) { pObject->Update(); });
			if (player != nullptr)
				player->Update();
			playerCloud->Update();
			stamBar->Update();
			for (unsigned long long int i = 0; i < numCoins; i++)
			{
				if (coins[i] != nullptr)
					coins[i]->Update();
			}

			mainCamera->Follow(player->GetPosition());
			Draw();
		}
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

	pPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *pFoundation, physx::PxTolerancesScale(), true); //create the physics solver

	PxSceneDesc sceneDesc(pPhysics->getTolerancesScale()); //create the scene description
	sceneDesc.gravity = PxVec3(0.0f, -9.81f, 0.0f); //set gravity to g
	pDispatcher = PxDefaultCpuDispatcherCreate(2); //create the default cpu task dispatcher
	sceneDesc.cpuDispatcher = pDispatcher; //..
	sceneDesc.filterShader = DefaultFilterShader; //create the default shader
	sceneDesc.simulationEventCallback = &pContactCallback;
	pScene = pPhysics->createScene(sceneDesc); //create the scene
	pScene->setSimulationEventCallback(&pContactCallback);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	//load shaders
	errorShader = new Shader(true);
	errorShader->Use();
	shadowShader = new Shader(Path("shadow.vert"), Path("basic.frag"));
	animatedShadowShader = new Shader(Path("shadow_animated.vert"), Path("basic.frag"));
	outlineShader = new Shader(Path("outline.vert"), Path("outline.frag"));
	animatedOutlineShader = new Shader(Path("outline_animated.vert"), Path("outline.frag"));
	emissiveOutlineShader = new Shader(Path("outline.vert"), Path("emissive_outline.frag"));
	outlineBufferShader = new Shader(Path("outline_buffer.vert"), Path("outline_buffer.frag"));
	animatedOutlineBufferShader = new Shader(Path("outline_buffer_animated.vert"), Path("outline_buffer.frag"));
	fullScreenShader = new Shader(Path("fullscreen.vert"), Path("fullscreen.frag"));
	toggleShader = new Shader(Path("fullscreen.vert"), Path("toggle.frag"));
	fullScreenShader->Use();
	glUniform1i(glGetUniformLocation(fullScreenShader->GetProgram(), "mainMenuTex"), 5);
	glUniform1i(glGetUniformLocation(fullScreenShader->GetProgram(), "textAtlas"), 6);
	toggleShader->Use();
	glUniform1i(glGetUniformLocation(toggleShader->GetProgram(), "tex"), 5);
	glUniform3fv(glGetUniformLocation(toggleShader->GetProgram(), "colour"), 1, glm::value_ptr(glm::vec3(1.f, 0.f, 0.f)));
	
	mainMenuTexture = new Texture(Path("textures/main_menu.png"));
	mainMenuTexture->Use(5);
	toggleTexture = new Texture(Path("textures/toggle.png"));
	mainCamera = new Camera(glm::vec3(0.00f), glm::radians(90.00f));
	depthBuffer = new GLFramebuffer();
	depthBuffer->GetColor()->Use(3); //set which texture units to use with the buffer
	depthBuffer->GetDepth()->Use(4);

	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); //enable multisampling

	sun = new Sun(glm::vec3(-5.00f, 5.00f, -1.00f));
	toggle = new Model(Path("models/plane.obj"), glm::vec3(0.00f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), glm::vec3(0.1f));

	glClearColor(0.529f, 0.808f, 0.922f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	return 0;
}

void Draw()
{
	Shader* shader;
	depthBuffer->Use();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	//outline buffer pass (draw worldspace normals and depth buffer)
	shader = outlineBufferShader;
	shader->Use();
	shader->SetUniforms();
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); }); //loop over each element in drawModel and draw it
	playerCloud->Draw();
	stamBar->Draw();
	for (unsigned long long int i = 0; i < numCoins; i++) //loop over each coin and draw it
	{
		if (coins[i] != nullptr)
			coins[i]->Draw();
	}
	std::for_each(pistonLights.begin(), pistonLights.end(), [&](PistonLight* pistonLight) { pistonLight->Draw(); });
	levelTestModel->Draw();
	shader = animatedOutlineBufferShader;
	shader->Use();
	shader->SetUniforms();
	player->Draw(shader);
	std::for_each(pistons.begin(), pistons.end(), [&](Piston* pistons) { pistons->Draw(shader); });

	//shadow pass
	glEnable(GL_MULTISAMPLE);
	shader = shadowShader;
	sun->StartShadowPass(shader);
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); });
	stamBar->Draw();
	playerCloud->Draw();
	for (unsigned long long int i = 0; i < numCoins; i++)
	{
		if (coins[i] != nullptr)
			coins[i]->Draw();
	}
	std::for_each(pistonLights.begin(), pistonLights.end(), [&](PistonLight* pistonLight) { pistonLight->Draw(); });
	levelTestModel->Draw();
	shader = animatedShadowShader;
	shader->Use();
	shader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	player->Draw(shader);
	std::for_each(pistons.begin(), pistons.end(), [&](Piston* pistons) { pistons->Draw(shader); });
	sun->EndShadowPass();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); //clear frame buffer
	//main pass
	shader = outlineShader;
	shader->Use();
	shader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	std::for_each(drawModels.begin(), drawModels.end(), [&](Model* drawModel) { drawModel->Draw(); });
	playerCloud->Draw();
	for (unsigned long long int i = 0; i < numCoins; i++)
	{
		if (coins[i] != nullptr)
			coins[i]->Draw();
	}
	levelTestModel->Draw();
	shader = animatedOutlineShader;
	shader->Use();
	shader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	player->Draw(shader);
	std::for_each(pistons.begin(), pistons.end(), [&](Piston* pistons) { pistons->Draw(shader); });

	//draw emissive objects
	shader = emissiveOutlineShader;
	shader->Use();
	shader->SetUniforms(sun->CalculateCombinedMatrix(), sun->GetPosition());
	stamBar->Draw(shader, outlineShader); //draw the staminaBar as emissive
	std::for_each(pistonLights.begin(), pistonLights.end(), [&](PistonLight* pistonLight) { pistonLight->Draw(shader, outlineShader); });

	//draw UI
	toggleShader->Use();
	toggle->Draw();
	
	PrintGLErrors();

	SDL_GL_SwapWindow(window);
}