#include "SDL3/SDL_main.h"
#include "SDL3/SDL.h"

#ifdef _WIN32
#define exit(code) exit(##code##)
#endif

int quit(int code);


SDL_Window* window;

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