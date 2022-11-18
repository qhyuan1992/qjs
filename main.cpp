#include <quickjs/quickjs.h>
#include <cstring>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static JSValue js_print(JSContext *ctx, JSValueConst this_val,
                        int argc, JSValueConst *argv)
{
    int i;
    const char *str;
    size_t len;

    for(i = 0; i < argc; i++) {
        if (i != 0)
            putchar(' ');
        str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str)
            return JS_EXCEPTION;
        fwrite(str, 1, len, stdout);
        JS_FreeCString(ctx, str);
    }
    putchar('\n');
    return JS_UNDEFINED;
}

void init_global_obj(JSContext *ctx) {
    // 导出全局属性console
    JSValue global_obj = JS_GetGlobalObject(ctx);
    JSValue console = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_print, "log", 1));
    JS_SetPropertyStr(ctx, global_obj, "console", console);
    JS_FreeValue(ctx, global_obj);
}

static JSClassID js_std_file_class_id;

typedef struct {
    FILE *f;
    int close_in_finalizer;
    int is_popen;
} JSSTDFile;

static void js_std_file_finalizer(JSRuntime *rt, JSValue val)
{
    JSSTDFile *s = (JSSTDFile *)JS_GetOpaque(val, js_std_file_class_id);
    if (s) {
        if (s->f && s->close_in_finalizer) {
            if (s->is_popen)
                pclose(s->f);
            else
                fclose(s->f);
        }
        js_free_rt(rt, s);
    }
}

static JSClassDef js_std_file_class = {
        "FILE",
        .finalizer = js_std_file_finalizer,
};

static FILE *js_std_file_get(JSContext *ctx, JSValueConst obj)
{
    JSSTDFile *s = (JSSTDFile*)JS_GetOpaque2(ctx, obj, js_std_file_class_id);
    if (!s)
        return NULL;
    if (!s->f) {
        JS_ThrowTypeError(ctx, "invalid file handle");
        return NULL;
    }
    return s->f;
}

static JSValue js_std_file_puts(JSContext *ctx, JSValueConst this_val,
                                int argc, JSValueConst *argv, int magic)
{
    FILE *f;
    int i;
    const char *str;
    size_t len;

    if (magic == 0) {
        f = stdout;
    } else {
        f = js_std_file_get(ctx, this_val);
        if (!f)
            return JS_EXCEPTION;
    }

    for(i = 0; i < argc; i++) {
        str = JS_ToCStringLen(ctx, &len, argv[i]);
        if (!str)
            return JS_EXCEPTION;
        fwrite(str, 1, len, f);
        JS_FreeCString(ctx, str);
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_std_file_proto_funcs[] = {
        JS_CFUNC_MAGIC_DEF("puts", 1, js_std_file_puts, 1 ),
};

static const JSCFunctionListEntry js_std_funcs[] = {
        JS_CFUNC_MAGIC_DEF("puts", 1, js_std_file_puts, 0 ),
};

static JSValue js_new_std_file(JSContext *ctx, FILE *f,
                               int close_in_finalizer,
                               int is_popen)
{
    JSSTDFile *s;
    JSValue obj;
    obj = JS_NewObjectClass(ctx, js_std_file_class_id);
    if (JS_IsException(obj))
        return obj;
    s = (JSSTDFile*)js_mallocz(ctx, sizeof(*s));
    if (!s) {
        JS_FreeValue(ctx, obj);
        return JS_EXCEPTION;
    }
    s->close_in_finalizer = close_in_finalizer;
    s->is_popen = is_popen;
    s->f = f;
    JS_SetOpaque(obj, s);
    return obj;
}

int js_std_init(JSContext *ctx, JSModuleDef *m) {
    JS_NewClassID(&js_std_file_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_std_file_class_id, &js_std_file_class);

    // 设置改class的prototype对象
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, proto, js_std_file_proto_funcs, countof(js_std_file_proto_funcs));
    JS_SetClassProto(ctx, js_std_file_class_id, proto);
    // 导出函数
    JS_SetModuleExportList(ctx, m, js_std_funcs, countof(js_std_funcs));
    // 导出属性
    JS_SetModuleExport(ctx, m, "out", js_new_std_file(ctx, stdout, 0, 0));
    return 0;
}


JSModuleDef* init_module_std(JSContext *ctx, const char *module_name) {
    JSModuleDef* m = JS_NewCModule(ctx, module_name, js_std_init);
    if (!m)
        return NULL;

    // 向外导出js_std_funcs中的函数和out对象；out对象是一个对象，其定义了原型对象中定义了一些函数
    JS_AddModuleExportList(ctx, m, js_std_funcs, countof(js_std_funcs));
    JS_AddModuleExport(ctx, m, "out");
    return m;
}

int main(int argc, char **argv)
{
    const char *scripts;

    JSRuntime *rt;
    JSContext *ctx;
    rt = JS_NewRuntime();
    ctx = JS_NewContext(rt);
    // 创建全局对象函数
    init_global_obj(ctx);
    // 测试console.log
    scripts = "console.log('hello quickjs')";
    JS_Eval(ctx, scripts, strlen(scripts), "main", 0);

    // 创建模块
    init_module_std(ctx, "std");
    // 测试模块
    scripts = "import * as std from 'std';"
              "std.out.puts('std.out.puts');"
              " std.puts('std.puts');";
    JS_Eval(ctx, scripts, strlen(scripts), "main", JS_EVAL_TYPE_MODULE);

    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}
