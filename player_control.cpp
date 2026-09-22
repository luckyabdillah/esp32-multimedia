#include "player_control.h"
#include "globals.h"
#include "config.h"
#include "sd_manifest.h"
#include "gif_player.h"
#include "audio_player.h"

void playRandomPair() {
  String pick = pickRandomBasename(g_currentBasename);

  if (pick.length() == 0) {
    Serial.println("No available GIF+WAV pairs to play.");
    return;
  }

  Serial.printf(">> Playing pair of: %s.gif + %s.wav\n", pick.c_str(), pick.c_str());

  setGif(pick);
  sendAudioCommand(pick, AUTO_REPEAT_AUDIO);
}
