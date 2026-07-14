AudioEventData eventManifest[AUDIO_EVENT_MAX];
SoundBank *sounds = 0;

int processAudioFile(char *file, bool loop) {
	//linkedList *cur = aMan->sounds;
	int fileLen = strlen(file);
	for (int i = 0; i < sounds->soundNum; i++) {
		Sound s = sounds->bank[i];
		if (memcmp(s.file, file, fileLen) == 0) {
			//printf("already have the sound\n");
			return i;
		}
	}
	SNDFILE *infile = 0;
	SF_INFO sfinfo = {0};
	if (!(infile = sf_open(file, SFM_READ, &sfinfo))) {
		//printf ("Not able to open input file %s.\n", file) ;
		sf_perror (NULL) ;
		return  0 ;
	}
	long read = 0;
	long size = sf_seek(infile, 0, SEEK_END);
	sf_seek(infile, 0, SEEK_SET);
	Sound *s = &sounds->bank[sounds->soundNum];//calloc(1, sizeof(Sound));
	s->file = calloc(fileLen + 1, sizeof(char));
	memcpy(s->file, file, fileLen);
	s->file[fileLen] = '\0';
	s->readFrames = 0;
	s->buff = calloc(size, sizeof(float));
	s->volume = 1;//aMan->volumes;
	s->loop = loop;
	sf_read_float(infile, s->buff, size);
	//s->len = size / (FPB * 2);
	s->totalFrames = size / 2;
	sf_close(infile);
	//addToList(&aMan->sounds, s);
	sounds->soundNum++;
	return sounds->soundNum-1;
}

void playAudio(int sound) {
	if (sound >= 0 && sound < sounds->soundNum) {
		AudioCommand ac;
		ac.cmd = 0;
		ac.obj = sound;
		ac.data = 0;
		AudioCommandQueue_aqPush(&audioQueue, ac);
	}
}

void stopAudio(int sound) {
	if (sound >= 0 && sound < sounds->soundNum) {
		/*
		AudioCommand ac;
		ac.cmd = 2;
		ac.data = sound;
		aqPush(&audioQueue, &ac, sizeof(AudioCommand));
		*/
		addAudioCommand(2, sound, 0);
	}
}

void scheduleAudio(int sound, double frequency) {
	if (sound >= 0 && sound < sounds->soundNum) {
		/*
		AudioCommand ac;
		ac.cmd = 0;
		ac.sound = sound;
		ac.data = frequency;
		aqPush(&audioQueue, &ac, sizeof(AudioCommand));
		*/
		addAudioCommand(0, sound, frequency);
	}
}

void unScheduleAudio(int sound) {
	if (sound >= 0 && sound < sounds->soundNum) {
		/*
		AudioCommand ac;
		ac.cmd = 3;
		ac.data = 1;
		ac.sound = sound;
		aqPush(&audioQueue, &ac, sizeof(AudioCommand));
		*/
		// 1 indicates sound rather than event
		addAudioCommand(3, sound, 1);
	}
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

void unscheduleEvent(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		// 2 indicates event rather than sound
		addAudioCommand(3, event, 2);
		eventManifest[event].func = 0;
		eventManifest[event].data = 0;
	}
}

void pauseAudioEvent(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		addAudioCommand(4, event, 2);
	}
}

void unpauseAudioEvent(int event) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		addAudioCommand(5, event, 2);
	}
}

void setVolume(int sound, double volume) {
	addAudioCommand(6, sound, volume);
}

void addAudioCommand(int cmd, int obj, double data) {
	AudioCommand ac;
	ac.cmd = cmd;
	ac.obj = obj;
	ac.data = data;
	AudioCommandQueue_aqPush(&audioQueue, ac);
}

void parseAudioEvents() {
	int command;
	while (IntQueue_aqPop(&audioEventQueue, &command)) {
		if (eventManifest[command].func != 0) {
			void *data = eventManifest[command].data;
			eventManifest[command].func(data);
		}
	}
}

void flushAudioEvents() {
	int toilet;
	while (IntQueue_aqPop(&audioEventQueue, &toilet)) {}
}

void freeSound(void *snd) {
	Sound *s = snd;
	free(s->file);
	free(s->buff);
	free(s);
}

void freeSoundBank() {
	if (!sounds) {
		return;
	}
	for (int i = 0; i < sounds->soundNum; i++) {
		free(sounds->bank[i].file);
		free(sounds->bank[i].buff);
	}
	free(sounds);
	sounds = NULL;
}

