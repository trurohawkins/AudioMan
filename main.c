#include "AudioMan.h"

bool poo = false;
float volume = 1.0;
int sound0;
int sound1;

void doAudioCommands(int command) {
	printf("\n\npopped: %d\n\n", command);
	if (command == 2) {
		if (volume - 0.1 > 0) {
			volume -= 0.1;
		}
		setVolume(sound1, volume);
	} else if (command == 1) {
		playAudio(sound0);
	}
}

int main() {
	initAudio();
	setAudioCommands(&doAudioCommands);

	sound0 = processAudioFile("sounds/a1.wav", false);
	//playAudio(sound0);
	sound1 = processAudioFile("sounds/a2.wav", false);

	double frequency = 1.0;
	scheduleAudio(sound1, frequency);
	scheduleEvent(0, frequency);
	double f2 = 3.0;
	scheduleEvent(1, f2);
	//scheduleEvent(2, 6.0);
	while (true) {
		parseAudioEvents();
	}
}
