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
		AudioCommandQueue_aqPush(&audioQueue, ac);
	}
}

void stopAudio(int sound) {
	if (sound >= 0 && sound < sounds->soundNum) {
		addAudioCommand(2, sound, 0, 0);
	}
}

void scheduleAudio(int sound, double frequency) {
	if (sound >= 0 && sound < sounds->soundNum) {
		addAudioCommand(0, sound, &frequency, sizeof(double));
	}
}

void unScheduleAudio(int sound) {
	if (sound >= 0 && sound < sounds->soundNum) {
		// 1 indicates sound rather than event
		addAudioCommand(3, sound, 0, 0);
	}
}


void pauseAudioEvent(int event, bool state) {
	if (event >= 0 && event < AUDIO_EVENT_MAX) {
		addAudioCommand(4, event, &state, sizeof(bool));
	}
}

void pauseAudioEvents(bool pause) {
	addAudioCommand(5, 0, &pause, sizeof(bool));
}

void setVolume(int sound, double volume) {
	addAudioCommand(7, sound, &volume, sizeof(double));
}

void addAudioCommand(int cmd, int obj, void *data, uint16_t size) {
	AudioCommand ac;
	ac.cmd = cmd;
	ac.obj = obj;
	memcpy(ac.data, data, size);
	ac.size = size;
	AudioCommandQueue_aqPush(&audioQueue, ac);
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

