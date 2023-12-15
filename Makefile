CC = gcc

#Para mais informações sobre as flags de warning, consulte a informação adicional no lab_ferramentas
CFLAGS = -g -std=c17 -D_POSIX_C_SOURCE=200809L \
		 -Wall -Werror -Wextra \
		 -Wcast-align -Wconversion -Wfloat-equal -Wformat=2 -Wnull-dereference -Wshadow -Wsign-conversion -Wswitch-enum -Wundef -Wunreachable-code -Wunused \
		 -fsanitize=address -fsanitize=undefined

ifneq ($(shell uname -s),Darwin) # if not MacOS
	CFLAGS += -fmax-errors=5
endif

all: clean ems run compare

# event management system
ems: main.c constants.h operations.o parser.o eventlist.o aux.o
	$(CC) $(CFLAGS) $(SLEEP) -o ems main.c operations.o parser.o eventlist.o aux.o

%.o: %.c %.h
	$(CC) $(CFLAGS) -c ${@:.o=.c}

# ems, jobs, processes, threads, delay
run: ems
	@./ems jobs 3 2 0

clean:
	rm -f *.o ems jobs/*.out jobs/*.out jobs2/*.out jobs/*.diff


compare:
	@for i in `ls jobs/*.out | sed -e "s/.out//"` ; do $(MAKE) -s $$i; done

jobs/%:
	diff jobs/$*.result jobs/$*.out > jobs/$*.diff

format:
	@which clang-format >/dev/null 2>&1 || echo "Please install clang-format to run this command"
	clang-format -i *.c *.h

backup:
	zip backups/backup-$(shell date +%d-%m-%Y-%T) *.c *.h Makefile jobs*/* jobs/*