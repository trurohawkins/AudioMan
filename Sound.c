#include "helper.h"
#include "Sound.h"

AudioManager *aMan = 0;
AtomicQueue audioQueue;
AudioEventScheduler *scheduler = 0;
bool streaming = false;

#include "Bank.c"


int initAudio() {
	aMan = calloc(1, sizeof(AudioManager));
	//aMan->sounds = makeList();
	aMan->volumes = calloc(1, sizeof(float));
	aMan->volumes[0] = 1;
	aMan->vGroups = 1;

	PaStreamParameters outputParameters;
	PaError err = Pa_Initialize();
	if (err != paNoError) {
		goto exit;
	}
	outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */

	if (outputParameters.device == paNoDevice) {
		fprintf(stderr, "Error: No default output device.\n");
		goto exit;
	}

	outputParameters.channelCount = 2;       /* stereo output */
	outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	double sampleRate = 44100.0;

	err = Pa_OpenStream(
			&aMan->stream,
			NULL, /* no input */
			&outputParameters,
			sampleRate,
			FPB,
			paClipOff,      /* we won't output out of range samples so don't bother clipping them */
			paLibsndfileCb,
			aMan);

	if(err != paNoError) {
		printf("open stream error  ");
		aMan->stream = 0;//do i need?
		goto exit;
	}

	err = Pa_StartStream(aMan->stream);
	if (err != paNoError) {
		goto exit;
	}
	const PaStreamInfo *info = Pa_GetStreamInfo(aMan->stream);
	aMan->sampleRate = info->sampleRate;
	aMan->bpm = 120.0;
	sounds = calloc(1, sizeof(SoundBank));
	scheduler = calloc(1, sizeof(AudioEventScheduler));
	return err;

exit:
	if (err != paNoError) { 
		printf("PortAudio error: %s\n", Pa_GetErrorText(err));
	}
	if (aMan != 0) {
		freeAudioManager();
	}
	Pa_Terminate();

}

Voice getVoice(Sound *s) {
	Voice v;
	v.sound = s;
	v.readFrames = 0;
	return v;
}

static int paLibsndfileCb(const void *inputBuffer, void *outputBuffer,
		unsigned long framesPerBuffer,
		const PaStreamCallbackTimeInfo* timeInfo,
		PaStreamCallbackFlags statusFlags,
		void *userData) {
	float *out = (float*)outputBuffer;
	memset(out, 0, framesPerBuffer * 2 * sizeof(float));
	AudioManager *a = userData;
	long long bufferStart = aMan->currentFrame;
	long long bufferEnd = bufferStart + framesPerBuffer;

	checkAudioCommands();
	for (int i = 0; i < EVENT_MAX; i++) {
		AudioEvent *ae = &scheduler->events[i];
		if (ae->type != 0) {
			// maybe remove 1st check so we can catch up if neede
				// currently we will drop it if its too far behind
			if (ae->nextTriggerFrame >= bufferStart) {
				if (ae->type == 1) {
					spawnVoice(ae, bufferStart, bufferEnd);
				}
			}
		}
	}
	/*
	for (int i = 0; i < sounds->soundNum; i++) {
		Sound *s = &sounds->bank[i];
		if (s) {
			if (s->scheduled) {
				spawnVoice(s, bufferStart, bufferEnd);
			}
		}
	}
	*/
	// mix of current songs
	for (int i = 0; i < VOICE_MAX; i++) {
		Voice *vo = &aMan->mix[i];
		Sound *s = vo->sound;
		if (!s) {
			continue;
		}
		// mixing
		long remaining = s->totalFrames - vo->readFrames;
		if (remaining <= 0) {
			if (!vo->sound->loop) {
				vo->sound = NULL;
				continue;
			} else {
				vo->readFrames = 0;
			}
		}
		long framesToMix = remaining < framesPerBuffer ? remaining : framesPerBuffer;
		long sampleOffset = vo->readFrames * 2;
		float volume = a->volumes[s->volume];

		long mixStart = 0;
		if (s->scheduled) {
			mixStart = vo->bufferOffset;
			vo->bufferOffset = 0;
		}

		for (long i = mixStart*2; i < framesToMix * 2; i++) {
			long buffIndex = sampleOffset + i - mixStart*2;
			if (buffIndex < s->totalFrames * 2) {
				out[i] += s->buff[buffIndex] * volume;
			}
		}
		vo->readFrames += framesToMix;
	}

	for (long i = 0; i < framesPerBuffer * 2; i++) {
		if (out[i] > 1.0f) {
			out[i] = 1.0f;
		} else if (out[i] < -1.0f) {
			out[i] = -1.0f;
		}
	}

	aMan->currentFrame += framesPerBuffer;

	return paContinue;
}

void checkAudioCommands() {
	AudioCommand ac;
	while (aqPop(&audioQueue, &ac, sizeof(AudioCommand))) {
		if (ac.cmd == 0) {
			for (int i = 0; i < VOICE_MAX; i++) {
				if (aMan->mix[i].sound == NULL) {
					Sound *s = &sounds->bank[ac.sound];
					if (ac.data != 0) {
						//s->scheduled = true;
						s->loop = false;
						AudioEvent ae;
						ae.type = 1;
						ae.data = ac.sound;
						ae.intervalFrames = (long long)((ac.data / (aMan->bpm/60.0)) * aMan->sampleRate);
						ae.nextTriggerFrame = aMan->currentFrame + ae.intervalFrames;
						addAudioEvent(ae);
					} else {
						aMan->mix[i].sound = s;
						aMan->mix[i].readFrames = 0;
					}
					break;
				}
			}
		} else if (ac.cmd == 1) {
			Sound *s = &sounds->bank[ac.sound];
			for (int i = 0; i < VOICE_MAX; i++) {
				if (aMan->mix[i].sound != NULL) {
					if (strcmp(s->file, aMan->mix[i].sound->file) == 0) {
						aMan->mix[i].sound = NULL;
					}
				}
			}
		} else if (ac.cmd == 2) {
			removeAudioEvent(ac.data, ac.sound);
		}
	}
}

void addAudioEvent(AudioEvent ae) {
	int freeSpace = -1;
	for (int i = 0; i < EVENT_MAX; i++) {
		if (scheduler->events[i].type == 0) {
			freeSpace = i;
			break;
		}
	}
	if (freeSpace >= 0) {
		scheduler->events[freeSpace] = ae;
	}
}

void removeAudioEvent(int type, int data) {
	for (int i = 0; i < EVENT_MAX; i++) {
		AudioEvent *ae = &scheduler->events[i];
		if (ae->type == type && ae->data == data) {
			ae->type = 0;
			ae->data = 0;
		}
	}
}

void spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd) {
	Sound *s = &sounds->bank[ae->data];
	int mixSpot = 0;
	while (ae->nextTriggerFrame < bufferEnd) {
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
			ae->nextTriggerFrame += ae->intervalFrames;
		} else {
			break;
		}
	}
}

Voice *findFreeMixSpot() {
	for (int i = 0; i < VOICE_MAX; i++) {
		if (aMan->mix[i].sound == NULL) {
			return &aMan->mix[i];
		}
	}
}


void changeVolume(int group, float vol) {
	if (group < aMan->vGroups) {
		aMan->volumes[group] = vol;
	}
}

int addVolGroup() {
	if (aMan) {
		int cur = aMan->vGroups;
		aMan->vGroups++;
		float *tmp = calloc(aMan->vGroups, sizeof(float));
		for (int i = 0; i < aMan->vGroups - 1; i++) {
			tmp[i] = aMan->volumes[i];
		}
		tmp[aMan->vGroups-1] = 1;
		free(aMan->volumes);
		aMan->volumes = tmp;
		return cur;
	} else {
		return -1;
	}
}
void changeVolGroup(Sound *s, int group) {
	s->volume = group;
}

void endAudio() {
	freeAudioManager();
	Pa_Terminate();
}

void freeAudioManager() {
	if (aMan) {
		if (aMan->stream) {
			Pa_StopStream(aMan->stream);
			Pa_CloseStream(aMan->stream);
		}
		free(aMan->volumes);
		//freeListSaveObj(&aMan->mix);
		//deleteList(&aMan->sounds, freeSound);
		//free(aMan->stream);
		free(aMan);
		freeSoundBank();
		//free events as well
		free(scheduler);
	}
}
