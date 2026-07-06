#include "AudioMan.h"

bool poo = false;
float volume = 1.0;

void lowerVolume(void *sound) {
	int *s1 = sound;
	if (volume - 0.1 > 0) {
		volume -= 0.1;
	}
	setVolume(*s1, volume);
}

void specialSound(void *sound) {
	int *s0 = sound;
	playAudio(*s0);
}

int main() {
	initAudio();

	int sound0 = processAudioFile("sounds/a1.wav", false);
	//playAudio(sound0);
	int sound1 = processAudioFile("sounds/a2.wav", false);

	double frequency = 1.0;
	scheduleAudio(sound1, frequency);
	double f2 = 3.0;
	scheduleEvent(1, lowerVolume, &sound1, f2);
	scheduleEvent(2, specialSound, &sound0, 6.0);
	while (true) {
		parseAudioEvents();
	}
}
