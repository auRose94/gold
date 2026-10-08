#include "game/audioSystem.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

#include <map>
#include <mutex>
#include <vector>

namespace gold {

	namespace {

		struct sdlSound {
			SDL_AudioSpec spec;
			Uint8* data = nullptr;
			Uint32 len = 0;
		};

		// SDL3 audio backend. Plays WAVs through a single opened audio
		// device. Each play() binds a fresh audio stream (SDL3's mixing
		// model) so sounds can overlap, with the master volume applied per
		// stream.
		class sdlAudioSystem : public audioSystem {
			bool _inited = false;
			SDL_AudioDeviceID _device = 0;
			SDL_AudioSpec _devSpec = {};
			float _volume = 1.0f;
			uint64_t _nextHandle = 1;
			std::map<uint64_t, sdlSound> sounds;
			std::mutex playMutex;
			std::vector<SDL_AudioStream*> playing;

			void cleanupFinishedStreams() {
				// Drop streams whose queued data has fully played out.
				auto it = playing.begin();
				while (it != playing.end()) {
					SDL_AudioStream* s = *it;
					if (SDL_GetAudioStreamAvailable(s) == 0) {
						SDL_UnbindAudioStreams(&s, 1);
						SDL_DestroyAudioStream(s);
						it = playing.erase(it);
					} else {
						++it;
					}
				}
			}

		 public:
			~sdlAudioSystem() override { close(); }

			bool open() override {
				if (_inited) return true;
				if (!SDL_Init(SDL_INIT_AUDIO)) {
					fprintf(stderr, "[SDL3 audio] %s\n", SDL_GetError());
					return false;
				}
				// nullptr spec selects a sensible default (F32, stereo).
				_device = SDL_OpenAudioDevice(
					SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
				if (_device == 0) {
					fprintf(stderr, "[SDL3 audio] %s\n", SDL_GetError());
					SDL_QuitSubSystem(SDL_INIT_AUDIO);
					return false;
				}
				SDL_GetAudioDeviceFormat(_device, &_devSpec, nullptr);
				_inited = true;
				return true;
			}

			void close() override {
				stop();
				for (auto& it : sounds) SDL_free(it.second.data);
				sounds.clear();
				if (_device != 0) {
					SDL_CloseAudioDevice(_device);
					_device = 0;
				}
				if (_inited) {
					SDL_QuitSubSystem(SDL_INIT_AUDIO);
					_inited = false;
				}
			}

			uint64_t loadSound(const std::string& path) override {
				if (_device == 0) return 0;
				sdlSound sound;
				if (!SDL_LoadWAV(path.c_str(), &sound.spec, &sound.data,
						&sound.len)) {
					fprintf(stderr, "[SDL3 audio] load %s: %s\n",
						path.c_str(), SDL_GetError());
					return 0;
				}
				auto handle = _nextHandle++;
				sounds[handle] = sound;
				return handle;
			}

			void unloadSound(uint64_t handle) override {
				auto it = sounds.find(handle);
				if (it == sounds.end()) return;
				SDL_free(it->second.data);
				sounds.erase(it);
			}

			bool play(uint64_t handle, bool loop) override {
				(void)loop;  // SDL3 streams do not self-loop
				std::lock_guard<std::mutex> guard(playMutex);
				auto it = sounds.find(handle);
				if (it == sounds.end()) return false;
				cleanupFinishedStreams();
				SDL_AudioStream* stream =
					SDL_CreateAudioStream(&it->second.spec, &_devSpec);
				if (!stream) return false;
				SDL_SetAudioStreamGain(stream, _volume);
				SDL_PutAudioStreamData(
					stream, it->second.data, it->second.len);
				SDL_FlushAudioStream(stream);
				SDL_BindAudioStreams(_device, &stream, 1);
				playing.push_back(stream);
				return true;
			}

			void stop() override {
				std::lock_guard<std::mutex> guard(playMutex);
				for (auto* s : playing) {
					SDL_UnbindAudioStreams(&s, 1);
					SDL_DestroyAudioStream(s);
				}
				playing.clear();
			}

			void setVolume(float volume) override {
				_volume = volume > 1.0f ? 1.0f : (volume < 0.0f ? 0.0f : volume);
				std::lock_guard<std::mutex> guard(playMutex);
				for (auto* s : playing)
					SDL_SetAudioStreamGain(s, _volume);
			}

			const char* name() const override { return "sdl"; }
		};

		struct sdlRegistrar {
			sdlRegistrar() {
				// This TU ships as the SDL3 plugin (libgoldSdl3): register
				// both the concrete name for direct selection and the
				// generic "sdl" alias.
				auto factory = []() -> audioSystem* {
					return new sdlAudioSystem();
				};
				registerAudioSystem("sdl", factory);
				registerAudioSystem("sdl3", factory);
			}
		};
		sdlRegistrar sdlReg;

	}  // namespace

}  // namespace gold