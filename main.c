#include "AudioMan.h"

bool poo = false;
float volume = 1.0;
int sound0;
int sound1;

void lowerVolume() {
	if (volume - 0.1 > 0) {
		volume -= 0.1;
	}
	setVolume(sound1, volume);
}

void specialSound() {
	playAudio(sound0);
}

int main() {
	initAudio();

	sound0 = processAudioFile("sounds/a1.wav", false);
	//playAudio(sound0);
	sound1 = processAudioFile("sounds/a2.wav", false);

	double frequency = 1.0;
	scheduleAudio(sound1, frequency);
	double f2 = 3.0;
	scheduleEvent(1, lowerVolume, f2);
	scheduleEvent(2, specialSound, 6.0);
	while (true) {
		parseAudioEvents();
	}
}
