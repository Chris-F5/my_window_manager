OUTPUT = mwm
CC = gcc
LDFLAGS = -lxcb -lxcb-keysyms
SRC = my_window_manager.c decoration.c
HEADERS = my_window_manager.h
OBJS = $(SRC:.c=.o)

$(OUTPUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $<

OBJS: $(HEADERS)

.PHONY: clean

clean:
	rm -f $(OBJS) $(OUTPUT)
