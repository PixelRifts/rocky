#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <stdio.h>
#include <stdlib.h>

#include <rocky/main.h>
#include <rocky/jit.h>

typedef void printnum_fn(int p);
typedef void raylib_example_fn(void);

int main() {
    JITContext jit = {0};
    jit_init(&jit);
    
    jit_dylib_load(&jit, "raylib.dll");
    jit_add_raylib_functions(&jit);
    jit_add_dummy_functions(&jit);
    jit_bake(&jit);
    
    printnum_fn* fn = (printnum_fn*) jit_lookup_function(&jit, "printnum");
    fn(10);
    fn(20);
    fn(30);
    
    raylib_example_fn* raylib_example = (raylib_example_fn*) jit_lookup_function(&jit, "run_raylib_example");
    raylib_example();
    
    jit_free(&jit);
    
    hello_world();
    
    return 0;
}
