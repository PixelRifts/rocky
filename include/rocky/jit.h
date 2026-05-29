/* date = April 25th 2026 4:26 pm */

#ifndef JIT_H
#define JIT_H

#include <llvm-c/Core.h>
#include <llvm-c/Types.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>
#include <llvm-c/Support.h>
#include <llvm-c/Orc.h>
#include <llvm-c/LLJIT.h>

#include <stdbool.h>
#include <rocky/parser/ast.h>

typedef void void_func(void);

typedef struct JITModule JITModule;
struct JITModule {
    LLVMModuleRef handle;
    LLVMOrcThreadSafeModuleRef threadsafe_handle;
};

typedef struct JITContext JITContext;
struct JITContext {
    LLVMContextRef ctx;
    
    LLVMOrcLLJITRef    jit;
    LLVMOrcJITDylibRef jit_dylib;
    LLVMOrcThreadSafeContextRef orc_threadsafe_ctx;
    
    int created_module_count;
    // We only need to store the most recent module that is mutable for now.
    JITModule current_module;
    bool should_create_module;
};

void       jit_init(JITContext* ctx);
void       jit_free(JITContext* ctx);
void       jit_dylib_load(JITContext* ctx, const char* dylib_name);
void       jit_add_dummy_functions(JITContext* ctx);
void       jit_bake(JITContext* ctx);
void_func* jit_lookup_function(JITContext* ctx, char* function_name);

//- Temporary Raylib-specific stuff for testing
void jit_add_raylib_functions(JITContext* ctx);


#endif //JIT_H
