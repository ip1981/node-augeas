/*
 * Copyright (C) 2012, Nexenta Systems, Inc.
 *
 * The contents of this file are subject to the terms of
 * the Common Development and Distribution License ("CDDL").
 * You may not use this file except in compliance with this license.
 *
 * You can obtain a copy of the License at
 * http://www.opensource.org/licenses/CDDL-1.0
 */

#include <string>

#define BUILDING_NODE_EXTENSION

// node.h includes v8.h
#include <node.h>

extern "C" { // Yes, that bad
#include <augeas.h>
}

using namespace v8;


inline std::string aug_error_msg(augeas *aug)
{
    std::string msg = aug_error_message(aug);
    const char *minor = aug_error_minor_message(aug);
    const char *details = aug_error_details(aug);
    if (NULL != minor) {
        msg += " - ";
        msg += minor;
    }
    if (NULL != details) {
        msg += ": ";
        msg += details;
    }
    return msg;
}

inline void throw_aug_error_msg(augeas *aug)
{
    if (AUG_NOERROR != aug_error(aug)) {
        ThrowException(Exception::Error(String::New(aug_error_msg(aug).c_str())));
    } else {
        ThrowException(Exception::Error(
                           String::New(
                               "An error has occured from Augeas API call, but no description available")));
    }
}


/*
 * Helper function.
 * Converts value of object member *key into std::string.
 * Returns empty string if memder does not exist.
 */
inline std::string memberToString(Handle<Object> obj, const char *key)
{
    Local<Value> m = obj->Get(Local<String>(String::New(key)));
    if (!m->IsUndefined()) {
        String::Utf8Value str(m);
        return std::string(*str);
    } else {
        return std::string();
    }
}

/*
 * Helper function.
 * Converts value of object member *key into uint32.
 * Returns 0 if memder does not exist.
 */
inline uint32_t memberToUint32(Handle<Object> obj, const char *key)
{
    Local<Value> m = obj->Get(Local<String>(String::New(key)));
    if (!m->IsUndefined()) {
        return m->Uint32Value();
    } else {
        return 0;
    }
}

/*
 * Helper function.
 * Joins JS array by new line.
 */
inline std::string join(Local<Array> a)
{
    std::string res;
    uint32_t len = a->Length();
    if (len > 0) {
        for (uint32_t i = 0; i < len - 1; ++i) {
            String::Utf8Value v(a->Get(i));
            res.append(*v);
            res.append("\n");
        }
        String::Utf8Value v(a->Get(len-1));
        res.append(*v);
    }
    return res;
}

class LibAugeas : public node::ObjectWrap {
public:
    static void Init(Handle<Object> target);
    static Local<Object> New(augeas *aug);

protected:
    augeas * m_aug;
    LibAugeas();
    ~LibAugeas();

    static Persistent<FunctionTemplate> augeasTemplate;
    static Persistent<Function> constructor;

    static Handle<Value> defvar     (const Arguments& args);
    static Handle<Value> defnode    (const Arguments& args);
    static Handle<Value> get        (const Arguments& args);
    static Handle<Value> set        (const Arguments& args);
    static Handle<Value> setm       (const Arguments& args);
    static Handle<Value> rm         (const Arguments& args);
    static Handle<Value> mv         (const Arguments& args);
    static Handle<Value> save       (const Arguments& args);
    static Handle<Value> nmatch     (const Arguments& args);
    static Handle<Value> match      (const Arguments& args);
    static Handle<Value> load       (const Arguments& args);
    static Handle<Value> srun       (const Arguments& args);
    static Handle<Value> insertAfter  (const Arguments& args);
    static Handle<Value> insertBefore (const Arguments& args);
    static Handle<Value> error      (const Arguments& args);
    static Handle<Value> errorMsg   (const Arguments& args);
};

Persistent<FunctionTemplate> LibAugeas::augeasTemplate;
Persistent<Function> LibAugeas::constructor;

void LibAugeas::Init(Handle<Object> target)
{
    // flags for aug_init():
    NODE_DEFINE_CONSTANT(target, AUG_NONE);
    NODE_DEFINE_CONSTANT(target, AUG_SAVE_BACKUP);
    NODE_DEFINE_CONSTANT(target, AUG_SAVE_NEWFILE);
    NODE_DEFINE_CONSTANT(target, AUG_TYPE_CHECK);
    NODE_DEFINE_CONSTANT(target, AUG_NO_STDINC);
    NODE_DEFINE_CONSTANT(target, AUG_SAVE_NOOP);
    NODE_DEFINE_CONSTANT(target, AUG_NO_LOAD);
    NODE_DEFINE_CONSTANT(target, AUG_NO_MODL_AUTOLOAD);
    NODE_DEFINE_CONSTANT(target, AUG_ENABLE_SPAN);
    NODE_DEFINE_CONSTANT(target, AUG_NO_ERR_CLOSE);

    // error codes:
    NODE_DEFINE_CONSTANT(target, AUG_NOERROR);
    NODE_DEFINE_CONSTANT(target, AUG_ENOMEM);
    NODE_DEFINE_CONSTANT(target, AUG_EINTERNAL);
    NODE_DEFINE_CONSTANT(target, AUG_EPATHX);
    NODE_DEFINE_CONSTANT(target, AUG_ENOMATCH);
    NODE_DEFINE_CONSTANT(target, AUG_EMMATCH);
    NODE_DEFINE_CONSTANT(target, AUG_ESYNTAX);
    NODE_DEFINE_CONSTANT(target, AUG_ENOLENS);
    NODE_DEFINE_CONSTANT(target, AUG_EMXFM);
    NODE_DEFINE_CONSTANT(target, AUG_ENOSPAN);
    NODE_DEFINE_CONSTANT(target, AUG_EMVDESC);
    NODE_DEFINE_CONSTANT(target, AUG_ECMDRUN);
    NODE_DEFINE_CONSTANT(target, AUG_EBADARG);

    augeasTemplate = Persistent<FunctionTemplate>::New(FunctionTemplate::New());
    augeasTemplate->SetClassName(String::NewSymbol("Augeas"));
    augeasTemplate->InstanceTemplate()->SetInternalFieldCount(1);

// I do not want copy-n-paste errors here:
#define _NEW_METHOD(m) NODE_SET_PROTOTYPE_METHOD(augeasTemplate, #m, m)
    _NEW_METHOD(defvar);
    _NEW_METHOD(defnode);
    _NEW_METHOD(get);
    _NEW_METHOD(set);
    _NEW_METHOD(setm);
    _NEW_METHOD(rm);
    _NEW_METHOD(mv);
    _NEW_METHOD(save);
    _NEW_METHOD(nmatch);
    _NEW_METHOD(match);
    _NEW_METHOD(load);
    _NEW_METHOD(srun);
    _NEW_METHOD(insertAfter);
    _NEW_METHOD(insertBefore);
    _NEW_METHOD(error);
    _NEW_METHOD(errorMsg);


    constructor = Persistent<Function>::New(augeasTemplate->GetFunction());
}

/*
 * Creates an JS object to pass it in callback function.
 * This JS objects wraps an LibAugeas object.
 * Only for using within this C++ code.
 */
Local<Object> LibAugeas::New(augeas *aug)
{
    LibAugeas *obj = new LibAugeas();
    obj->m_aug = aug;
    Local<Object> O = augeasTemplate->InstanceTemplate()->NewInstance();
    obj->Wrap(O);
    return O;
}


/*
 * Wrapper of aug_defvar() - define a variable
 * The second argument is optional and if ommited,
 * variable will be removed if exists.
 *
 * On error, throws an exception and returns undefined;
 * on success, returns 0 if expr evaluates to anything
 * other than a nodeset, and the number of nodes if expr
 * evaluates to a nodeset
 */
Handle<Value> LibAugeas::defvar(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() < 1 || args.Length() > 2) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value n_str(args[0]);
    String::Utf8Value e_str(args[1]);

    const char *name = *n_str;
    const char *expr = *e_str;

    /* Returns -1 on error; on success, returns 0 if EXPR evaluates to anything
     * other than a nodeset, and the number of nodes if EXPR evaluates to a nodeset
     */
    int rc = aug_defvar(obj->m_aug, name, args[1]->IsUndefined() ? NULL : expr);
    if (-1 == rc) {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    } else {
        return scope.Close(Number::New(rc));
    }
}

/*
 * Wrapper of aug_defnode() - define a node.
 * The defnode command is very useful when you add a node
 * that you need to modify further, e. g. by adding children to it.
 *
 * On error, throws an exception and returns undefined;
 * on success, returns the number of nodes in the nodeset (>= 0)
 *
 * Arguments:
 * name - required
 * expr - required
 * value - optional
 * callback - optional
 *
 * The last argument could be a function. It will be called (synchronously)
 * with one argument set to True if a node was created, and False if it already existed.
 */
Handle<Value> LibAugeas::defnode(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() < 2 || args.Length() > 4) {
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value n_str(args[0]);
    String::Utf8Value e_str(args[1]);
    String::Utf8Value v_str(args[2]);

    const char *name = *n_str;
    const char *expr = *e_str;
    const char *value = NULL;
    if (!args[2]->IsUndefined() && !args[2]->IsFunction()) {
        value = *v_str;
    }
    int created;

    /* Returns -1 on error; on success, returns
     * the number of nodes in the nodeset, set created=1 if node created,
     * set created=0 if node already existed.
     */
    int rc = aug_defnode(obj->m_aug, name, expr, value, &created);
    if (-1 == rc) {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    } else {
        int last = args.Length() - 1;
        if (args[last]->IsFunction()) {
            Local<Function> cb =  Local<Function>::Cast(args[last]);

            Local<Value> argv[] = {Local<Boolean>::New(Boolean::New(created == 1))};

            TryCatch try_catch;
            cb->Call(Context::GetCurrent()->Global(), 1, argv);
            if (try_catch.HasCaught()) {
                node::FatalException(try_catch);
            }
        }
        return scope.Close(Number::New(rc));
    }
}

/*
 * Wrapper of aug_get() - get exactly one value
 */
Handle<Value> LibAugeas::get(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly one argument")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);

    const char *path = *p_str; // operator*() returns C-string
    const char *value;

    /*
     * Return 1 if there is exactly one node matching PATH,
     * 0 if there is none, and a negative value
     * if there is more than one node matching PATH,
     * or if PATH is not a legal path expression.
     *
     * The string *value must not be freed by the caller,
     * and is valid as long as its node remains unchanged.
     */
    int rc = aug_get(obj->m_aug, path, &value);
    if (1 == rc) {
        if (NULL != value) {
            return scope.Close(String::New(value));
        } else {
            return scope.Close(Null());
        }
    } else if (0 == rc) {
        return scope.Close(Undefined());
    } else if (rc < 0) {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    } else {
        ThrowException(Exception::Error(String::New("Unexpected return value of aug_get()")));
        return scope.Close(Undefined());
    }
}

/*
 * Wrapper of aug_set() - set exactly one value
 * Note: this method (as aug_set) does not write any files,
 *       it just changes internal tree. To write files use LibAugeas::save()
 */
Handle<Value> LibAugeas::set(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 2) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly two arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);
    String::Utf8Value v_str(args[1]);

    const char *path  = *p_str;
    const char *value = *v_str;

    /*
     * 0 on success, -1 on error. It is an error
     * if more than one node matches path.
     */
    int rc = aug_set(obj->m_aug, path, value);
    if (AUG_NOERROR != rc) {
        throw_aug_error_msg(obj->m_aug);
    }
    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_setm() - set the value of multiple nodes in one operation
 * Returns the number of modified nodes on success.
 */
Handle<Value> LibAugeas::setm(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 3) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly three arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value b_str(args[0]);
    String::Utf8Value s_str(args[1]);
    String::Utf8Value v_str(args[2]);

    const char *base  = *b_str;
    const char *sub   = *s_str;
    const char *value = *v_str;


    int rc = aug_setm(obj->m_aug, base, sub, value);
    if (rc >= 0) {
        return scope.Close(Int32::New(rc));
    } else {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    }
}


/*
 * Wrapper of aug_rm() - remove nodes
 * Remove path and all its children. Returns the number of entries removed.
 * All nodes that match PATH, and their descendants, are removed.
 */
Handle<Value> LibAugeas::rm(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly one argument")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);

    const char *path  = *p_str;

    int rc = aug_rm(obj->m_aug, path);
    if (rc >= 0) {
        return scope.Close(Number::New(rc));
    } else {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    }

}

/*
 * Wrapper of aug_mv() - move nodes
 */
Handle<Value> LibAugeas::mv(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 2) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly two arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value src(args[0]);
    String::Utf8Value dst(args[1]);

    const char *source = *src;
    const char *dest   = *dst;

    /*
     * 0 on success, -1 on error. It is an error
     * if more than one node matches path.
     */
    int rc = aug_mv(obj->m_aug, source, dest);
    if (AUG_NOERROR != rc) {
        throw_aug_error_msg(obj->m_aug);
    }
    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_insert(aug, path, label, 0) - insert 'label' after 'path'
 */
Handle<Value> LibAugeas::insertAfter(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 2) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly two arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);
    String::Utf8Value l_str(args[1]);

    const char *path  = *p_str;
    const char *label = *l_str;

    int rc = aug_insert(obj->m_aug, path, label, 0);
    if (AUG_NOERROR != rc) {
        throw_aug_error_msg(obj->m_aug);
    }
    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_insert(aug, path, label, 1) - insert 'label' before 'path'
 */
Handle<Value> LibAugeas::insertBefore(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 2) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly two arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);
    String::Utf8Value l_str(args[1]);

    const char *path  = *p_str;
    const char *label = *l_str;

    int rc = aug_insert(obj->m_aug, path, label, 1);
    if (AUG_NOERROR != rc) {
        throw_aug_error_msg(obj->m_aug);
    }
    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_error()
 * Returns the error code from the last API call
 */
Handle<Value> LibAugeas::error(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 0) {
        ThrowException(Exception::TypeError(String::New("Function does not accept arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());

    int rc = aug_error(obj->m_aug);
    return scope.Close(Int32::New(rc));
}

/*
 * Returns the error message from the last API call,
 * including all details.
 */
Handle<Value> LibAugeas::errorMsg(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 0) {
        ThrowException(Exception::TypeError(String::New("Function does not accept arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());

    return scope.Close(String::New(aug_error_msg(obj->m_aug).c_str()));
}



struct SaveUV {
    uv_work_t request;
    Persistent<Function> callback;
    augeas *aug;
    int rc; // = aug_save(), 0 on success, -1 on error
};

void saveWork(uv_work_t *req)
{
    SaveUV *suv = static_cast<SaveUV*>(req->data);
    suv->rc = aug_save(suv->aug);
}

/*
 * Execute JS callback after saveWork() terminated
 */
void saveAfter(uv_work_t* req)
{
    HandleScope scope;

    SaveUV *suv = static_cast<SaveUV*>(req->data);
    Local<Value> argv[] = {Int32::New(suv->rc)};

    TryCatch try_catch;
    suv->callback->Call(Context::GetCurrent()->Global(), 1, argv);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    suv->callback.Dispose();
    delete suv;
}

/*
 * Wrapper of aug_save() - save changed files.
 *
 * Without arguments this function performs blocking saving
 * and throws an exception on error.
 *
 * The only argument allowed is a callback function.
 * If such an argument is given this function performs
 * non-blocking (async) saving, and after saving is done (or failed)
 * executes the callback with one integer argument - return value of aug_save(),
 * i. e. 0 on success, -1 on failure.
 *
 * NOTE: multiple async calls of this function (from the same augeas object)
 * will result in crash (double free or segfault) because of using
 * shared augeas handle.
 *
 * Always returns undefined.
 */
Handle<Value> LibAugeas::save(const Arguments& args)
{
    HandleScope scope;

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());

    // if no args, save files synchronously (blocking):
    if (args.Length() == 0) {
        int rc = aug_save(obj->m_aug);
        if (AUG_NOERROR != rc) {
            ThrowException(Exception::Error(String::New("Failed to write files")));
        }
        // single argument is a function - async:
    } else if ((args.Length() == 1) && args[0]->IsFunction()) {
        SaveUV * suv = new SaveUV();
        suv->request.data = suv;
        suv->aug = obj->m_aug;
        suv->callback = Persistent<Function>::New(
                            Local<Function>::Cast(args[0]));
        uv_queue_work(uv_default_loop(), &suv->request, saveWork, saveAfter);
    } else {
        ThrowException(Exception::Error(String::New("Callback function or nothing")));
    }

    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_match(aug, path, NULL) - count all nodes matching path expression
 * Returns the number of found nodes.
 * Note: aug_match() allocates memory if the third argument is not NULL,
 * in this function we always set it to NULL and get only number of found nodes.
 */
Handle<Value> LibAugeas::nmatch(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly one argument")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);

    const char *path  = *p_str;

    int rc = aug_match(obj->m_aug, path, NULL);
    if (rc >= 0) {
        return scope.Close(Number::New(rc));
    } else {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    }
}

/*
 * Wrapper of aug_match(, , non-NULL).
 * Returns an array of nodes matching given path expression
 */
Handle<Value> LibAugeas::match(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly one argument")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());
    String::Utf8Value p_str(args[0]);

    const char *path  = *p_str;
    char **matches = NULL;

    int rc = aug_match(obj->m_aug, path, &matches);
    if (rc >= 0) {
        Local<Array> result = Array::New(rc);
        if (NULL != matches) {
            for (int i = 0; i < rc; ++i) {
                result->Set(Number::New(i), String::New(matches[i]));
                free(matches[i]);
            }
            free(matches);
        }
        return scope.Close(result);
    } else {
        throw_aug_error_msg(obj->m_aug);
        return scope.Close(Undefined());
    }
}

/*
 * Wrapper of aug_load() - load /files
 */
Handle<Value> LibAugeas::load(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 0) {
        ThrowException(Exception::TypeError(String::New("Function does not accept arguments")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());

    /*
     * aug_load() returns -1 on error, 0 on success. Success includes the case
     * where some files could not be loaded. Details of such files can be found
     * as '/augeas//error'.
     */
    int rc = aug_load(obj->m_aug);
    if (AUG_NOERROR != rc) {
        ThrowException(Exception::Error(String::New("Failed to load files")));
    }

    return scope.Close(Undefined());
}

/*
 * Wrapper of aug_srun() - run augeas commands (like augtool does)
 * Returns the number of executed commands.
 * Throws expression on error or if the 'quit' command encountered.
 * Arguments:
 * string or array of strings
 */
Handle<Value> LibAugeas::srun(const Arguments& args)
{
    HandleScope scope;

    if (args.Length() != 1) {
        ThrowException(Exception::TypeError(String::New("Function accepts exactly one argument")));
        return scope.Close(Undefined());
    }

    LibAugeas *obj = ObjectWrap::Unwrap<LibAugeas>(args.This());

    std::string text;

    if (args[0]->IsArray()) {
        text = join(Local<Array>::Cast(args[0]));
    } else {
        String::Utf8Value t_str(args[0]);
        text = *t_str;
    }

    /*
     * Returns the number of executed commands on success,
     * -1 on failure, and -2 if a 'quit' command was encountered.
     * TODO: use output (the second argument to aug_srun() != NULL)
     */
    int rc = aug_srun(obj->m_aug, NULL, text.c_str());
    if (rc >= 0) {
        return scope.Close(Number::New(rc));
    } else if (-1 == rc) {
        throw_aug_error_msg(obj->m_aug);
    } else if (-2 == rc) {
        ThrowException(Exception::Error(String::New("'quit' command was encountered")));
    } else {
        ThrowException(Exception::Error(String::New("Unexpected return code from aug_srun()")));
    }
}


LibAugeas::LibAugeas() : m_aug(NULL)
{
}

LibAugeas::~LibAugeas()
{
    aug_close(m_aug);
}


struct CreateAugeasUV {
    uv_work_t request;
    Persistent<Function> callback;
    std::string root;
    std::string loadpath;
    std::string lens;
    std::string incl;
    std::string excl;
    std::string srun;
    unsigned int flags;
    augeas *aug;
};


/*
 * This function should immediately return if any call to augeas API fails.
 * The caller should check aug_error() before doing anything.
 */
void createAugeasWork(uv_work_t *req)
{
    int rc = AUG_NOERROR;

    CreateAugeasUV *her = static_cast<CreateAugeasUV*>(req->data);

    // do not load all lenses if a specific lens is given,
    // ignore any setting in flags.
    // XXX: AUG_NO_MODL_AUTOLOAD implies AUG_NO_LOAD
    if (!her->lens.empty()) {
        her->flags |= AUG_NO_MODL_AUTOLOAD;
    }

    her->aug = aug_init(her->root.c_str(), her->loadpath.c_str(), her->flags);
    rc = aug_error(her->aug);
    if (AUG_NOERROR != rc)
        return;

    /*
     * Consider lens/incl/excl interface obsolete.
     * With srun: respect all flags (AUG_NO_MODL_AUTOLOAD, AUG_NO_LOAD),
     * execute srun commands and return.
     */
    if (!her->srun.empty()) {
        rc = aug_srun(her->aug, NULL, her->srun.c_str());
        return;
    }

    if (!her->lens.empty()) {
        // specifying which lens to load
        // /augeas/load/<random-name>/lens = e. g.: "hosts.lns" or "@Hosts_Access"
        std::string basePath = "/augeas/load/1"; // "1" is a random valid name :-)
        std::string lensPath = basePath + "/lens";
        std::string lensVal  = her->lens;
        if ((lensVal[0] != '@') // if not a module
                && (lensVal.rfind(".lns") == std::string::npos))
        {
            lensVal += ".lns";
        }
        rc = aug_set(her->aug, lensPath.c_str(), lensVal.c_str());
        if (AUG_NOERROR != rc)
            return;

        if (!her->incl.empty()) {
            // specifying which files to load :
            // /augeas/load/<random-name>/incl = glob
            std::string inclPath = basePath + "/incl";
            rc = aug_set(her->aug, inclPath.c_str(), her->incl.c_str());
            if (AUG_NOERROR != rc)
                return;
        }
        if (!her->excl.empty()) {
            // specifying which files NOT to load
            // /augeas/load/<random-name>/excl = glob (e. g. "*.dpkg-new")
            std::string exclPath = basePath + "/excl";
            rc = aug_set(her->aug, exclPath.c_str(), her->excl.c_str());
            if (AUG_NOERROR != rc)
                return;
        }

        rc = aug_load(her->aug);
        if (AUG_NOERROR != rc)
            return;
    }
}

void createAugeasAfter(uv_work_t* req)
{
    HandleScope scope;

    CreateAugeasUV *her = static_cast<CreateAugeasUV*>(req->data);
    Local<Value> argv[] = {LibAugeas::New(her->aug)};

    TryCatch try_catch;
    her->callback->Call(Context::GetCurrent()->Global(), 1, argv);
    if (try_catch.HasCaught()) {
        node::FatalException(try_catch);
    }

    her->callback.Dispose();
    delete her;
}

/*
 * Creates an Augeas object from JS side either in sync or async way
 * depending on the last argument:
 *
 * augeas.createAugeas([...], function(aug) {...}) - async
 *
 * or:
 *
 * var aug = augeas.createAugeas([...]) - sync
 *
 */
Handle<Value> createAugeas(const Arguments& args)
{
    HandleScope scope;

    // options for aug_init(root, loadpath, flags):
    std::string root;
    std::string loadpath;
    unsigned int flags;

    // Allow passing options as an JS object:
    if (args[0]->IsObject()) {
        Local<Object> obj = args[0]->ToObject();
        root     = memberToString(obj, "root");
        loadpath = memberToString(obj, "loadpath");
        flags    = memberToUint32(obj, "flags");
    } else {
        // C-like way:
        if (args[0]->IsString()) {
            String::Utf8Value p_str(args[0]);
            root = *p_str;
        }
        if (args[1]->IsString()) {
            String::Utf8Value l_str(args[1]);
            loadpath = *l_str;
        }
        if (args[2]->IsNumber()) {
            flags = args[2]->Uint32Value();
        }
    }

    // always set to be able to get error messages, if aug_init() failed:
    flags |= AUG_NO_ERR_CLOSE;

    /*
     * If the last argument is a function, create augeas
     * in an async way, and then pass it to that function.
     */
    bool async = (args.Length() > 0) && (args[args.Length()-1]->IsFunction());

    if (async) {
        CreateAugeasUV *her = new CreateAugeasUV();
        her->request.data = her;
        her->callback = Persistent<Function>::New(
                            Local<Function>::Cast(args[args.Length()-1]));

        her->root = root;
        her->loadpath = loadpath;
        her->flags = flags;

        // Extra options for async mode:
        if (args[0]->IsObject()) {
            Local<Object> obj = args[0]->ToObject();
            her->lens = memberToString(obj, "lens");
            her->incl = memberToString(obj, "incl");
            her->excl = memberToString(obj, "excl");

            Local<Value> srun = obj->Get(Local<String>(String::New("srun")));
            if (srun->IsArray()) {
                her->srun = join(Local<Array>::Cast(srun));
            } else {
                her->srun = memberToString(obj, "srun");
            }
        }

        uv_queue_work(uv_default_loop(), &her->request,
                      createAugeasWork, createAugeasAfter);

        return scope.Close(Undefined());
    } else { // sync

        augeas *aug = aug_init(root.c_str(), loadpath.c_str(), flags);

        if (NULL == aug) { // should not happen due to AUG_NO_ERR_CLOSE
            ThrowException(Exception::Error(
                               String::New("aug_init() badly failed: it should not return NULL, but it did.")));
            return scope.Close(Undefined());
        } else if (AUG_NOERROR != aug_error(aug)) {
            throw_aug_error_msg(aug);
            aug_close(aug);
            return scope.Close(Undefined());
        }

        return scope.Close(LibAugeas::New(aug));
    }
}



void init(Handle<Object> target)
{
    LibAugeas::Init(target);

    target->Set(String::NewSymbol("createAugeas"),
                FunctionTemplate::New(createAugeas)->GetFunction());
}

NODE_MODULE(libaugeas, init)

