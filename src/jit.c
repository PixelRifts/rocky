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
    LLVMAddFunction(module, "InitWindow", init_window_type);
    
    // void SetTargetFPS(int fps)
    LLVMTypeRef set_target_fps_args[] = { i32_type };
    LLVMTypeRef set_target_fps_type = LLVMFunctionType(void_type, set_target_fps_args, 1, 0);
    LLVMAddFunction(module, "SetTargetFPS", set_target_fps_type);
    
    // int WindowShouldClose(void)
    // Raylib returns C bool, but we'll use i32
    LLVMTypeRef window_should_close_type = LLVMFunctionType(i32_type, NULL, 0, 0);
    LLVMAddFunction(module, "WindowShouldClose", window_should_close_type);
    
    // void BeginDrawing(void)
    LLVMTypeRef begin_drawing_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMAddFunction(module, "BeginDrawing", begin_drawing_type);
    
    // void ClearBackground(Color color)
    LLVMTypeRef clear_background_args[] = { color_type };
    LLVMTypeRef clear_background_type = LLVMFunctionType(void_type, clear_background_args, 1, 0);
    LLVMAddFunction(module, "ClearBackground", clear_background_type);
    
    // void DrawText(const char *text, int posX, int posY, int fontSize, Color color)
    LLVMTypeRef draw_text_args[] = { char_ptr_type, i32_type, i32_type, i32_type, color_type };
    LLVMTypeRef draw_text_type = LLVMFunctionType(void_type, draw_text_args, 5, 0);
    LLVMAddFunction(module, "DrawText", draw_text_type);
    
    // void EndDrawing(void)
    LLVMTypeRef end_drawing_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMAddFunction(module, "EndDrawing", end_drawing_type);
    
    // void CloseWindow(void)
    LLVMTypeRef close_window_type = LLVMFunctionType(void_type, NULL, 0, 0);
    LLVMAddFunction(module, "CloseWindow", close_window_type);
}
