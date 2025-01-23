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

GLObject* testObj;
Shader* testShader;
File* testFile;

int main(int argc, char** argv)
{
	bool running = true;
	init();

	testObj = new GLObject(plane, sizeof(plane), planeIndices, sizeof(planeIndices));
	testShader = new Shader(Path("error.vert").c_str(), Path("error.frag").c_str());
	//testFile = new File(Path("error.vert").c_str());

	while (running)
	{
		HandleEvents();

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
	window = SDL_CreateWindow("Platformer", 1920, 1080, SDL_WINDOW_FULLSCREEN | SDL_WINDOW_OPENGL);
	SDL_GLContext glContext = SDL_GL_CreateContext(window); //Create GL context
	if (!glContext) //if failed to create context
		quit(-1); //close
	GLenum glew = glewInit(); //init glew
	if (glew != GLEW_OK) //if glew didn't work
		quit(-1); //close

	glClearColor(0.529f, 0.808f, 0.922f, 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	SDL_GL_SwapWindow(window);
	return 0;
}

int quit(int code)
{
	SDL_Quit();
	exit(code);
}

void Draw()
{
	glClear(GL_COLOR_BUFFER_BIT); //clear frame buffer

	glUseProgram(testShader->GetProgram());
	testObj->Draw();
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		std::cout << std::hex << err << "\n";
		std::cout << std::dec << std::endl;
	}

	SDL_GL_SwapWindow(window);
}