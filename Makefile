TARGET = SonicPSP
OBJS = main.o

# SDL2 on PSP keeps its headers in a folder called SDL2, so we tell the compiler where.
PSPPREFIX = $(shell psp-config --psp-prefix)
SDL_CFLAGS = -I$(PSPPREFIX)/include/SDL2 $(shell psp-pkg-config --cflags sdl2 2>/dev/null)
SDL_LIBS = $(shell psp-pkg-config --libs sdl2 2>/dev/null)

# If pkg-config gave us nothing, use this fallback list of libraries.
ifeq ($(strip $(SDL_LIBS)),)
SDL_LIBS = -lSDL2 -lGL -lGLU -lglut -lz -lpspvfpu -lpsphprm -lpspsdk \
           -lpspctrl -lpsppower -lpspgu -lpspge -lpspdisplay -lpspaudio -lpspaudiolib
endif

CFLAGS = -O2 -G0 -Wall $(SDL_CFLAGS)
LIBS = -lSDL2main $(SDL_LIBS) -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = Sonic PSP Fan Game

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
