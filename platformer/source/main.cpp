#include "types.h"

int init();
void HandleEvents();

int main(int argc, char** argv)
{
	bool running = true;
	init();
	while (running)
	{
		HandleEvents();
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
	return 0;
}

int quit(int code)
{
	SDL_Quit();
	exit(code);
}