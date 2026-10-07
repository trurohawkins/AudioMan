#pragma once
#include "AudioMan.h"
// audio thread side
typedef struct {
	int type;
	int data;

	long long nextTriggerFrame;
	bool paused;
	long long pauseFrame;
	long long intervalFrames;
} AudioEvent;

#define AUDIO_EVENT_MAX 256
typedef struct {
	AudioEvent events[AUDIO_EVENT_MAX];
	int eventNum;
	bool paused;
	long long pauseFrame;
} AudioEventScheduler;

//main thread side
typedef struct {
	void (*func)(void*);
	void *data;
} AudioCallback;

typedef struct {
	int data;
	double eventTime;
} AudioEventMessage;

void initAudioEventScheduling();
int scheduleEvent(void (*func)(void*), void *data, double frequency);
bool addAudioEvent(int type, int data, double frequency);
void unscheduleEvent(int event);
void removeAudioEvent(int type, int data);
void setPauseOnEvent(int type, int data, bool state);
void setPauseOnEvents(bool state, long long bufferStart);

void checkScheduler(long long bufferStart, long long bufferEnd, const PaStreamCallbackTimeInfo *timeInfo);

void parseAudioEvents();
void flushAudioEvents();
int compareAudioEventMessages(const void *a, const void *b, void *context);
bool spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd);
void endAudioScheduling();
