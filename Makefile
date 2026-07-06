TARGET = audioTest

DEV_CFLAGS = -g -fsanitize=address,undefined -fno-omit-frame-pointer
DEV_LDFLAGS = -fsanitize=address,undefined

TSAN_CFLAGS = -g -O1 -fsanitize=thread -fno-omit-frame-pointer
TSAN_LDFLAGS = -fsanitize=thread

PROD_CFLAGS = -O2
PROD_LDFLAGS =

CFLAGS = -MMD -MP
LDFLAGS =

dev: CFLAGS += $(DEV_CFLAGS)
dev: LDFLAGS += $(DEV_LDFLAGS)
dev: $(TARGET)

tsan: CFLAGS += $(TSAN_CFLAGS)
tsan: LDFLAGS += $(TSAN_LDFLAGS)
tsan: $(TARGET)

prod: CFLAGS += $(PROD_CFLAGS)
prod: LDFLAGS += $(PROD_LDFLAGS)
prod: $(TARGET)

$(TARGET): AudioMan.h  libAudioMan.a  libHelper.a main.o 
	gcc main.o -o $@ $(LDFLAGS) libAudioMan.a libHelper.a -lportaudio -lsndfile -lm

libAudioMan.a: Sound.o AudioMan.h
	ar rs libAudioMan.a Sound.o

AudioMan.h: Sound.o helper.h
	@echo "Generating portable sound headers"
	@cat helper.h Sound.h  >> AudioMan.h

Sound.o: Sound.c Sound.h Bank.c helper.h
	gcc $(CFLAGS) -c Sound.c -lportaudio -lsndfile

libHelper.a:
	$(MAKE) -C ../FormNetwork/
	cp ../FormNetwork/libHelper.a .

helper.h:
	$(MAKE) -C ../FormNetwork/
	cp ../FormNetwork/helper.h .

clean:
	rm -f *.o *.a *.d

fclean:
	rm -f *.o *.a *.d helper.h audioTest AudioMan.h

# merges .d files into dependency graph
-include *.d
