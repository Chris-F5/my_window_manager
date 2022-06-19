OUTPUT = mwm
CC = gcc
LDFLAGS = -lxcb -llua
SRC = my_window_manager.c decoration.c
HEADERS = my_window_manager.h decoration.h
OBJS = $(SRC:.c=.o)

$(OUTPUT): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) -c $<

OBJS: $(HEADERS)

.PHONY: clean

clean:
	rm -f $(OBJS) $(OUTPUT)
