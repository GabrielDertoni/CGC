CC := gcc
CFLAGS := -O0 -g
LFLAGS := -ldl

all: tree minimal

%: cgc.c examples/%.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

%:%.c
	$(CC) $(CFLAGS) $< -o $@ $(LFLAGS)

shim.so: shim.c cgc.c cgc.h
	$(CC) $(CFLAGS) -fpic -shared -DGC_EXTERN_ALLOC $^ -o $@ $(LFLAGS)

libcgc.a: shim.c cgc.c cgc.h
	$(CC) $(CFLAGS) -DGC_EXTERN_ALLOC -c shim.c -o shim.o $(LFLAGS)
	$(CC) $(CFLAGS) -DGC_EXTERN_ALLOC -c cgc.c -o cgc.o $(LFLAGS)
	ld -r shim.o cgc.o -o $@
	# Cleanup
	rm shim.o cgc.o
