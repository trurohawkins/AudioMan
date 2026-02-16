#include "AudioMan.h"

int main() {
	initAudio();

	int sound0 = processAudioFile("sounds/a1.wav", false);
	int sound1 = processAudioFile("sounds/a2.wav", false);

	scheduleAudio(sound0, 1.0);
	scheduleAudio(sound1, 3.0);
	while (true) {};
}
