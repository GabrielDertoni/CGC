# C-GC: A dumm Garbage Collector for C

## WARNING

**DO NOT USE IN REAL USEFUL CODE**

This is a very simple garbage collector that is not efficient but works! Well,
sometimes. In order to mark allocated blocks that are reachable, it traverses
the stack and tries to find something that looks like a pointer and points to
one of the allocated blocks. If it finds that, it marks the block as referenced.
When it reaches the end of the stack, it checks for unmarked blocks and frees
them. This means **it is very unsafe** and could only work without much or any
optimizations. This is because if the compiler decides that some pointer
variable doesn't need to be in the stack and can simply be a register, the
garbage collector may free it's memory! Or if the pointer is not aligned to 8
bytes (for some reason).

Note that the garbage collector may mark blocks as reached by accident. If in
the stack there were two 32 bit integers one after the other, and the bits just
coincide to represent a 64 bit pointer to one of the blocks, it will mark that
block as visited even if it wasn't really a reference. However, marking blocks
as visited isn't a memory safety concern since it will just not free it _now_.
The hope is that this sort of thing doesn't happen very often, and if it does,
it's short lived.

## Usage

There are many ways of using the library. The classic way is to always use
`gcmalloc`, `gccalloc` and `gcrealloc` in order to allocate memory. No usage of
`free` is needed (of course). This will require the code to be compiled and
linked with `cgc.c`.

The quickest way is if you have a single file, just include `cgc.inl` and use
the usual `malloc`, `calloc` and `realloc` directly. This can be a quick hack to
avoid manual memory management. `free` will just do nothing.

Another possibility is to compile `shim.c` into a dynamic library and use
`LD_PRELOAD` with it before your binary. In this way you can just avoit manual
memory management in an already existing binary.

Finally you could use static linking with `shim.c`. That would work, but note
that tools like `valgrind` will overwrite `malloc`, etc. to the usual
implementations so the garbage collector would not be active when running on
`valgrind`.

### Rules

- No obscured pointers such as XOR linked lists
- No packing of pointers in structures. All pointers should be aligned.
- Only works on Linux, x86_64, with GCC or Clang compilers.

## Configuration

As of now, the only possible configuration is the `CG_INTERVAL` definition. It
specifies how many calls to the API before the garbage collector is run. Setting
it to `0` will run the garbage collector on every API call.

## Future things

Keeping track of blocks can be optimized a lot by using some sort of balanced
tree, preferably a B-Tree.

## Inspiration and credit

This implementation was very much inspired by [Tsoding's memalloc](https://github.com/tsoding/memalloc).
