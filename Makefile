CC = gcc -c -g

audioTest: main.c libHelper.a libAudioMan.a 
	gcc main.c libAudioMan.a libHelper.a -lportaudio -lsndfile -lm -o audioTest

libAudioMan.a: Sound.o AudioMan.h
	ar rs libAudioMan.a Sound.o

AudioMan.h: Sound.o helper.h
	@echo "Generating portable sound headers"
	@cat helper.h Sound.h  >> AudioMan.h

Sound.o: Sound.c Sound.h Bank.c
	$(CC) Sound.c

libHelper.a:
	$(MAKE) -C ../
	cp ../helper.h .
	cp ../libHelper.a .

clean:
	rm audioTest
	rm *.o

fclean:
	rm audioTest
	rm *.o
	rm AudioMan.h
	rm libAudioMan.a
