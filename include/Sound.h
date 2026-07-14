#ifndef AUDIOMAN
#define AUDIOMAN

#include <portaudio.h>
#include <sndfile.h>
#define SOUND_MAX 256
#define EVENT_MAX 256

typedef struct Sound {
	//immutable
	char *file;
	float *buff;
	long totalFrames;

	//switch to atomics
	bool loop;
	float volume;
	int volGroup;

	//only audio thread controls
	bool active;
	long readFrames;
	//timing scheduling
	long long nextTriggerFrame;
	bool scheduled;
	long long intervalFrames;
} Sound;

typedef struct {
	int type;
	int data;

	long long nextTriggerFrame;
	bool paused;
	long long intervalFrames;
} AudioEvent;

typedef struct {
	AudioEvent events[EVENT_MAX];
	int eventNum;
} AudioEventScheduler;

#define AUDIO_EVENT_MAX 256
typedef struct {
	void (*func)(void*);
	void *data;
} AudioEventData;

typedef struct {
	Sound *sound;
	long readFrames;
	long long bufferOffset;
	//volume
} Voice;

typedef struct {
	Sound bank[SOUND_MAX];
	int soundNum;
} SoundBank;


int processAudioFile(char *file, bool loop);
void addAudioCommand(int cmd, int obj, double data);

void playAudio(int sound);
void stopAudio(int sound);
void scheduleAudio(int sound, double frequency);
void unScheduleAudio(int sound);
int scheduleEvent(void (*func)(void*), void *data, double frequency);
bool addAudioEvent(int type, int data, double frequency);
void unscheduleEvent(int event);
void pauseAudioEvent(int event);
void unpauseAudioEvent(int event);
void setPauseOnEvent(int type, int data, bool state);

void setVolume(int sound, double volume);
void removeAudioEvent(int type, int data);

void parseAudioEvents();
void flushAudioEvents();
void freeSound(void *snd);
//Frames PerBuffer
#define FPB 4096
#define VOICE_MAX 256

typedef struct {
	int cmd;
	int obj;
	double data;
} AudioCommand;

typedef struct AudioManager {
	//linkedList *mix;
	Voice mix[VOICE_MAX];
	int mixCount;
	float *volumes;
	int vGroups;
	PaStream *stream;

	long long currentFrame;
	double sampleRate;
	double bpm;
} AudioManager;

extern AudioManager *aMan;

int initAudio();
void endAudio();
Voice getVoice(Sound *s);
static int paLibsndfileCb(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void *userData);
void checkAudioCommands();
bool spawnVoice(AudioEvent *ae, long long bufferStart, long long bufferEnd);
Voice *findFreeMixSpot();
void changeVolumeGroup(int group, float vol);
void changeVolGroup(Sound *s, int group);
int addVolGroup();
void freeAudioManager();
void freeSoundBank();

#endif
