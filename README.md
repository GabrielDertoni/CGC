# CGC - A dumm Garbage Collector for C.

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
garbage collector may free it's memory!

## Usage

Now for the nice things. Usage is simple, include `cgc.h` in all your `.c` files
and replace usages of `malloc` for `cgmalloc`, `calloc` for `cgcalloc` and
`realloc` with `gcrealloc`. In the `main` function, before doing any allocation,
use the macro `CG_INIT(argv)`, where `argv` is the argument received by `main`.
Finally, compile and link with `cgc.c`.

## Future things

Maybe it would be possible to create some solution using a shim to overwrite the
default `malloc`, `calloc`, `realloc` and `free` directly. In this case, the
user would just have to link the library and CGC would do the rest. `free` could
just be replaced by a function that does nothing, in case the user has forgotten
to remove from the codebase.
