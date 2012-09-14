MAIN:=git-credential-wincred
all:: $(MAIN).exe

CC = gcc
RM = rm -f
CFLAGS = -O2 -Wall
CPPFLAGS = -DNO_STRNDUP

-include ../../../config.mak.autogen
-include ../../../config.mak

HELPER:=../helper
VPATH +=$(HELPER)

SRCS:=$(MAIN).c
SRCS+=credential_helper.c
OBJS:=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(HELPER) -o $@ -c $<

$(MAIN).exe: $(OBJS)
	$(LINK.o) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean:
	$(RM) $(MAIN).exe $(OBJS)
