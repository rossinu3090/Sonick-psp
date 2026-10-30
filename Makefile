TARGET = SonicPSP
OBJS = main.o

CFLAGS = -O2 -G0 -Wall
LIBS = -lSDL2main -lSDL2 -lGL -lGLU -lglut -lz -lpspvfpu -lpsphprm -lpspsdk \
       -lpspctrl -lpsppower -lpspgu -lpspge -lpspdisplay -lpspaudio -lpspaudiolib -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Sonic PSP Fan Game

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
