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


/*
 * -emit-llvm
*
*#include "raylib.h"
*
*int main(void) {
    *  const int screenWidth = 800;
    *  const int screenHeight = 450;
    *  InitWindow(screenWidth, screenHeight, "Minimal Raylib Window");
    *  SetTargetFPS(60);
*
    *  while (!WindowShouldClose())
    *  {
        *    BeginDrawing();
            *    ClearBackground(RAYWHITE);
            *    DrawText("Congrats! You created your first Raylib window.", 190, 200, 20, DARKGRAY);
        *    EndDrawing();
    *  }
*
    *  CloseWindow();
    *  return 0;
*}
*/


//   define void @run_raylib_example() {
//   entry:
//     call void @InitWindow(i32 800, i32 450, ptr @window_title)
//     call void @SetTargetFPS(i32 60)
//     br label %loop_cond
//   loop_cond:
//     %should_close = call i32 @WindowShouldClose()
//     %keep_going   = icmp eq i32 %should_close, 0
//     br i1 %keep_going, label %loop_body, label %loop_exit
//   loop_body:
//     call void @BeginDrawing()
//     call void @ClearBackground({ i8, i8, i8, i8 } { 245, 245, 245, 255 })
//     call void @DrawText(ptr @congrats_str, i32 190, i32 200, i32 20,
//                         { i8, i8, i8, i8 } { 80, 80, 80, 255 })
//     call void @EndDrawing()
//     br label %loop_cond
//   loop_exit:
//     call void @CloseWindow()
//     ret void
//   }


#endif //JIT_H
