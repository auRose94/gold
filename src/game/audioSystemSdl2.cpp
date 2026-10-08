// The SDL2 audio parity adapter. SDL2 has no SDL3-style audio streams;
// the equivalent is one opened output device plus a callback that mixes
// every playing sound itself (SDL_MixAudioFormat), which also brings the
// `loop` support the SDL3 backend lacks.

#include "game/audioSystem.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <vector>

namespace gold {

	namespace {

		struct sdl2Sound {
			SDL_AudioSpec spec;
			uint8_t* data = nullptr;
			uint32_t len = 0;
		};

		// One playing voice.
		struct sdl2Voice {
			sdl2Sound* sound = nullptr;
			uint32_t cursor = 0;  // byte offset into the source buffer
			bool loop = false;
			float volume = 1.0f;
		};

		class sdl2AudioSystem : public audioSystem {
			bool _inited = false;
			SDL_AudioDeviceID _device = 0;
			SDL_AudioSpec _devSpec = {};
			float _volume = 1.0f;
			uint64_t _nextHandle = 1;
			std::map<uint64_t, sdl2Sound> sounds;
			std::mutex playMutex;
			std::vector<sdl2Voice> voices;

			/** The audio callback: mix every voice into the device
			 *  buffer. Runs on the SDL audio thread. */
			static void mixCrate(void* userdata, uint8_t* stream,
				int len) {
				auto self = (sdl2AudioSystem*)userdata;
				SDL_memset(stream, 0, (size_t)len);
				std::lock_guard<std::mutex> guard(self->playMutex);
				auto it = self->voices.begin();
				while (it != self->voices.end()) {
					sdl2Voice& v = *it;
					const uint32_t remaining = v.sound->len - v.cursor;
					const uint32_t chunk =
						(uint32_t)SDL_min((int64_t)len, (int64_t)remaining);
					if (chunk == 0) {
						it = self->voices.erase(it);
						continue;
					}
					SDL_MixAudioFormat(stream,
						v.sound->data + v.cursor, AUDIO_F32SYS,
						(uint32_t)chunk,
						(int)(v.volume * self->_volume * 255.0f));
					v.cursor += chunk;
					// A voice past its end either loops or is dropped.
					if (v.cursor >= v.sound->len) {
						if (v.loop) v.cursor = 0;
						else {
							it = self->voices.erase(it);
							continue;
						}
					}
					++it;
				}
			}

			/** Convert a sound's WAV data to the device format (F32) so
			 *  the mixer sees a uniform sample type. SDL_AudioStreamCVT
			 *  converts in place. */
			bool toDeviceFormat(sdl2Sound& sound) {
				if (sound.spec.format == AUDIO_F32SYS) return true;
				SDL_AudioCVT cvt;
				const int planned = SDL_BuildAudioCVT(&cvt, sound.spec.format,
					sound.spec.channels, (int)sound.spec.freq,
					AUDIO_F32SYS, _devSpec.channels,
					(int)_devSpec.freq);
				// 0 means no conversion is needed; -1 is the failure.
				if (planned <= 0) return planned == 0;
				const size_t converted = (size_t)(sound.len * cvt.len_mult);
				uint8_t* buffer = (uint8_t*)SDL_malloc(converted);
				if (!buffer) return false;
				SDL_memcpy(buffer, sound.data, sound.len);
				SDL_free(sound.data);
				cvt.buf = buffer;
				cvt.len = (int)sound.len;
				if (SDL_ConvertAudio(&cvt) != 0) {
					SDL_free(buffer);
					return false;
				}
				sound.data = buffer;
				sound.len = (uint32_t)cvt.len_cvt;
				sound.spec.format = AUDIO_F32SYS;
				sound.spec.channels = _devSpec.channels;
				sound.spec.freq = _devSpec.freq;
				return true;
			}

		 public:
			~sdl2AudioSystem() override { close(); }

			bool open() override {
				if (_inited) return true;
				if (SDL_Init(SDL_INIT_AUDIO) != 0) {
					fprintf(stderr, "[SDL2 audio] %s\n", SDL_GetError());
					return false;
				}
				SDL_AudioSpec want;
				SDL_zero(want);
				want.freq = 48000;
				want.format = AUDIO_F32SYS;
				want.channels = 2;
				want.samples = 1024;
				want.callback = sdl2AudioSystem::mixCrate;
				want.userdata = this;
				SDL_AudioSpec have;
				_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have,
					SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
				if (_device == 0) {
					fprintf(stderr, "[SDL2 audio] %s\n", SDL_GetError());
					SDL_QuitSubSystem(SDL_INIT_AUDIO);
					return false;
				}
				_devSpec = have;
				SDL_PauseAudioDevice(_device, 0);
				_inited = true;
				return true;
			}

			void close() override {
				stop();
				for (auto& it : sounds) SDL_FreeWAV(it.second.data);
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
				sdl2Sound sound;
				if (!SDL_LoadWAV(path.c_str(), &sound.spec, &sound.data,
						&sound.len)) {
					fprintf(stderr, "[SDL2 audio] load %s: %s\n",
						path.c_str(), SDL_GetError());
					return 0;
				}
				if (!toDeviceFormat(sound)) {
					SDL_FreeWAV(sound.data);
					fprintf(stderr, "[SDL2 audio] convert %s: %s\n",
						path.c_str(), SDL_GetError());
					return 0;
				}
				const auto handle = _nextHandle++;
				sounds[handle] = sound;
				return handle;
			}

			void unloadSound(uint64_t handle) override {
				// The mixer thread mixes voices that point into `sounds`:
				// prune its voices under the same lock before freeing.
				std::lock_guard<std::mutex> guard(playMutex);
				auto it = sounds.find(handle);
				if (it == sounds.end()) return;
				voices.erase(std::remove_if(voices.begin(), voices.end(),
					[&it](const sdl2Voice& v) {
						return v.sound == &it->second;
					}),
					voices.end());
				SDL_FreeWAV(it->second.data);
				sounds.erase(it);
			}

			bool play(uint64_t handle, bool loop) override {
				std::lock_guard<std::mutex> guard(playMutex);
				auto it = sounds.find(handle);
				if (it == sounds.end()) return false;
				voices.push_back(
					sdl2Voice{&it->second, 0, loop, 1.0f});
				return true;
			}

			void stop() override {
				std::lock_guard<std::mutex> guard(playMutex);
				voices.clear();
			}

			void setVolume(float volume) override {
				_volume = volume > 1.0f ? 1.0f
										: (volume < 0.0f ? 0.0f : volume);
			}

			const char* name() const override { return "sdl2"; }
		};

	}  // namespace

}  // namespace gold

namespace {
	struct sdl2Registrar {
		sdl2Registrar() {
			auto audioFactory = []() -> gold::audioSystem* {
					return new gold::sdl2AudioSystem();
				};
				gold::registerAudioSystem("sdl2", audioFactory);
				gold::registerAudioSystem("sdl", audioFactory);
		}
	} sdl2Reg;
}  // namespace