// testing tools for openal

#include <stdio.h>
#include <math.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <imgui.h>
#include "imgui_impl_sdl.h"

//#define DResourcesRoot "./data/"
#define DResourcesRoot "/home/shared/src/xbx/testbed-openal/data/"

#define ERR(...)    fprintf(stderr, __VA_ARGS__)

static float FromDecibel(float dB)
{
	float Gain = exp10f(dB / 20.f);
	return Gain < 0.0001f ? 0 : Gain;
}

static float ToDecibel(float gain)
{
	if (gain <= .001f) // -60dB
		return -60.f;
	else
		return 20.f * log10f(gain);
}

// -------------------  LoadSound -------------------------
static ALuint LoadSound(const char* name)
{
	SDL_AudioSpec wav_spec;
	Uint32 wav_length;
	Uint8 *wav_buffer;
	if (SDL_LoadWAV(name, &wav_spec, &wav_buffer, &wav_length) == NULL) {
		ERR("LoadSound(%s): SDL_LoadWAV failed: %s\n", name, SDL_GetError());
		return 0;
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
		return 0;
	}

	ALuint buffer;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, wav_buffer, wav_length, wav_spec.freq);
	SDL_FreeWAV(wav_buffer);

	ALenum err = alGetError();
	if(err != AL_NO_ERROR)
	{
		ERR("LoadSound(%s): alBufferData Error: %s\n", name, alGetString(err));
		if(alIsBuffer(buffer))
			alDeleteBuffers(1, &buffer);
		return 0;
	}

	return buffer;
}

static void FreeSound(ALuint _Buf)
{
	if(alIsBuffer(_Buf))
		alDeleteBuffers(1, &_Buf);
}



// ------------------- Program resources -------------------------

struct SResources
{
	ALuint	albuf_mono;
	ALuint	albuf_stereo;
};

static bool LoadResources(SResources& _Res)
{
	_Res.albuf_mono = LoadSound(DResourcesRoot "mono.wav");
	if (_Res.albuf_mono == 0)
		return false;

	_Res.albuf_stereo = LoadSound(DResourcesRoot "stereo.wav");
	if (_Res.albuf_stereo == 0)
		return false;

	return true;
}

static void FreeResources(SResources& _Res)
{
	FreeSound(_Res.albuf_mono);		_Res.albuf_mono = 0;
	FreeSound(_Res.albuf_stereo);	_Res.albuf_stereo = 0;
}


// ------------------- OpenAl sources manager -------------------------
#define MGR_MAX_SOURCES 32
struct SMgrState {
	ALuint Avail[MGR_MAX_SOURCES];	int cAvail;
	ALuint Active[MGR_MAX_SOURCES];	int cActive;
};

static void Mgr_Init(SMgrState& _State)
{
	memset(&_State, 0, sizeof(_State));
	alGenSources(MGR_MAX_SOURCES, _State.Avail);
	_State.cAvail = MGR_MAX_SOURCES;
}

static void Mgr_Destroy(SMgrState& _State)
{
	if (_State.cActive > 0) {
		alSourceStopv(_State.cActive, _State.Active);
		memcpy(_State.Avail + _State.cAvail, _State.Active, _State.cActive*sizeof(ALuint));
		_State.cActive = 0;
	}
	alDeleteSources(_State.cAvail, _State.Avail);
}

static void Mgr_Update(SMgrState& _State)
{
	for (int i = 0; i<_State.cActive; i++) {
		ALuint s = _State.Active[i];
		ALenum state = AL_STOPPED;
		alGetSourcei(s, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING) {
			_State.Avail[_State.cAvail] = s;						_State.cAvail++;
			_State.Active[i] = _State.Active[_State.cActive-1];		_State.cActive --;
			i--;
		}
	}
}

static void Mgr_Play(SMgrState& _State, ALuint _Buf, float _dB)
{
	if (_State.cAvail == 0) {
		ERR("Too many sounds\n");
		return;
	}

	ALuint s = _State.Avail[_State.cAvail-1];	_State.cAvail--;
	_State.Active[_State.cActive] = s;			_State.cActive++;
	alSourcei(s, AL_BUFFER, _Buf);
	alSourcef(s, AL_GAIN, FromDecibel(_dB));
	alSourcePlay(s);
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

	// OpenAL: Open and initialize a device with default settings
	// and set current context, making the program ready to call OpenAL functions.
	ALCdevice*	alc_device = NULL;
	ALCcontext*	alc_ctx = NULL;
	const char*	alc_device_spec = NULL;
	const char*	alc_ext = NULL;
	const char*	al_vendor = NULL;
	const char*	al_renderer = NULL;
	const char*	al_version = NULL;
	const char*	al_ext = NULL;
	{
		alc_device = alcOpenDevice(NULL);
		if(!alc_device)
		{
			ERR("Could not open a device!\n");
			return 1;
		}

		alc_ctx = alcCreateContext(alc_device, NULL);
		if(alc_ctx == NULL || alcMakeContextCurrent(alc_ctx) == ALC_FALSE)
		{
			if(alc_ctx != NULL)
				alcDestroyContext(alc_ctx);
			alcCloseDevice(alc_device);
			ERR("Could not set a context!\n");
			return 1;
		}

		alc_device_spec = alcGetString(alc_device, ALC_DEVICE_SPECIFIER);
		alc_ext = alcGetString(alc_device, ALC_EXTENSIONS);
		//alcGetIntegerv(alc_device, ALC_MAJOR_VERSION, 1, &alc_major);
		//alcGetIntegerv(alc_device, ALC_MINOR_VERSION, 1, &alc_minor);

		al_vendor = alGetString(AL_VENDOR);
		al_renderer = alGetString(AL_RENDERER);
		al_version = alGetString(AL_VERSION);
		al_ext = alGetString(AL_EXTENSIONS);
	}

	// Load resource
	SResources Resources;
	{
		bool ok = LoadResources(Resources);
		if (!ok) {
			ERR("Could not load all program resource.\n");
			return 1;
		}
	}

	SMgrState MgrState;
	Mgr_Init(MgrState);

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
		Mgr_Update(MgrState);

		ImGui_ImplSdl_NewFrame(sdl_window);

		ImGui::SetNextWindowPos(ImVec2(10, 10));
		ImGui::SetNextWindowSize(ImVec2(500, 700));
		ImGui::Begin("main", NULL, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove);

		// top bar.
		{
			if (ImGui::Button("Quit"))
				break;

			ImGui::SameLine();

			static bool ShowImguiHelp = false;
			ImGui::Checkbox("show imgui help", &ShowImguiHelp);
			if (ShowImguiHelp)
				ImGui::ShowTestWindow();		// documentation shortcut
		}

		ImGui::Spacing();	// -----------------

		// Openal info:
		if (ImGui::CollapsingHeader("OpenAL info"))
		{
			ImGui::Columns(2, "OpenAL_info");
			ImGui::Text("device");			ImGui::NextColumn();	ImGui::TextWrapped(alc_device_spec);	ImGui::NextColumn();
			ImGui::Text("vendor");			ImGui::NextColumn();	ImGui::TextWrapped(al_vendor);			ImGui::NextColumn();
			ImGui::Text("renderer");		ImGui::NextColumn();	ImGui::TextWrapped(al_renderer);		ImGui::NextColumn();
			ImGui::Text("version");			ImGui::NextColumn();	ImGui::TextWrapped(al_version);			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Text("ALC extensions");	ImGui::NextColumn();	ImGui::TextWrapped(alc_ext);			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Text("AL extensions");	ImGui::NextColumn();	ImGui::TextWrapped(al_ext);				ImGui::NextColumn();
			ImGui::Columns(1);
		}

		ImGui::Spacing();	// -----------------

		// sound test
		if (ImGui::CollapsingHeader("Basic test", NULL, true, true))
		{
			static float mono_gaindB = -3.f;
			static float stereo_gaindB = -3.f;

			if (ImGui::Button("Play mono"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, mono_gaindB);
			}
			ImGui::SameLine();
			ImGui::SliderFloat("monogain", &mono_gaindB, -60, 6, "%.1fdB");

			if (ImGui::Button("Play stereo"))
			{
				Mgr_Play(MgrState, Resources.albuf_stereo, stereo_gaindB);
			}
			ImGui::SameLine();
			ImGui::SliderFloat("stereogain", &stereo_gaindB, -60, 6, "%.1fdB");
		}

		ImGui::Spacing();	// -----------------

		// Test stuff
		{
			static float f = 0.0f;
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", (float*)&clear_color);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}



		ImGui::End();

		// Rendering
		glViewport(0, 0, (int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y);
		glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();
		SDL_GL_SwapWindow(sdl_window);
	}

	Mgr_Destroy(MgrState);
	FreeResources(Resources);

	// OpenAL: cleanup
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(alc_ctx);
		alcCloseDevice(alc_device);
	}

	// Cleanup
	ImGui_ImplSdl_Shutdown();
	SDL_GL_DeleteContext(glcontext);
	SDL_DestroyWindow(sdl_window);
	SDL_Quit();

	return 0;
}
