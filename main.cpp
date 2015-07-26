// testing tools for openal

#include <stdio.h>
#include <SDL.h>
#include <SDL_opengl.h>

#include <AL/al.h>
#include <AL/alext.h>

#include <imgui.h>
#include "imgui_impl_sdl.h"

//#define DResourcesRoot "./data/"
#define DResourcesRoot "/home/shared/src/xbx/testbed-openal/data/"

#define ERR(...)    fprintf(stderr, __VA_ARGS__)


// -------------------  LoadSound -------------------------
struct SWavData {
	ALenum format;
	ALsizei size;
	ALsizei freq;
	const ALvoid *data;
};

bool LoadSound(const char* name, SWavData& _Wav)
{
	SDL_AudioSpec wav_spec;
	Uint32 wav_length;
	Uint8 *wav_buffer;
	if (SDL_LoadWAV(name, &wav_spec, &wav_buffer, &wav_length) == NULL) {
		ERR("LoadSound(%s): SDL_LoadWAV failed: %s\n", name, SDL_GetError());
		return false;
	}

	ALenum format = 0;
	if (wav_spec.channels == 1) {
		switch(wav_spec.format) {
		case AUDIO_U8:			format = AL_FORMAT_MONO8; break;
		case AUDIO_S16LSB:		format = AL_FORMAT_MONO16; break;
		case AUDIO_F32LSB:		format = AL_FORMAT_MONO_FLOAT32; break;
		}
	} else if (wav_spec.channels == 2) {
		switch(wav_spec.format) {
		case AUDIO_U8:			format = AL_FORMAT_STEREO8; break;
		case AUDIO_S16LSB:		format = AL_FORMAT_STEREO16; break;
		case AUDIO_F32LSB:		format = AL_FORMAT_STEREO_FLOAT32; break;
		}
	}

	if (format == 0) {
		// TODO: if needed, more channels / other formats.
		ERR("LoadSound(%s): Unsupported format (TODO): 0x%X\n", name, wav_spec.format);
		SDL_FreeWAV(wav_buffer);
		return false;
	}

	_Wav.data = wav_buffer;
	_Wav.size = wav_length;
	_Wav.freq = wav_spec.freq;
	_Wav.format = format;
	return true;
}

void FreeSound(SWavData& _Wav)
{
	if (_Wav.data)
		SDL_FreeWAV((Uint8*)_Wav.data);
	_Wav.data = NULL;
}



// ------------------- Program resources -------------------------

struct SResources
{
	SWavData wav_mono;
	SWavData wav_stereo;
};

bool LoadResources(SResources& _Res)
{
	bool ok;
	ok = LoadSound(DResourcesRoot "mono.wav", _Res.wav_mono);
	if (!ok)
		return false;

	ok = LoadSound(DResourcesRoot "stereo.wav", _Res.wav_stereo);
	if (!ok)
		return false;

	return true;
}

void FreeResources(SResources& _Res)
{
	FreeSound(_Res.wav_mono);
	FreeSound(_Res.wav_stereo);
}


// ------------------- Main -------------------------

int main(int, char**)
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
		return -1;

	// Setup window
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	SDL_Window *sdl_window = SDL_CreateWindow("testbed", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE);
	SDL_GLContext glcontext = SDL_GL_CreateContext(sdl_window);

	// Setup ImGui binding
	ImGui_ImplSdl_Init(sdl_window);

	// Load resource
	SResources Resources;
	bool ok = LoadResources(Resources);
	if (!ok) {
		ERR("Could not load all program resource.\n");
		return 1;
	}


	ImVec4 clear_color = ImColor(114, 144, 154);

	// Main loop
	bool done = false;
	while (!done)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSdl_ProcessEvent(&event);
			if (event.type == SDL_QUIT)
				done = true;
		}
		ImGui_ImplSdl_NewFrame(sdl_window);

		{
			ImGui::SetNextWindowPos(ImVec2(10, 10));
			ImGui::Begin("main", NULL, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize);

			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", (float*)&clear_color);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);

			ImGui::End();
		}

		// Rendering
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		SDL_GL_SwapWindow(sdl_window);
	}

	FreeResources(Resources);

	// Cleanup
	ImGui_ImplSdl_Shutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();

	return 0;
}
