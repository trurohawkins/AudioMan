#include "AudioEvent.h"

// audio thread side
AudioEventScheduler *scheduler = 0;
//queue to send audio massages to main thread
DECLARE_SPSC(AudioEventMessage, AudioEventMessageQueue, 256)
AudioEventMessageQueue audioEventQueue;
//array of rhythms that the events use
BitSet *rhythms = 0;
int rhythmMax = 0;
int rhythmCur = 0;

// main thread side
#define AEM_HEAPSIZE 64
Heap audioEventMessageHeap;
AudioCallback eventManifest[AUDIO_EVENT_MAX];
int curEvent = 0;

void initAudioEventScheduling(int rhythmsSize) {
	scheduler = calloc(1, sizeof(AudioEventScheduler));
	heapInit(&audioEventMessageHeap, sizeof(AudioEventMessage), AEM_HEAPSIZE, compareAudioEventMessages, 0);
	rhythms = calloc(max(1, rhythmsSize), sizeof(BitSet));
	rhythmMax = rhythmsSize;
	addRhythm(4, 0b1111);
}

int addRhythm(int length, uint64_t pattern) {
	if (rhythmCur < rhythmMax) {
		bitsetInit(rhythms, length);
		bitsetSetUInt64(&rhythms[rhythmCur], pattern);
		rhythmCur++;
	}
}

int scheduleEvent(void (*func)(void*), void *data, double frequency) {
	if (curEvent < AUDIO_EVENT_MAX) {
		eventManifest[curEvent].func = func;
		eventManifest[curEvent].data = data;
		addAudioCommand(1, curEvent, &frequency, sizeof(double));
		curEvent++;
	}
	return curEvent - 1;
}

bool addAudioEvent(int type, int obj, double frequency) {
	if (obj < 0 ||  obj > AUDIO_EVENT_MAX) {
		// we need to check if there are AUDIO_EVENT_MAX events happening, 
		//if not we want to reorder them downwards and adjust scheduler->eventNum
		// if there are, we return false
		return false;
	}
	if (scheduler->events[obj].type == 0) {
		AudioEvent ae;
		ae.type = type;
		// every event should have unique obj position, given by the manifest
		ae.obj = obj;
		ae.paused = false;
		ae.intervalFrames = (long long)((frequency / (aMan->bpm/60.0)) * aMan->sampleRate);
		ae.nextTriggerFrame = aMan->currentFrame + ae.intervalFrames;
		ae.rhythm = &rhythms[0];
		scheduler->events[obj] = ae;
		if (obj >= scheduler->eventNum) {
			scheduler->eventNum = obj+1;
		}
		return true;
	} else {
		//there already is an event here, somethign went wrong
		return false;
	}
}

//called on all events passed this event
void unscheduleEvents(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		for (int i = event; i < AUDIO_EVENT_MAX; i++) {
			eventManifest[i].func = 0;
			eventManifest[i].data = 0;
		}
		addAudioCommand(3, event, 0, 0);
	}
}

void removeAudioEvents(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		for (int i = event; i < AUDIO_EVENT_MAX; i++) {
			AudioEvent *ae = &scheduler->events[i];
			ae->type = 0;
			ae->obj = -1;
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
						if (!ae->paused && bitsetGet(ae->rhythm, ae->step)) {
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
										.data = ae->obj,
										.eventTime = timeInfo->outputBufferDacTime + (double)offset / aMan->sampleRate,
									};
									// push to signal to main thread to execute event
									AudioEventMessageQueue_aqPush(&audioEventQueue, aem); 
								}
							}
							// set the next trigger event time
							ae->nextTriggerFrame += ae->intervalFrames;
							ae->step = (ae->step + 1) % ae->rhythm->bitCount;
					}
				}
			}
		}
	}
}



bool spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd) {
	Sound *s = &sounds->bank[ae->obj];
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

void setPauseOnEvent(int event, bool state) {
	if (event >= 0 && event < scheduler->eventNum) {
		AudioEvent *ae = &scheduler->events[event];
		ae->paused = state;
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
	if (scheduler != NULL) {
		free(scheduler);
		scheduler = NULL;
	}
	if (rhythms != NULL) {
		free(rhythms);
		rhythms = NULL;
	}
}
