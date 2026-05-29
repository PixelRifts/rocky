#include <rocky/jit.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME_MAX 256

static void jit_verify_module_mutable(JITContext* ctx) {
    if (ctx->should_create_module) {
        char namebuf[MODULE_NAME_MAX] = {0};
        snprintf(namebuf, MODULE_NAME_MAX, "rocky_module_%d", ctx->created_module_count);
        
        ctx->current_module.handle = LLVMModuleCreateWithNameInContext((const char*) namebuf, ctx->ctx);
        LLVMSetTarget(ctx->current_module.handle, LLVMGetDefaultTargetTriple());
        ctx->current_module.threadsafe_handle = LLVMOrcCreateNewThreadSafeModule(ctx->current_module.handle, ctx->orc_threadsafe_ctx);
        
        ctx->created_module_count += 1;
        ctx->should_create_module = false;
    }
}

void jit_error_report(void* ctx, LLVMErrorRef err) {
    printf("JIT Error Report: %s\n", LLVMGetErrorMessage(err));
}

void jit_init(JITContext* ctx) {
    memset(ctx, 0, sizeof(JITContext));
    
    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    
    ctx->ctx = LLVMContextCreate();
    ctx->orc_threadsafe_ctx = LLVMOrcCreateNewThreadSafeContextFromLLVMContext(ctx->ctx);
    
    
    LLVMOrcLLJITBuilderRef jit_builder = LLVMOrcCreateLLJITBuilder();
    // No options to specify for jit_builder as of yet.
    // Seems to allow changing target machine spec or linking layer neither of which
    //  we need to change from default.
    
    LLVMErrorRef err = LLVMOrcCreateLLJIT(&ctx->jit, NULL);
    LLVMOrcDisposeLLJITBuilder(jit_builder);
    if (err) {
        fprintf(stderr, "JIT Engine could not be initialized\n");
        return;
    }
    
    
    LLVMOrcExecutionSessionRef session = LLVMOrcLLJITGetExecutionSession(ctx->jit);
    LLVMOrcExecutionSessionSetErrorReporter(session, jit_error_report, NULL);
    
    ctx->jit_dylib = LLVMOrcLLJITGetMainJITDylib(ctx->jit);
    ctx->should_create_module = true;
}

void jit_dylib_load(JITContext* ctx, const char* dylib_name) {
    LLVMLoadLibraryPermanently(dylib_name);
}

// @Temporary Adds printnum
void jit_add_dummy_functions(JITContext* ctx) {
    jit_verify_module_mutable(ctx);
    LLVMModuleRef module = ctx->current_module.handle;
    
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx->ctx);
    
    // Add printf
    LLVMTypeRef printf_args[] = { LLVMPointerType(LLVMInt8Type(), 0) };
    LLVMTypeRef printf_type = LLVMFunctionType(LLVMInt32TypeInContext(ctx->ctx), printf_args, 1, 1);
    LLVMValueRef printf_fn = LLVMAddFunction(module, "printf", printf_type);
    
    // Add simple printnum function
    LLVMTypeRef printnum_arg_types[] = { LLVMInt32TypeInContext(ctx->ctx) };
    LLVMTypeRef printnum_ret_type = LLVMVoidTypeInContext(ctx->ctx);
    LLVMTypeRef printnum_func_type = LLVMFunctionType(printnum_ret_type, printnum_arg_types, 1, 0);
    LLVMValueRef printnum = LLVMAddFunction(module, "printnum", printnum_func_type);
    
    // Add entry BB
    LLVMBasicBlockRef entry = LLVMAppendBasicBlockInContext(ctx->ctx, printnum, "entry");
    LLVMPositionBuilderAtEnd(builder, entry);
    
    // Printf call
    LLVMValueRef printf_call_fmtstr = LLVMBuildGlobalStringPtr(builder, "Hello %d\n", "str");
    LLVMValueRef printf_call_args[] = { printf_call_fmtstr, LLVMGetParam(printnum, 0) };
    LLVMBuildCall2(builder, printf_type, printf_fn, printf_call_args, 2, "printf_call");
    LLVMBuildRetVoid(builder);
    
    LLVMDisposeBuilder(builder);
}

void jit_bake(JITContext* ctx) {
    LLVMOrcLLJITAddLLVMIRModule(ctx->jit, ctx->jit_dylib, ctx->current_module.threadsafe_handle);
    ctx->should_create_module = true;
}

void_func* jit_lookup_function(JITContext* ctx, char* function_name) {
    LLVMOrcJITTargetAddress ret = 0;
    LLVMOrcLLJITLookup(ctx->jit, &ret, function_name);
    return (void_func*) ret;
}

void jit_free(JITContext* ctx) {
    LLVMOrcDisposeLLJIT(ctx->jit);
    LLVMOrcDisposeThreadSafeContext(ctx->orc_threadsafe_ctx);
}


//- Temporary Raylib-specific stuff for testing
void jit_add_raylib_functions(JITContext* ctx) {
    jit_verify_module_mutable(ctx);
    LLVMModuleRef module = ctx->current_module.handle;
    
    //~~~ All the declarations ~~~
    
    // Basic Types
    LLVMTypeRef i8_type = LLVMInt8TypeInContext(ctx->ctx);
    LLVMTypeRef i32_type = LLVMInt32TypeInContext(ctx->ctx);
    LLVMTypeRef void_type = LLVMVoidTypeInContext(ctx->ctx);
    LLVMTypeRef char_ptr_type = LLVMPointerType(i8_type, 0);
    
    // Color struct: { unsigned char r, g, b, a }
    LLVMTypeRef color_fields[] = { i8_type, i8_type, i8_type, i8_type };
    LLVMTypeRef color_type = LLVMStructTypeInContext(ctx->ctx, color_fields, 4, 0);
    
    // void InitWindow(int width, int height, const char *title)
    LLVMTypeRef init_window_args[] = { i32_type, i32_type, char_ptr_type };
    LLVMTypeRef init_window_type = LLVMFunctionType(void_type, init_window_args, 3, 0);
    LLVMValueRef init_window_fn = LLVMAddFunction(module, "InitWindow", init_window_type);
    
    // void SetTargetFPS(int fps)
    LLVMTypeRef set_target_fps_args[] = { i32_type };
    LLVMTypeRef set_target_fps_type = LLVMFunctionType(void_type, set_target_fps_args, 1, 0);
    LLVMValueRef set_target_fps_fn = LLVMAddFunction(module, "SetTargetFPS", set_target_fps_type);
    
    // int WindowShouldClose(void)
    // Raylib returns C bool, but we'll use i32
    LLVMTypeRef window_should_close_type = LLVMFunctionType(i32_type, NULL, 0, 0);
    LLVMValueRef window_should_close_fn = LLVMAddFunction(module, "WindowShouldClose", window_should_close_type);
    
    // void BeginDrawing(void)
    LLVMTypeRef begin_drawing_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef begin_drawing_fn = LLVMAddFunction(module, "BeginDrawing", begin_drawing_type);
    
    // void ClearBackground(Color color)
    LLVMTypeRef clear_background_args[] = { color_type };
    LLVMTypeRef clear_background_type = LLVMFunctionType(void_type, clear_background_args, 1, 0);
    LLVMValueRef clear_background_fn = LLVMAddFunction(module, "ClearBackground", clear_background_type);
    
    // void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
    LLVMTypeRef draw_text_args[] = { char_ptr_type, i32_type, i32_type, i32_type, color_type };
    LLVMTypeRef draw_text_type = LLVMFunctionType(void_type, draw_text_args, 5, 0);
    LLVMValueRef draw_text_fn = LLVMAddFunction(module, "DrawText", draw_text_type);
    
    // void EndDrawing(void)
    LLVMTypeRef end_drawing_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef end_drawing_fn = LLVMAddFunction(module, "EndDrawing", end_drawing_type);
    
    // void CloseWindow(void)
    LLVMTypeRef close_window_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef close_window_fn = LLVMAddFunction(module, "CloseWindow", close_window_type);
    
    
    //~~~ Example Function ~~~
    LLVMBuilderRef builder = LLVMCreateBuilderInContext(ctx->ctx);
    
    // Basic blocks and decls
    LLVMTypeRef  run_fn_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMValueRef run_fn      = LLVMAddFunction(module, "run_raylib_example", run_fn_type);
    
    LLVMBasicBlockRef entry_bb     = LLVMAppendBasicBlockInContext(ctx->ctx, run_fn, "entry");
    LLVMBasicBlockRef loop_cond_bb = LLVMAppendBasicBlockInContext(ctx->ctx, run_fn, "loop_cond");
    LLVMBasicBlockRef loop_body_bb = LLVMAppendBasicBlockInContext(ctx->ctx, run_fn, "loop_body");
    LLVMBasicBlockRef loop_exit_bb = LLVMAppendBasicBlockInContext(ctx->ctx, run_fn, "loop_exit");
    
    LLVMValueRef raywhite_vals[] = {
        LLVMConstInt(i8_type, 245, 0),
        LLVMConstInt(i8_type, 245, 0),
        LLVMConstInt(i8_type, 245, 0),
        LLVMConstInt(i8_type, 255, 0),
    };
    LLVMValueRef color_raywhite = LLVMConstNamedStruct(color_type, raywhite_vals, 4);
    
    LLVMValueRef blue_vals[] = {
        LLVMConstInt(i8_type,  50, 0),
        LLVMConstInt(i8_type,  80, 0),
        LLVMConstInt(i8_type, 200, 0),
        LLVMConstInt(i8_type, 255, 0),
    };
    LLVMValueRef color_blue = LLVMConstNamedStruct(color_type, blue_vals, 4);
    
    // Entry BB
    LLVMPositionBuilderAtEnd(builder, entry_bb);
    
    LLVMValueRef window_title = LLVMBuildGlobalStringPtr(builder, "Minimal Raylib Window", "window_title");
    {
        LLVMValueRef args[] = {
            LLVMConstInt(i32_type, 800, 0),
            LLVMConstInt(i32_type, 450, 0),
            window_title,
        };
        LLVMBuildCall2(builder, init_window_type, init_window_fn, args, 3, "");
    }
    {
        LLVMValueRef args[] = { LLVMConstInt(i32_type, 60, 0) };
        LLVMBuildCall2(builder, set_target_fps_type, set_target_fps_fn, args, 1, "");
    }
    LLVMBuildBr(builder, loop_cond_bb);
    
    // Loop condition BB
    LLVMPositionBuilderAtEnd(builder, loop_cond_bb);
    
    LLVMValueRef should_close = LLVMBuildCall2(builder, window_should_close_type, window_should_close_fn, NULL, 0, "should_close");
    LLVMValueRef keep_going = LLVMBuildICmp(builder, LLVMIntEQ, should_close, LLVMConstInt(i32_type, 0, 0), "keep_going");
    LLVMBuildCondBr(builder, keep_going, loop_body_bb, loop_exit_bb);
    
    // Loop body BB
    LLVMPositionBuilderAtEnd(builder, loop_body_bb);
    
    LLVMBuildCall2(builder, begin_drawing_type, begin_drawing_fn, NULL, 0, "");
    {
        LLVMValueRef args[] = { color_raywhite };
        LLVMBuildCall2(builder, clear_background_type, clear_background_fn, args, 1, "");
    }
    LLVMValueRef congrats_str = LLVMBuildGlobalStringPtr(builder, "Congrats! You created your first Raylib window.", "congrats_str");
    {
        LLVMValueRef args[] = {
            congrats_str,
            LLVMConstInt(i32_type, 190, 0),
            LLVMConstInt(i32_type, 200, 0),
            LLVMConstInt(i32_type,  20, 0),
            color_blue,
        };
        LLVMBuildCall2(builder, draw_text_type, draw_text_fn, args, 5, "");
    }
    LLVMBuildCall2(builder, end_drawing_type, end_drawing_fn, NULL, 0, "");
    LLVMBuildBr(builder, loop_cond_bb);
    
    // Loop exit BB
    LLVMPositionBuilderAtEnd(builder, loop_exit_bb);
    
    LLVMBuildCall2(builder, close_window_type, close_window_fn, NULL, 0, "");
    LLVMBuildRetVoid(builder);
    
    LLVMDisposeBuilder(builder);
}
