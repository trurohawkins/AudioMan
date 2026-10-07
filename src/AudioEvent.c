#include "AudioEvent.h"

// audio thread side
DECLARE_SPSC(AudioEventMessage, AudioEventMessageQueue, 256)
AudioEventMessageQueue audioEventQueue;
AudioEventScheduler *scheduler = 0;

// main thread side
#define AEM_HEAPSIZE 64
Heap audioEventMessageHeap;
AudioCallback eventManifest[AUDIO_EVENT_MAX];

void initAudioEventScheduling() {
	scheduler = calloc(1, sizeof(AudioEventScheduler));
	heapInit(&audioEventMessageHeap, sizeof(AudioEventMessage), AEM_HEAPSIZE, compareAudioEventMessages, 0);
}

int scheduleEvent(void (*func)(void*), void *data, double frequency) {
	int event = -1;
	for (int i = 0; i < AUDIO_EVENT_MAX; i++) {
		if (eventManifest[i].func == 0) {
			event = i;
			break;
		}
	}
	if (event != -1) {
		eventManifest[event].func = func;
		eventManifest[event].data = data;
		addAudioCommand(1, event, frequency);
	}
	return event;
}

bool addAudioEvent(int type, int data, double frequency) {
	if (scheduler->eventNum >= AUDIO_EVENT_MAX) {
		// we need to check if there are AUDIO_EVENT_MAX events happening, 
		//if not we want to reorder them downwards and adjust scheduler->eventNum
		// if there are, we return false
	}
	int freeSpace = -1;
	for (int i = 0; i < AUDIO_EVENT_MAX; i++) {
		int event = (scheduler->eventNum + i) % AUDIO_EVENT_MAX;
		if (scheduler->events[event].type == 0) {
			freeSpace = event;
			break;
		}
	}
	if (freeSpace >= 0) {
		AudioEvent ae;
		ae.type = type;
		ae.data = data;
		ae.paused = false;
		ae.intervalFrames = (long long)((frequency / (aMan->bpm/60.0)) * aMan->sampleRate);
		ae.nextTriggerFrame = aMan->currentFrame + ae.intervalFrames;
		scheduler->events[freeSpace] = ae;
		if (scheduler->eventNum < AUDIO_EVENT_MAX-1) {
			scheduler->eventNum++;
		}
		return true;
	}
	return false;
}


void unscheduleEvent(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		// 2 indicates event rather than sound
		addAudioCommand(3, event, 2);
		eventManifest[event].func = 0;
		eventManifest[event].data = 0;
	}
}

void removeAudioEvent(int type, int data) {
	for (int i = 0; i < scheduler->eventNum; i++) {
		AudioEvent *ae = &scheduler->events[i];
		if (ae->type == type && ae->data == data) {
			ae->type = 0;
			ae->data = 0;
			if (i >= scheduler->eventNum - 1) {
				scheduler->eventNum--;
			}
		}
	}
}

void checkScheduler(long long bufferStart, long long bufferEnd, const PaStreamCallbackTimeInfo *timeInfo) {
	if (!scheduler->paused) {
		for (int i = 0; i < scheduler->eventNum; i++) {
			AudioEvent *ae = &scheduler->events[i];
			if (ae->type != 0) {
				// maybe remove 1st check so we can catch up if neede
				// currently we will drop it if its too far behind
				//printf("      event %i triggerFrame %lld\n", i, ae->nextTriggerFrame);
				if (ae->nextTriggerFrame >= bufferStart) {
					while (ae->nextTriggerFrame < bufferEnd) {
						if (!ae->paused) {
							long long triggerFrame = ae->nextTriggerFrame;
							//how far into audio buffer did the event occur?
							long long offset = triggerFrame - bufferStart;
							//printf("Audio event: frame=%lld offset=%lld\n", triggerFrame, offset);
								if (ae->type == 1) {
									if (!spawnVoice(ae, bufferStart, bufferEnd)) {
										break;
									}
								} else {
									AudioEventMessage aem = {
										.data = ae->data,
										.eventTime = timeInfo->outputBufferDacTime + (double)offset / aMan->sampleRate,
									};
									// push to signal to main thread to execute event
									AudioEventMessageQueue_aqPush(&audioEventQueue, aem); 
								}
							}
							// set the next trigger event time
							ae->nextTriggerFrame += ae->intervalFrames;
					}
				}
			}
		}
	}

}



bool spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd) {
	Sound *s = &sounds->bank[ae->data];
	int mixSpot = 0;
	Voice *vo = NULL;//findFreeMixSpot();
	for (;mixSpot < VOICE_MAX; mixSpot++) {
		if (aMan->mix[mixSpot].sound == NULL) {
			vo = &aMan->mix[mixSpot];
			break;
		}
	}
	if (vo) {
		vo->sound = s;
		vo->readFrames = 0;
		vo->bufferOffset = ae->nextTriggerFrame - bufferStart;
	} else {
		return false;
	}
	return true;
}

void setPauseOnEvent(int type, int data, bool state) {
	for (int i = 0; i < scheduler->eventNum; i++) {
		AudioEvent *ae = &scheduler->events[i];
		if (ae->type == type && ae->data == data) {
			ae->paused = state;
		}
	}
}

void setPauseOnEvents(bool state, long long bufferStart) {
	if (state) {
		if (!scheduler->paused) {
			scheduler->paused = true;
			scheduler->pauseFrame = bufferStart;
		}
	} else {
		if (scheduler->paused) {
			scheduler->paused = false;
			for (int i = 0; i < scheduler->eventNum; i++) {
				AudioEvent *ae = &scheduler->events[i];
				if (ae->type != 0) {// && ae->data != 0) {
					ae->nextTriggerFrame = bufferStart + (ae->nextTriggerFrame - scheduler->pauseFrame);
				}
			}
		}
	}
}

void executeEvent(AudioEventMessage *command) {
	if (command) {
		AudioCallback ac = eventManifest[command->data];
		void *data = ac.data;
		if (ac.func) {
			ac.func(data);
		}
	}
}

void parseAudioEvents() {
	AudioEventMessage command;
	while (AudioEventMessageQueue_aqPop(&audioEventQueue, &command)) {
		if (eventManifest[command.data].func != 0) {
			if (Pa_GetStreamTime(aMan->stream) >= command.eventTime) {
				executeEvent(&command);
			} else {
				//delay event
				heapPush(&audioEventMessageHeap, &command);
			}
		}
	}
	while (!heapIsEmpty(&audioEventMessageHeap)) {
		AudioEventMessage *check = heapPeek(&audioEventMessageHeap);
		if (check) {
			if (Pa_GetStreamTime(aMan->stream) >= check->eventTime) {
				executeEvent(check);
				heapPop(&audioEventMessageHeap, 0);
			} else {
				break;
			}
		}
	}
}

void flushAudioEvents() {
	AudioEventMessage toilet;
	while (AudioEventMessageQueue_aqPop(&audioEventQueue, &toilet)) {}
}

// used for the event message heap
int compareAudioEventMessages(const void *a, const void *b, void *context) {
	AudioEventMessage x = *(const AudioEventMessage *)a;
	AudioEventMessage y = *(const AudioEventMessage *)b;

	if (x.eventTime < y.eventTime) {
		return -1;
	} else if (x.eventTime > y.eventTime) {
		return 1;
	} else {
		return 0;
	}
}

void endAudioScheduling() {
	heapDestroy(&audioEventMessageHeap);
	free(scheduler);
	scheduler = NULL;
}
