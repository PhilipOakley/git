MAIN:=git-credential-osxkeychain
all:: $(MAIN)

CC = gcc
RM = rm -f
CFLAGS = -g -O2 -Wall

-include ../../../config.mak.autogen
-include ../../../config.mak

INCS:=
LIBS:=-Wl,-framework -Wl,Security
HELPER:=../helper
VPATH +=$(HELPER)

SRCS:=$(MAIN).c
SRCS+=credential_helper.c
OBJS:=$(SRCS:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -I$(HELPER) $(INCS) -o $@ -c $<

$(MAIN): $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	@$(RM) $(MAIN) $(OBJS)
