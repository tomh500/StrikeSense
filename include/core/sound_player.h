#pragma once

#include <string>

namespace sound {

bool Init();
void Quit();
void PreloadSounds();
void Play(int id, float volume);
void StopMusicKitPlayback();

} // namespace sound
