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
static const float PI = 3.14159f;

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
	ALuint  albuf_monoloop;
	ALuint  albuf_stereoloop;
};

static bool LoadResources(SResources& _Res)
{
	_Res.albuf_mono = LoadSound(DResourcesRoot "drip.wav");
	if (_Res.albuf_mono == 0)
		return false;

	_Res.albuf_stereo = LoadSound(DResourcesRoot "bark.wav");
	if (_Res.albuf_stereo == 0)
		return false;

	_Res.albuf_monoloop = LoadSound(DResourcesRoot "mosquitoloop.wav");
	if (_Res.albuf_monoloop == 0)
		return false;

	_Res.albuf_stereoloop = LoadSound(DResourcesRoot "rainloop.wav");
	if (_Res.albuf_stereoloop == 0)
		return false;

	return true;
}

static void FreeResources(SResources& _Res)
{
	FreeSound(_Res.albuf_mono);			_Res.albuf_mono = 0;
	FreeSound(_Res.albuf_stereo);		_Res.albuf_stereo = 0;
	FreeSound(_Res.albuf_monoloop);		_Res.albuf_monoloop = 0;
	FreeSound(_Res.albuf_stereoloop);	_Res.albuf_stereoloop = 0;
}


// ------------------- OpenAl sources manager -------------------------
#define MGR_MAX_SOURCES 32
#define MGR_MAX_EMITTERS 8
struct SEmitter {
	ALuint Source;
	bool  active;
	float dB;

	// spatial
	float radius;
	float pos[3];
	float vel[3];
};

struct SMgrState {
	ALuint		Avail[MGR_MAX_SOURCES];		int cAvail;
	ALuint		Active[MGR_MAX_SOURCES];	int cActive;

	SEmitter	Emitters[MGR_MAX_EMITTERS];
};

static void Mgr_Init(SMgrState& _State)
{
	memset(&_State, 0, sizeof(_State));
	alGenSources(MGR_MAX_SOURCES, _State.Avail);
	_State.cAvail = MGR_MAX_SOURCES;

	for (int i=0; i < MGR_MAX_EMITTERS; i++) {
		ALuint s = _State.Avail[_State.cAvail-1];	_State.cAvail--;
		_State.Emitters[i].Source = s;
	}
}

static void Mgr_Destroy(SMgrState& _State)
{
	if (_State.cActive > 0) {
		alSourceStopv(_State.cActive, _State.Active);
		memcpy(_State.Avail + _State.cAvail, _State.Active, _State.cActive*sizeof(ALuint));
		_State.cAvail += _State.cActive;
		_State.cActive = 0;
	}
	for (int i=0; i < MGR_MAX_EMITTERS; i++) {
		ALuint s = _State.Emitters[i].Source;
		alSourceStop(s);
		_State.Emitters[i].Source = 0;
		_State.Avail[_State.cAvail] = s; _State.cAvail ++;
	}
	alDeleteSources(_State.cAvail, _State.Avail);
}

static int Mgr_Update(SMgrState& _State)
{
	int cActive = 0;

	for (int i = 0; i<_State.cActive; i++) {
		ALuint s = _State.Active[i];
		ALenum state = AL_STOPPED;
		alGetSourcei(s, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING) {
			_State.Avail[_State.cAvail] = s;						_State.cAvail++;
			_State.Active[i] = _State.Active[_State.cActive-1];		_State.cActive --;
			i--;
		} else {
			cActive ++;
		}
	}

	for (int i=0; i < MGR_MAX_EMITTERS; i++) {
		const SEmitter& E = _State.Emitters[i];
		ALuint s = E.Source;
		alSourcef(s, AL_GAIN, FromDecibel(E.dB));
		alSourcef(s, AL_SOURCE_RADIUS, E.radius);
		alSource3f(s, AL_POSITION, E.pos[0], E.pos[1], E.pos[2]);
		alSource3f(s, AL_VELOCITY, E.vel[0], E.vel[1], E.vel[2]);

		ALenum state = AL_STOPPED;
		alGetSourcei(s, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING)
			cActive ++;
		if (E.active && state != AL_PLAYING)
			alSourcePlay(s);
		else if (!E.active && state != AL_STOPPED)
			alSourceStop(s);
	}

	return cActive;
}

static void Mgr_Play(SMgrState& _State, ALuint _Buf, float _dB, bool _Direct=false)
{
	if (_State.cAvail == 0) {
		ERR("Too many sounds\n");
		return;
	}

	ALuint s = _State.Avail[_State.cAvail-1];	_State.cAvail--;
	_State.Active[_State.cActive] = s;			_State.cActive++;

	alSourcei(s, AL_BUFFER, _Buf);
	alSourcef(s, AL_GAIN, FromDecibel(_dB));
	alSourcei(s, AL_LOOPING, AL_FALSE);
	alSourcei(s, AL_DIRECT_CHANNELS_SOFT, _Direct?AL_TRUE:AL_FALSE);
	alSourcef(s, AL_SOURCE_RADIUS, 0);
	alSource3f(s, AL_POSITION, 0, 0, 0);
	alSource3f(s, AL_VELOCITY, 0, 0, 0);

	alSourcePlay(s);
}
static void Mgr_Play(SMgrState& _State, ALuint _Buf, float _dB, const float _Pos[3], float _Radius=0)
{
	if (_State.cAvail == 0) {
		ERR("Too many sounds\n");
		return;
	}

	ALuint s = _State.Avail[_State.cAvail-1];	_State.cAvail--;
	_State.Active[_State.cActive] = s;			_State.cActive++;

	alSourcei(s, AL_BUFFER, _Buf);
	alSourcef(s, AL_GAIN, FromDecibel(_dB));
	alSourcei(s, AL_LOOPING, AL_FALSE);
	alSourcei(s, AL_DIRECT_CHANNELS_SOFT, AL_FALSE);
	alSourcef(s, AL_SOURCE_RADIUS, _Radius);
	alSource3f(s, AL_POSITION, _Pos[0], _Pos[1], _Pos[2]);
	alSource3f(s, AL_VELOCITY, 0, 0, 0);

	alSourcePlay(s);
}


// ------------------- imgui helper -------------------------
static float max(float a, float b) { return a>b?a:b; }
static void ImGuiPointOnMap(const char* id, float*x, float *y, float radius, float ref_size, float center_circle_radius)
{
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();            // ImDrawList API uses screen coordinates!
	float canvas_radius = 100; //0.5f*max(50.0f,ImGui::GetWindowContentRegionMax().x-ImGui::GetCursorPos().x);
	ImVec2 canvas_size = ImVec2(2*canvas_radius, 2*canvas_radius);
	draw_list->AddRectFilledMultiColor(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), ImColor(0,0,0), ImColor(0,0,0), ImColor(5,5,10), ImColor(5,5,10));
	draw_list->AddRect(canvas_pos, ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y), ImColor(255,255,255));
	draw_list->AddCircle(ImVec2(canvas_pos.x + canvas_size.x/2, canvas_pos.y + canvas_size.y/2), canvas_radius*(center_circle_radius/ref_size), ImColor(128,128,0));
	ImGui::InvisibleButton(id, canvas_size);	// (for space allocation and clicks)
	if (ImGui::IsItemHovered())
	{
		ImVec2 mouse_pos_in_canvas = ImVec2(ImGui::GetIO().MousePos.x - canvas_pos.x, ImGui::GetIO().MousePos.y - canvas_pos.y);
		if (ImGui::GetIO().MouseDown[0] && ImGui::GetIO().MouseDownOwned[0])
		{
			(*x) = ref_size * (mouse_pos_in_canvas.x-(canvas_size.x/2))/canvas_radius;
			(*y) = ref_size * (mouse_pos_in_canvas.y-(canvas_size.y/2))/canvas_radius;
		}
	}
	draw_list->PushClipRect(ImVec4(canvas_pos.x, canvas_pos.y, canvas_pos.x+canvas_size.x, canvas_pos.y+canvas_size.y));      // clip lines within the canvas (if we resize it, etc.)
	draw_list->AddCircleFilled(ImVec2(canvas_pos.x + canvas_size.x/2 + (*x)*canvas_radius/ref_size, canvas_pos.y + canvas_size.y/2 + (*y)*canvas_radius/ref_size), max(2,radius*canvas_radius/ref_size), 0x88FFFFFF);
	draw_list->PopClipRect();
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

	// openal sources
	SMgrState MgrState;
	SEmitter* SpatialEmit;
	SEmitter* AmbiantLoop;
	{
		Mgr_Init(MgrState);

		SpatialEmit = &MgrState.Emitters[0];
		AmbiantLoop = &MgrState.Emitters[1];

		alSourcei(SpatialEmit->Source, AL_BUFFER, Resources.albuf_monoloop);
		alSourcei(SpatialEmit->Source, AL_LOOPING, AL_TRUE);
		alSourcei(SpatialEmit->Source, AL_DIRECT_CHANNELS_SOFT, AL_FALSE);
		SpatialEmit->active = false;
		SpatialEmit->dB = 0.f;
		SpatialEmit->pos[0] = .5f;
		SpatialEmit->pos[1] = .75f;
		SpatialEmit->pos[2] = -3;
		SpatialEmit->radius = 0.01f;

		alSourcei(AmbiantLoop->Source, AL_BUFFER, Resources.albuf_stereoloop);
		alSourcei(AmbiantLoop->Source, AL_LOOPING, AL_TRUE);
		alSourcei(AmbiantLoop->Source, AL_DIRECT_CHANNELS_SOFT, AL_TRUE);
		AmbiantLoop->active = false;
		AmbiantLoop->dB = -9.f;
	}

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
		uint CurTimeMs = SDL_GetTicks();
		int ActiveSources = Mgr_Update(MgrState);

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

		// basic test
		if (ImGui::CollapsingHeader("Basic", NULL, true, true))
		{
			static float mono_gaindB = -3.f;
			static float stereo_gaindB = -3.f;

			if (ImGui::Button("Play mono"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, mono_gaindB);
			}
			ImGui::SameLine();
			ImGui::SliderFloat("##vol1", &mono_gaindB, -60, 6, "%.1fdB");

			if (ImGui::Button("Play stereo"))
			{
				Mgr_Play(MgrState, Resources.albuf_stereo, stereo_gaindB);
			}
			ImGui::SameLine();
			ImGui::SliderFloat("##vol2", &stereo_gaindB, -60, 6, "%.1fdB");
		}

		ImGui::Spacing();	// -----------------

		// Direct
		if (ImGui::CollapsingHeader("Direct", NULL, true, true))
		{
			ImGui::Checkbox("Ambiance", &AmbiantLoop->active);
			ImGui::SameLine();
			ImGui::SliderFloat("##vol0", &AmbiantLoop->dB, -60, 6, "%.1fdB");
		}

		ImGui::Spacing();	// -----------------

		// Spatialized
		if (ImGui::CollapsingHeader("Spatialized", NULL, true, true))
		{
			ImGui::Checkbox("Mosquito", &SpatialEmit->active);
			ImGui::SameLine();
			ImGui::SliderFloat("##vol4", &SpatialEmit->dB, -60, 6, "%.1fdB");
			ImGui::SliderFloat("radius", &SpatialEmit->radius, 0, 5);

			static uint PrevTime = 0;
			static float PrevPos[3];
			static bool automove = false;
			ImGui::Checkbox("Auto move", &automove);
			if (automove) {
				struct SLocal {
					static void anim(float t, float v[3]) {
						float w = (t*2*PI);
						v[0] = 5*sinf(w*2+1) + 3*sinf(w*6+2) + 2*sinf(w*6+3) + 1*sinf(w*9+4);
						v[1] = 5*sinf(w*3+5) + 3*sinf(w*4+6) + 2*sinf(w*7+7) + 1*sinf(w*8+8);
						v[2] = 5*sinf(w*4+9) + 3*sinf(w*5+1) + 2*sinf(w*8+2) + 1*sinf(w*7+3);
					}
				};

				float t = (CurTimeMs % 60000) / 60000.f;
				SLocal::anim(t, SpatialEmit->pos);
			}

			ImGui::InputFloat3("pos", SpatialEmit->pos);
			ImGui::InputFloat3("vel", SpatialEmit->vel);
			ImGuiPointOnMap("top", &SpatialEmit->pos[0], &SpatialEmit->pos[2], SpatialEmit->radius, 10, 0.25f);
			ImGui::SameLine();
			ImGuiPointOnMap("front", &SpatialEmit->pos[0], &SpatialEmit->pos[1], SpatialEmit->radius, 10, 0.25f);

			if (PrevTime != 0 && CurTimeMs > PrevTime) {
				float dt = 0.001f*(CurTimeMs-PrevTime);
				float k = 1.f;
				SpatialEmit->vel[0] = k * ((SpatialEmit->pos[0] - PrevPos[0]) / dt);
				SpatialEmit->vel[1] = k * ((SpatialEmit->pos[1] - PrevPos[1]) / dt);
				SpatialEmit->vel[2] = k * ((SpatialEmit->pos[2] - PrevPos[2]) / dt);
			}
			PrevTime = CurTimeMs;
			memcpy(PrevPos, SpatialEmit->pos, sizeof(PrevPos));
		}

		ImGui::Spacing();	// -----------------

		// basic test
		if (ImGui::CollapsingHeader("Tests", NULL, true, true))
		{
			static const float Front[3] = {0,0,-1};
			if (ImGui::Button("stereo base"))
			{
				Mgr_Play(MgrState, Resources.albuf_stereo, -3, false);
			}
			ImGui::SameLine();
			if (ImGui::Button("stereo direct"))
			{
				Mgr_Play(MgrState, Resources.albuf_stereo, -3, true);
			}
			if (ImGui::Button("mono base"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, -3, false);
			}
			ImGui::SameLine();
			if (ImGui::Button("mono direct"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, -3, true);
			}
			ImGui::SameLine();
			if (ImGui::Button("mono 3d narrow"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, -3, Front, 0.01f);
			}
			ImGui::SameLine();
			if (ImGui::Button("mono 3d wide"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, -3, Front, 1.f);
			}
			ImGui::SameLine();
			if (ImGui::Button("mono 3d omni"))
			{
				Mgr_Play(MgrState, Resources.albuf_mono, -3, Front, 10.f);
			}
		}

		ImGui::Spacing();	// -----------------

		// status
		{
			ImGui::Separator();
			ImGui::Text("Active Sources: %d / %d\n", ActiveSources, MGR_MAX_SOURCES);
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

		ImGui::End();

		// Rendering
		ImVec4 clear_color = ImColor(114, 144, 154);
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
