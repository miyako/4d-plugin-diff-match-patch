/*
 * Synthetic 4D engine boundary for ownership regressions.
 *
 * Links the real plugin sources (4DPlugin-Diff-Match-Patch.cpp,
 * support/4DPlugin-JSON.cpp, C_TEXT.cpp) and the bundled, unmodified
 * 4D Plugin API (4DPluginAPI.c). Only gCall4D is replaced: it models the
 * engine-side allocations (Unistrings, pictures, objects, handles) with
 * malloc/free so ASan reports double frees / use-after-free, and keeps a
 * live set of every engine resource so leaks are detected exactly.
 *
 * Engine semantics modelled (per the SDK contract):
 *  - PA_CreateVariable(eVK_Unistring/eVK_Picture) allocates an empty value.
 *  - PA_SetObjectProperty deep-copies the variable (strings, pictures) or
 *    retains (objects); the caller still owns its variable.
 *  - PA_GetObjectProperty returns an owned deep copy that must be cleared;
 *    on error the returned variable is left uninitialised (poisoned here).
 *  - PA_ClearVariable releases what the variable owns.
 */

#include "4DPluginAPI.h"
#include "PrivateTypes.h"
#include "EntryPoints.h"
#include "4DPlugin-JSON.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <functional>

extern "C" Call4DProcPtr gCall4D;

namespace {

typedef std::basic_string<PA_Unichar> u16;

struct Obj {
    int rc;
    std::map<u16, PA_Variable> props;
};

std::set<void *> g_strings;
std::set<void *> g_pictures;
std::set<Obj *> g_objects;
std::set<void *> g_handles;
bool g_fail_get = false;
int g_get_calls = 0;
/* Stands in for the uninitialised result of a failed PA_GetObjectProperty:
   readable (so the test fails by assertion, not by crashing) but never
   engine-owned, so clearing it is reported as a violation. */
PA_Unichar kPoisonBuf[] = { 'B', 'A', 'D', '!', 0 };
PA_Unichar *const kPoison = kPoisonBuf;

[[noreturn]] void die(const char *msg) {
    fprintf(stderr, "ENGINE VIOLATION: %s\n", msg);
    fflush(stderr);
    abort();
}

PA_Unistring new_string(const PA_Unichar *src, size_t len) {
    PA_Unichar *buf = (PA_Unichar *)malloc((len + 1) * sizeof(PA_Unichar));
    if(len) memcpy(buf, src, len * sizeof(PA_Unichar));
    buf[len] = 0;
    g_strings.insert(buf);
    PA_Unistring s;
    s.fLength = (PA_long32)len;
    s.fString = buf;
    s.fReserved1 = 0;
    s.fReserved2 = 0;
    return s;
}

void free_string(PA_Unistring *s) {
    if(!s->fString) return;
    if(s->fString == kPoison) die("disposed a string from a failed PA_GetObjectProperty");
    if(!g_strings.erase(s->fString)) die("disposed a string the engine does not own");
    free(s->fString);
    s->fString = 0;
    s->fLength = 0;
}

void *new_picture() {
    void *p = malloc(16);
    g_pictures.insert(p);
    return p;
}

void free_picture(void *p) {
    if(!p) return;
    if(!g_pictures.erase(p)) die("disposed a picture the engine does not own");
    free(p);
}

void release_object(Obj *o);

void clear_var(PA_Variable *v) {
    switch(v->fType) {
        case eVK_Unistring: free_string(&v->uValue.fString); break;
        case eVK_Picture: free_picture(v->uValue.fPicture); break;
        case eVK_Object: if(v->uValue.fObject) release_object((Obj *)v->uValue.fObject); break;
        default: break;
    }
    v->fType = eVK_Undefined;
}

PA_Variable copy_var(const PA_Variable &src) {
    PA_Variable dst = src;
    switch(src.fType) {
        case eVK_Unistring:
            dst.uValue.fString = new_string(src.uValue.fString.fString, src.uValue.fString.fLength);
            break;
        case eVK_Picture:
            dst.uValue.fPicture = new_picture();
            break;
        case eVK_Object:
            if(src.uValue.fObject) {
                Obj *o = (Obj *)src.uValue.fObject;
                if(!g_objects.count(o)) die("copied a dead object");
                o->rc++;
            }
            break;
        default: break;
    }
    return dst;
}

Obj *new_object() {
    Obj *o = new Obj();
    o->rc = 1;
    g_objects.insert(o);
    return o;
}

void release_object(Obj *o) {
    if(!g_objects.count(o)) die("released a dead object");
    if(--o->rc == 0) {
        for(auto &kv : o->props) clear_var(&kv.second);
        g_objects.erase(o);
        delete o;
    }
}

u16 key_of(const PA_Unistring &k) { return u16(k.fString, k.fLength); }

struct Handle { char *data; };

void engine(short selector, EngineBlock *eb) {
    switch(selector) {
        case EX_CREATE_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_DISPOSE_UNISTRING:
            free_string(&eb->fUniString1);
            break;
        case EX_SET_UNISTRING: {
            const PA_Unichar *src = (const PA_Unichar *)eb->fHandle;
            size_t len = 0;
            while(src[len]) len++;
            free_string(&eb->fUniString1);
            eb->fUniString1 = new_string(src, len);
            break;
        }
        case EX_VARIABLE_TO_STRING: {
            PA_Variable *v = (PA_Variable *)eb->fHandle;
            if(v->fType == eVK_Unistring) eb->fUniString1 = v->uValue.fString; /* borrowed */
            break;
        }
        case EX_CREATE_PICTURE:
            eb->fPicture = new_picture();
            break;
        case EX_DISPOSE_PICTURE:
            free_picture(eb->fPicture);
            break;
        case EX_CLEAR_VARIABLE:
            clear_var((PA_Variable *)eb->fPtr1);
            break;
        case EX_SET_OBJ_VALUE: {
            Obj *o = (Obj *)eb->fPtr1;
            if(!g_objects.count(o)) die("set on a dead object");
            u16 k = key_of(eb->fUniString1);
            auto it = o->props.find(k);
            if(it != o->props.end()) clear_var(&it->second);
            o->props[k] = copy_var(*(PA_Variable *)eb->fPtr2);
            break;
        }
        case EX_GET_OBJ_VALUE: {
            g_get_calls++;
            Obj *o = (Obj *)eb->fPtr1;
            PA_Variable *out = (PA_Variable *)eb->fPtr2;
            if(g_fail_get) {
                memset(out, 0, sizeof(*out));
                out->fType = eVK_Unistring;
                out->uValue.fString.fString = kPoison;
                out->uValue.fString.fLength = 4;
                eb->fError = -1;
                break;
            }
            if(!g_objects.count(o)) die("get on a dead object");
            auto it = o->props.find(key_of(eb->fUniString1));
            if(it == o->props.end()) {
                memset(out, 0, sizeof(*out));
                out->fType = eVK_Undefined;
            } else {
                *out = copy_var(it->second);
            }
            break;
        }
        case EX_EXECUTE_COMMAND_BY_ID: {
            PA_Variable **params = (PA_Variable **)eb->fPtr1;
            PA_Variable *ret = (PA_Variable *)eb->fPtr2;
            if(eb->fParam1 == 1231 /* OB Is defined */) {
                Obj *o = (Obj *)params[0]->uValue.fObject;
                ret->fType = eVK_Boolean;
                ret->uValue.fBoolean = o && o->props.count(key_of(params[1]->uValue.fString)) ? 1 : 0;
            } else {
                die("unexpected PA_ExecuteCommandByID");
            }
            break;
        }
        case EX_HANDLE_MANAGER: {
            switch(eb->fParam1) {
                case 1: {
                    Handle *h = (Handle *)malloc(sizeof(Handle));
                    h->data = (char *)malloc(eb->fParam2 ? eb->fParam2 : 1);
                    g_handles.insert(h);
                    eb->fHandle = (PA_Handle)h;
                    break;
                }
                case 2: {
                    Handle *h = (Handle *)eb->fHandle;
                    if(!g_handles.erase(h)) die("disposed an unknown handle");
                    free(h->data);
                    free(h);
                    break;
                }
                case 5: eb->fParam3 = (sLONG_PTR)((Handle *)eb->fHandle)->data; break;
                case 6: break;
                default: die("unexpected handle manager op");
            }
            break;
        }
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "unmodelled engine selector %d", selector);
            die(msg);
        }
    }
}

/* ---------- test helpers ---------- */

PA_Unistring U(const char *ascii) {
    u16 s;
    for(const char *p = ascii; *p; p++) s.push_back((PA_Unichar)*p);
    return new_string(s.c_str(), s.size());
}

void put(Obj *o, const char *key, PA_Variable v) {
    PA_Unistring k = U(key);
    PA_SetObjectProperty((PA_ObjectRef)o, &k, v);
    PA_DisposeUnistring(&k);
}

PA_Variable real_var(double d) { PA_Variable v; memset(&v, 0, sizeof(v)); PA_SetRealVariable(&v, d); return v; }
PA_Variable bool_var(bool b) { PA_Variable v; memset(&v, 0, sizeof(v)); PA_SetBooleanVariable(&v, b); return v; }
PA_Variable text_var(const char *s) { PA_Variable v; memset(&v, 0, sizeof(v)); PA_Unistring u = U(s); PA_SetStringVariable(&v, &u); return v; }

void put_text(Obj *o, const char *key, const char *value) {
    PA_Variable v = text_var(value);
    put(o, key, v);
    PA_ClearVariable(&v);
}

bool check(bool ok, const char *what) {
    if(!ok) fprintf(stderr, "  check failed: %s\n", what);
    return ok;
}

bool no_leaks() {
    bool ok = true;
    if(!g_strings.empty()) { fprintf(stderr, "  LEAK: %zu engine string(s)\n", g_strings.size()); ok = false; }
    if(!g_pictures.empty()) { fprintf(stderr, "  LEAK: %zu engine picture(s)\n", g_pictures.size()); ok = false; }
    if(!g_objects.empty()) { fprintf(stderr, "  LEAK: %zu engine object(s)\n", g_objects.size()); ok = false; }
    if(!g_handles.empty()) { fprintf(stderr, "  LEAK: %zu engine handle(s)\n", g_handles.size()); ok = false; }
    return ok;
}

/* Runs a plugin command through the real PluginMain entry point. */
u16 run_command(PA_long32 selector, const char *a, const char *b, Obj *option) {
    PA_Unistring p1 = U(a), p2 = U(b);
    PA_ObjectRef opt = (PA_ObjectRef)option;
    void *params[3] = { &p1, &p2, &opt };
    PA_Unistring result = U("");
    PA_Unistring *resultPtr = &result;
    PluginBlock block;
    block.fParameters = params;
    block.fResult = &resultPtr;
    block.fData = 0;
    PluginMain(selector, &block);
    u16 out(result.fString, result.fLength);
    PA_DisposeUnistring(&result);
    PA_DisposeUnistring(&p1);
    PA_DisposeUnistring(&p2);
    return out;
}

std::string narrow(const u16 &s) { std::string r; for(PA_Unichar c : s) r.push_back((char)c); return r; }

/* ---------- tests ---------- */

typedef std::function<bool()> Test;

std::map<std::string, Test> tests() {
    std::map<std::string, Test> t;

    /* Defects: dormant object setters leak the empty owner allocated by
       PA_CreateVariable(eVK_Picture/eVK_Unistring) before the shallow set. */
    t["defect_ob_set_p_empty_owner"] = [] {
        Obj *o = new_object();
        ob_set_p((PA_ObjectRef)o, L"pic", (PA_Picture)new_picture()); /* helper takes the picture */
        bool ok = check(g_pictures.size() == 1, "only the object's copy of the picture is live");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_set_s_wkey_empty_owner"] = [] {
        Obj *o = new_object();
        ob_set_s((PA_ObjectRef)o, L"k", "value");
        release_object(o);
        return no_leaks();
    };
    t["defect_ob_set_s_ckey_empty_owner"] = [] {
        Obj *o = new_object();
        ob_set_s((PA_ObjectRef)o, "k", "value");
        release_object(o);
        return no_leaks();
    };
    t["defect_ob_set_a_wchar_empty_owner"] = [] {
        Obj *o = new_object();
        ob_set_a((PA_ObjectRef)o, L"k", L"value");
        release_object(o);
        return no_leaks();
    };
    t["defect_ob_set_a_u16_empty_owner"] = [] {
        Obj *o = new_object();
        CUTF16String v;
        for(const char *p = "value"; *p; p++) v.push_back((PA_Unichar)*p);
        ob_set_a((PA_ObjectRef)o, L"k", &v);
        release_object(o);
        return no_leaks();
    };

    /* Defects: PA_GetObjectProperty results are owned and never cleared. */
    t["defect_ob_get_a_result_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "hello");
        CUTF8String out;
        bool ok = check(ob_get_a((PA_ObjectRef)o, L"k", &out), "defined");
        ok = check(out == CUTF8String((const uint8_t *)"hello"), "value") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_n_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "diffTimeout", "not a number");
        bool ok = check(ob_get_n((PA_ObjectRef)o, L"diffTimeout") == 0.0, "wrong kind reads as 0");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_ob_get_b_wrong_kind_not_cleared"] = [] {
        Obj *o = new_object();
        put_text(o, "flag", "true");
        bool ok = check(ob_get_b((PA_ObjectRef)o, L"flag") == false, "wrong kind reads as false");
        release_object(o);
        return no_leaks() && ok;
    };

    /* Defects reachable from the shipped commands: a non-numeric option
       value is copied out by PA_GetObjectProperty inside ob_get_n and leaked. */
    t["defect_cmd_diff_text_option"] = [] {
        Obj *o = new_object();
        put_text(o, "diffTimeout", "1");
        u16 html = run_command(1, "abc", "abd", o);
        bool ok = check(!html.empty(), "diff returned html");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_cmd_match_text_option"] = [] {
        Obj *o = new_object();
        put_text(o, "near", "2");
        u16 loc = run_command(2, "abcdef", "cd", o);
        bool ok = check(narrow(loc) == "2", "match location");
        release_object(o);
        return no_leaks() && ok;
    };
    t["defect_cmd_patch_text_option"] = [] {
        Obj *o = new_object();
        put_text(o, "patchMargin", "4");
        u16 txt = run_command(3, "abc", "abd", o);
        bool ok = check(narrow(txt).find("@@") == 0, "patch text");
        release_object(o);
        return no_leaks() && ok;
    };

    /* Controls: must pass on both baseline and fixed sources. */
    t["control_ob_set_scalars"] = [] {
        Obj *o = new_object();
        ob_set_n((PA_ObjectRef)o, L"n", 1.5);
        ob_set_i((PA_ObjectRef)o, L"i", 7);
        ob_set_b((PA_ObjectRef)o, L"b", true);
        bool ok = check(o->props.size() == 3, "three properties");
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_ob_set_o_transfers_object"] = [] {
        Obj *o = new_object();
        Obj *child = new_object();
        ob_set_o((PA_ObjectRef)o, L"child", (PA_ObjectRef)child);
        bool ok = check(g_objects.count(child) && child->rc == 1, "parent holds the only reference");
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_ob_get_n_real"] = [] {
        Obj *o = new_object();
        put(o, "x", real_var(2.25));
        bool ok = check(ob_is_defined((PA_ObjectRef)o, L"x"), "defined");
        ok = check(!ob_is_defined((PA_ObjectRef)o, L"y"), "undefined") && ok;
        ok = check(ob_get_n((PA_ObjectRef)o, L"x") == 2.25, "value") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_ob_get_b_bool"] = [] {
        Obj *o = new_object();
        put(o, "f", bool_var(true));
        bool ok = check(ob_get_b((PA_ObjectRef)o, L"f") == true, "value");
        release_object(o);
        return no_leaks() && ok;
    };
    /* Guard (contract hardening, not a confirmed engine defect): on a failed
       PA_GetObjectProperty the result is neither read nor cleared. */
    t["guard_get_error_result_untouched"] = [] {
        Obj *o = new_object();
        put_text(o, "k", "v");
        put(o, "n", real_var(1));
        g_fail_get = true;
        CUTF8String out;
        ob_get_a((PA_ObjectRef)o, L"k", &out);
        ob_get_n((PA_ObjectRef)o, L"n");
        ob_get_b((PA_ObjectRef)o, L"k");
        g_fail_get = false;
        bool ok = check(g_get_calls == 3, "getter reached the engine");
        ok = check(out.empty(), "failed result was not read") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_cmd_real_options"] = [] {
        Obj *o = new_object();
        put(o, "diffTimeout", real_var(1));
        put(o, "near", real_var(2));
        put(o, "patchMargin", real_var(4));
        bool ok = check(!run_command(1, "abc", "abd", o).empty(), "diff");
        ok = check(narrow(run_command(2, "abcdef", "cd", o)) == "2", "match") && ok;
        ok = check(narrow(run_command(3, "abc", "abd", o)).find("@@") == 0, "patch") && ok;
        release_object(o);
        return no_leaks() && ok;
    };
    t["control_cmd_no_options"] = [] {
        bool ok = check(!run_command(1, "abc", "abd", 0).empty(), "diff");
        ok = check(narrow(run_command(2, "abcdef", "cd", 0)) == "2", "match") && ok;
        ok = check(narrow(run_command(3, "abc", "abd", 0)).find("@@") == 0, "patch") && ok;
        return no_leaks() && ok;
    };
    return t;
}

} // namespace

int main(int argc, char **argv) {
    gCall4D = engine;
    std::map<std::string, Test> all = tests();
    if(argc == 2 && !strcmp(argv[1], "--list")) {
        for(auto &kv : all) printf("%s\n", kv.first.c_str());
        return 0;
    }
    if(argc != 2 || !all.count(argv[1])) {
        fprintf(stderr, "usage: %s --list | <test>\n", argv[0]);
        return 2;
    }
    return all[argv[1]]() ? 0 : 1;
}
