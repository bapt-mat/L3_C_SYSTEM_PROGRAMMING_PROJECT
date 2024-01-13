CC = gcc
CCFLAGS = -Wall -O3
PROGS = initial archiviste journaliste
SRCS_INITIAL = initial.c 
SRCS_ARCHIVISTE = archiviste.c 
SRCS_JOURNALISTE = journaliste.c
OBJS_INITIAL = $(SRCS_INITIAL:.c=.o)
OBJS_ARCHIVISTE = $(SRCS_ARCHIVISTE:.c=.o)
OBJS_JOURNALISTE = $(SRCS_JOURNALISTE:.c=.o)

all: $(PROGS)

initial: $(OBJS_INITIAL)
	$(CC) $(CCFLAGS) $^ -o $@

archiviste: $(OBJS_ARCHIVISTE)
	$(CC) $(CCFLAGS) $^ -o $@

journaliste: $(OBJS_JOURNALISTE)
	$(CC) $(CCFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

clean:
	rm -f *.o $(PROGS)
