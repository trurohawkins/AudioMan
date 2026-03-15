#include "AudioMan.h"

int main() {
	initAudio();

	int sound0 = processAudioFile("sounds/a1.wav", false);
	int sound1 = processAudioFile("sounds/a2.wav", false);
	
	double frequency = 1.0;
	scheduleAudio(sound1, frequency);
	scheduleEvent(0, frequency);
	//scheduleAudio(sound1, 3.0);
	bool playing = true;
	while (true) {
		/*
		char command = 0;
		scanf("%c", &command);
		if (command == ' ') {
			if (playing) {
				unScheduleAudio(sound1);
				playing = false;
			} else {
				scheduleAudio(sound1, 1.0);
				playing = true;
			}
		}
		*/
		int command;
		while (aqPop(&audioEventQueue, &command, sizeof(int))) {
			printf("popped: %d\n", command);
		}
	};
}
