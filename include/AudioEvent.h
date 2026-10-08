#pragma once
#include "AudioMan.h"

// audio thread side
typedef struct {
	int type;
	int obj; //reference to cllaback in eventManifest or sound in soundbank

	long long nextTriggerFrame;
	bool paused;
	long long intervalFrames;

	BitSet *rhythm;
	int step;
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

void initAudioEventScheduling(int rhythmsSize);
int scheduleEvent(void (*func)(void*), void *data, double frequency);
bool addAudioEvent(int type, int data, double frequency);
void unscheduleEvents(int start);
void removeAudioEvents(int start);
void setPauseOnEvent(int event, bool state);
void setPauseOnEvents(bool state, long long bufferStart);
void setRhythmCommand(int event, uint64_t pattern, size_t size);
void setRhythm(int event, uint64_t pattern, size_t size);
BitSet *addRhythm(uint64_t pattern, int length);

void checkScheduler(long long bufferStart, long long bufferEnd, const PaStreamCallbackTimeInfo *timeInfo);

void parseAudioEvents();
void flushAudioEvents();
int compareAudioEventMessages(const void *a, const void *b, void *context);
bool spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd);
void endAudioScheduling();
