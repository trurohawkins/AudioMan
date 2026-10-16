#include "helper.h"
#include "Sound.h"

AudioManager *aMan = 0;
DECLARE_SPSC(AudioCommand, AudioCommandQueue, 256)

AudioCommandQueue audioQueue;

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
		//fprintf(stderr, "Error: No default output device.\n");
		goto exit;
	}

	outputParameters.channelCount = 2;       /* stereo output */
	outputParameters.sampleFormat = paFloat32; /* 32 bit floating point output */
	outputParameters.suggestedLatency = Pa_GetDeviceInfo(outputParameters.device)->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	double sampleRate = 44100.0;
	
	sounds = calloc(1, sizeof(SoundBank));

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
		//printf("open stream error  ");
		aMan->stream = 0;//do i need?
		goto exit;
	}

	err = Pa_StartStream(aMan->stream);
	if (err != paNoError) {
		//printf("Start stream failed: %s\n", Pa_GetErrorText(err));
		goto exit;
	}
	const PaStreamInfo *info = Pa_GetStreamInfo(aMan->stream);
	aMan->sampleRate = info->sampleRate;
	aMan->bpm = 120.0;
	initAudioEventScheduling(16);
	return err;

exit:
	if (err != paNoError) { 
		//printf("PortAudio error: %s\n", Pa_GetErrorText(err));
	}
	if (aMan != 0) {
		freeAudioManager();
	}
	Pa_Terminate();
	return err;

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
	long long bufferStart = a->currentFrame;
	long long bufferEnd = bufferStart + framesPerBuffer;
	checkAudioCommands(a->currentFrame);
	//printf("    bufferStart %lld bufferEnd %lld\n", bufferStart, bufferEnd);
	checkScheduler(bufferStart, bufferEnd, timeInfo);
	// mix of current songs
	for (int i = 0; i < VOICE_MAX; i++) {
		Voice *vo = &a->mix[i];
		Sound *s = vo->sound;
		if (!s) {
			continue;
		}
		/*
		char msg[160];

		snprintf(msg, sizeof(msg),
						 "sound=%p buff=%p totalFrames=%ld readFrames=%ld\n",
						 (void *)s,
						 (void *)s->buff,
						 s->totalFrames,
						 vo->readFrames);

		debugWrite(msg);
		*/
		// mixing
		long startFrame = vo->bufferOffset;
		long framesAvailable = framesPerBuffer - startFrame;

		long remaining = s->totalFrames - vo->readFrames;
		if (remaining <= 0) {
			if (!s->loop) {
				vo->sound = NULL;
				continue;
			} else {
				vo->readFrames = 0;
			}
		}
		long framesToMix = remaining < framesAvailable ? remaining : framesAvailable;
		long sampleOffset = vo->readFrames * 2;
		float volume = s->muted ? 0 : a->volumes[s->volGroup] * s->volume;
		if (volume > 0) {
			for (long frame = 0; frame < framesToMix; frame++) {
				long outFrame = startFrame + frame;

				long outIndex = outFrame * 2;
				long buffIndex = sampleOffset + frame * 2;
				out[outIndex] += s->buff[buffIndex] * volume;
				out[outIndex + 1] += s->buff[buffIndex + 1] * volume;
			}
		}
		vo->readFrames += framesToMix;
		vo->bufferOffset = 0;
	}

	for (long i = 0; i < framesPerBuffer * 2; i++) {
		if (out[i] > 1.0f) {
			out[i] = 1.0f;
		} else if (out[i] < -1.0f) {
			out[i] = -1.0f;
		}
	}

	a->currentFrame += framesPerBuffer;
	/*
	float peak = 0.0f;
	double sumSquares = 0.0;

	for (unsigned long i = 0; i < framesPerBuffer * 2; i++) {
			float sample = out[i];

			if (fabsf(sample) > peak) {
					peak = fabsf(sample);
			}

			sumSquares += (double)sample * sample;
	}

	double rms = sqrt(sumSquares / (framesPerBuffer * 2));

	char msg[128];
	snprintf(msg, sizeof(msg),
					 "OUTPUT peak=%f rms=%f\n", peak, rms);
	debugWrite(msg);
	*/
	return paContinue;
}

void checkAudioCommands(long long currentFrame) {
	AudioCommand ac;
	while (AudioCommandQueue_aqPop(&audioQueue, &ac)) {
		//play audio command
		if (ac.cmd == 0) {
			for (int i = 0; i < VOICE_MAX; i++) {
				if (aMan->mix[i].sound == NULL) {
					Sound *s = &sounds->bank[ac.obj];
					aMan->mix[i].sound = s;
					aMan->mix[i].readFrames = 0;
					break;
				}
			}
		} else if(ac.cmd == 1) {
			int sound;
			memcpy(&sound, ac.data, sizeof(int));
			double frequency;
			memcpy(&frequency, ac.data + sizeof(int), sizeof(double));
			addAudioEvent(sound, ac.obj, frequency);
		} else if (ac.cmd == 2) {
			Sound *s = &sounds->bank[ac.obj];
			for (int i = 0; i < VOICE_MAX; i++) {
				if (aMan->mix[i].sound != NULL) {
					if (strcmp(s->file, aMan->mix[i].sound->file) == 0) {
						aMan->mix[i].sound = NULL;
					}
				}
			}
		} else if (ac.cmd == 3) {
			removeAudioEvents(ac.obj);
		} else if (ac.cmd == 4) {
			bool state;
			memcpy(&state, ac.data, sizeof(bool));
			setPauseOnEvent(ac.obj, state);
		} else if (ac.cmd == 5) {
			//pause all events
			bool state;
			memcpy(&state, ac.data, sizeof(bool));
			setPauseOnEvents(state, currentFrame);
		} else if (ac.cmd == 6) {
			//add rhythm
			uint64_t pattern;
			memcpy(&pattern, ac.data, sizeof(uint64_t));
			size_t size;
			memcpy(&size, ac.data + sizeof(uint64_t), sizeof(size_t));
			//BitSet *bs = addRhythm(pattern, ac.size);
			setRhythm(ac.obj, pattern, size);
		} else if (ac.cmd == 7) {
			Sound *s = &sounds->bank[ac.obj];
			memcpy(&s->volume, ac.data, sizeof(double));
			//s->volume = ac.data;
		} else if (ac.cmd == 8) {
			Sound *s = &sounds->bank[ac.obj];
			memcpy(&s->muted, ac.data, sizeof(bool));
		} else if (ac.cmd == 9) {
			setSound(ac.obj, *ac.data);
		}
	}
}

Voice *findFreeMixSpot() {
	for (int i = 0; i < VOICE_MAX; i++) {
		if (aMan->mix[i].sound == NULL) {
			return &aMan->mix[i];
		}
	}
	return NULL;
}


void changeVolumeGroup(int group, float vol) {
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
	s->volGroup = group;
}

void endAudio() {
	if (aMan && aMan->stream) {
		PaError err = Pa_StopStream(aMan->stream);
		if (err != paNoError) {
			//printf("%s\n", Pa_GetErrorText(err));
		}
		Pa_CloseStream(aMan->stream);
		aMan->stream = NULL;
	}
	endAudioScheduling();
	freeAudioManager();
	Pa_Terminate();
}

void freeAudioManager() {
	if (aMan) {
		freeSoundBank();
		//free events as well
		free(aMan->volumes);
		aMan->volumes = NULL;
		free(aMan);
		aMan = NULL;
	}
}

const char *__lsan_default_options(void) {
	return "suppressions=lsan.supp";
}
