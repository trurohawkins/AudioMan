#include "AudioMan.h"
#include <signal.h>
#include <fcntl.h>

bool poo = false;
volatile bool running  = true;
float volume = 1.0;
int volEvent = -1;
uint64_t lastTime = 0;
uint64_t lastTime1 = 0;

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

uint64_t elapsed[100];
int count = 0;

void specialSound(void *sound) {
	uint64_t now = nowMS();
	printf("now %" PRIu64 " speical elapsed %" PRIu64 "\n", now, now - lastTime);
	//elapsed[count] = now - lastTime;
	//count++;
	lastTime = now;
	/*
	int *s0 = sound;
	playAudio(*s0);
	if (!poo) {
		pauseAudioEvent(volEvent);
		poo = true;
	} else {
		unpauseAudioEvent(volEvent);
		poo = false;
	}
	*/
}

void foopy(void *ound) {
	uint64_t now = nowMS();
	printf("foopy elpased: %" PRIu64 "\n", now - lastTime1);
	lastTime1 = now;
}


int main() {
	initAudio();
	signal(SIGINT, handler);
	int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
	fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

	//int sound0 = processAudioFile("sounds/a1.wav", false);
	//playAudio(sound0);
	//int sound1 = processAudioFile("sounds/a2.wav", false);

	double frequency = 1.0;
	//scheduleAudio(sound1, frequency);
	double f2 = 2.0;
	//volEvent = scheduleEvent(lowerVolume, &sound1, f2);
	int event = scheduleEvent(specialSound, 0, 2.0);
	event = scheduleEvent(foopy, 0, 4  * 0.3);
	lastTime = lastTime1 = nowMS();
	char buff[32];
	bool eventPaused = false;
	uint64_t pauseTime;
	while (running) {
		ssize_t r = read(STDIN_FILENO, buff, sizeof(buff));
		if (r >= 1) {
			eventPaused = !eventPaused;
			pauseAudioEvents(eventPaused);
			/*
			if (eventPaused) {
				pauseAudioEvent(event);
			} else {
				unpauseAudioEvent(event);
			}
			*/
			//printf("event: %i at %" PRIu64 "\n", eventPaused, nowMS());
		}
		parseAudioEvents();
	}
	endAudio();
	for (int i = 0; i < count; i++) {
		printf("elapsed %" PRIu64 "\n", elapsed[i]);

	}
	return 0;
}
