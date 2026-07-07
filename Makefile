TARGET = audioTest

LIBDIR = lib/
INCDIR = include/

HELPERDIR = ../HelperFuncs/
HELPERINC = $(HELPERDIR)include/
HELPERLIB = $(HELPERDIR)lib/

DEV_CFLAGS = -g -fsanitize=address,undefined -fno-omit-frame-pointer
DEV_LDFLAGS = -fsanitize=address,undefined

TSAN_CFLAGS = -g -O1 -fsanitize=thread -fno-omit-frame-pointer
TSAN_LDFLAGS = -fsanitize=thread

PROD_CFLAGS = -O2
PROD_LDFLAGS =

CFLAGS = -MMD -MP -I$(HELPERINC) -I$(INCDIR)
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

$(TARGET): $(INCDIR)AudioMan.h  $(LIBDIR)libAudioMan.a  $(HELPERLIB)libHelper.a  $(HELPERINC)helper.h main.o 
	gcc main.o -o $@ $(LDFLAGS) $(LIBDIR)libAudioMan.a -L$(HELPERLIB) -lHelper -lportaudio -lsndfile -lm

$(HELPERLIB)libHelper.a:
	$(MAKE) -C $(HELPERDIR)

$(LIBDIR)libAudioMan.a: Sound.o | $(LIBDIR)
	ar rs $@ Sound.o

main.o: main.c
	gcc $(CFLAGS) -c main.c -o $@

Sound.o: Sound.c $(INCDIR)Sound.h Bank.c
	gcc $(CFLAGS) -c Sound.c -lportaudio -lsndfile

$(LIBDIR):
	mkdir -p $(LIBDIR)

clean:
	rm -f *.o *.d

fclean:
	rm -f *.o *.d $(TARGET) $(LIBDIR)libAudioMan.a

# merges .d files into dependency graph
-include *.d
