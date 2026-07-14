#include "AudioMan.h"
#include <signal.h>

bool poo = false;
volatile bool running  = true;
float volume = 1.0;
int volEvent = -1;

void lowerVolume(void *sound) {
	int *s1 = sound;
	if (volume - 0.1 > 0) {
		volume -= 0.1;
	} else {
		volume = 0;
	}
	setVolume(*s1, volume);
}

void handler (int sig) {
	running = false;
}

void specialSound(void *sound) {
	int *s0 = sound;
	playAudio(*s0);
	if (!poo) {
		pauseAudioEvent(volEvent);
		poo = true;
	} else {
		unpauseAudioEvent(volEvent);
		poo = false;
	}
}


int main() {
	signal(SIGINT, handler);
	initAudio();

	int sound0 = processAudioFile("sounds/a1.wav", false);
	//playAudio(sound0);
	int sound1 = processAudioFile("sounds/a2.wav", false);

	double frequency = 1.0;
	scheduleAudio(sound1, frequency);
	double f2 = 2.0;
	volEvent = scheduleEvent(lowerVolume, &sound1, f2);
	scheduleEvent(specialSound, &sound0, 10.0);
	while (running) {
		parseAudioEvents();
	}
	endAudio();
	return 0;
}
