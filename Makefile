CC := gcc
CFLAGS := -O0 -g
LFLAGS := -ldl

all: tree minimal

%: cgc.c examples/%.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

%:%.c
	$(CC) $(CFLAGS) $< -o $@ $(LFLAGS)
