// OhShark NAPI: exposes the cross-compiled upstream Wireshark engine (tshark's
// main() inside libohsharkcore.so) to the ArkTS UI.
//
// HarmonyOS blocks execve() for app payloads, so tshark cannot be spawned as a
// process; instead libohsharkcore.so is dlopen'ed from the signed bundle and
// each run happens in a fork()ed child (upstream main() is not re-entrant, so
// one pristine child per run). The child writes tshark's stdout/stderr to files
// in the app sandbox; the parent collects them and resolves the Promise.
// A mutex serializes runs because they share one pair of output files.

#include "napi/native_api.h"

#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "sample_pcap.h"

using RunTsharkFn = int (*)(int, char **, const char *, const char *);
using ListIfacesFn = int (*)(const char *);
using VersionFn = const char *(*)();

static void *g_handle = nullptr;
static RunTsharkFn g_run = nullptr;
static ListIfacesFn g_ifaces = nullptr;
static VersionFn g_version = nullptr;
static std::mutex g_runMtx;

static const char *kOutPath = "/data/storage/el2/base/files/ohshark_run.out";
static const char *kErrPath = "/data/storage/el2/base/files/ohshark_run.err";

static bool loadEngine(std::string &error)
{
    if (g_run && g_version)
        return true;
    const char *candidates[] = {
        "/data/storage/el1/bundle/libs/arm64/libohsharkcore.so",
        "/data/storage/el1/bundle/libs/arm64-v8a/libohsharkcore.so",
        "libohsharkcore.so",
    };
    for (const char *c : candidates) {
        g_handle = dlopen(c, RTLD_NOW | RTLD_LOCAL);
        if (!g_handle)
            continue;
        g_run = reinterpret_cast<RunTsharkFn>(dlsym(g_handle, "ohshark_run_tshark"));
        g_ifaces = reinterpret_cast<ListIfacesFn>(dlsym(g_handle, "ohshark_list_interfaces"));
        g_version = reinterpret_cast<VersionFn>(dlsym(g_handle, "ohshark_engine_version"));
        if (g_run && g_version)
            return true;
    }
    const char *dl = dlerror();
    error = dl ? std::string(dl) : std::string("libohsharkcore.so not found");
    return false;
}

struct RunOutcome {
    int rc = -1;
    std::string out;
    std::string err;
};

static std::string slurp(const char *path)
{
    std::string data;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return data;
    char buf[65536];
    ssize_t n = 0;
    while ((n = read(fd, buf, sizeof buf)) > 0)
        data.append(buf, size_t(n));
    close(fd);
    return data;
}

static RunOutcome runEngine(const std::vector<std::string> &args, bool listIfaces)
{
    std::lock_guard<std::mutex> lock(g_runMtx);
    RunOutcome r;
    std::string loadErr;
    if (!loadEngine(loadErr)) {
        r.err = "engine load failed: " + loadErr;
        return r;
    }
    unlink(kOutPath);
    unlink(kErrPath);

    std::vector<char *> argv;
    argv.push_back(const_cast<char *>("tshark"));
    for (const auto &a : args)
        argv.push_back(const_cast<char *>(a.c_str()));
    argv.push_back(nullptr);

    const pid_t pid = fork();
    if (pid < 0) {
        r.err = "fork failed";
        return r;
    }
    if (pid == 0) {
        int rc = -1;
        if (listIfaces && g_ifaces) {
            rc = g_ifaces(kOutPath);
        } else if (g_run) {
            rc = g_run(int(argv.size() - 1), argv.data(), kOutPath, kErrPath);
        }
        _exit(rc < 0 ? 200 : (rc > 199 ? 199 : rc));
    }
    int status = 0;
    bool done = false;
    for (int i = 0; i < 900; ++i) { // 90 s watchdog
        if (waitpid(pid, &status, WNOHANG) == pid) {
            done = true;
            break;
        }
        usleep(100000);
    }
    if (!done) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        r.err = "engine run timed out and was killed";
        return r;
    }
    r.rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    r.out = slurp(kOutPath);
    r.err += slurp(kErrPath);
    return r;
}

// ---- Promise plumbing -------------------------------------------------------

struct RunRequest {
    std::vector<std::string> args;
    bool listIfaces = false;
    napi_deferred deferred = nullptr;
    napi_threadsafe_function tsfn = nullptr;
};

static napi_value DummyJsFn(napi_env env, napi_callback_info info)
{
    (void)env;
    (void)info;
    return nullptr;
}

static void CallJsDone(napi_env env, napi_value jsFunc, void *context, void *data)
{
    (void)jsFunc;
    RunRequest *req = static_cast<RunRequest *>(context);
    RunOutcome *r = static_cast<RunOutcome *>(data);
    if (req == nullptr)
        return;

    napi_value undefined = nullptr;
    napi_get_undefined(env, &undefined);
    if (r != nullptr) {
        napi_value obj = nullptr;
        napi_create_object(env, &obj);
        napi_value v = nullptr;
        napi_create_int32(env, r->rc, &v);
        napi_set_named_property(env, obj, "rc", v);
        napi_create_string_utf8(env, r->out.c_str(), r->out.size(), &v);
        napi_set_named_property(env, obj, "out", v);
        napi_create_string_utf8(env, r->err.c_str(), r->err.size(), &v);
        napi_set_named_property(env, obj, "err", v);
        napi_resolve_deferred(env, req->deferred, obj);
        delete r;
    } else {
        napi_value errStr = nullptr;
        napi_create_string_utf8(env, "engine run produced no result", NAPI_AUTO_LENGTH, &errStr);
        napi_value errObj = nullptr;
        napi_create_error(env, nullptr, errStr, &errObj);
        napi_reject_deferred(env, req->deferred, errObj);
    }
    if (req->tsfn != nullptr)
        napi_release_threadsafe_function(req->tsfn, napi_tsfn_release);
    delete req;
}

static napi_value StartRun(napi_env env, napi_callback_info info, bool listIfaces)
{
    size_t argc = 1;
    napi_value argv[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    auto *req = new RunRequest();
    req->listIfaces = listIfaces;
    if (argc >= 1) {
        bool isArray = false;
        napi_is_array(env, argv[0], &isArray);
        if (!isArray) {
            delete req;
            napi_throw_type_error(env, nullptr, "argument must be an array of strings");
            return nullptr;
        }
        uint32_t len = 0;
        napi_get_array_length(env, argv[0], &len);
        for (uint32_t i = 0; i < len; ++i) {
            napi_value el = nullptr;
            napi_get_element(env, argv[0], i, &el);
            size_t slen = 0;
            if (napi_get_value_string_utf8(env, el, nullptr, 0, &slen) != napi_ok)
                continue;
            std::string s(slen, '\0');
            size_t written = 0;
            napi_get_value_string_utf8(env, el, &s[0], slen + 1, &written);
            req->args.push_back(s);
        }
    }

    napi_value promise = nullptr;
    napi_create_promise(env, &req->deferred, &promise);

    napi_value jsCb = nullptr;
    napi_create_function(env, nullptr, 0, DummyJsFn, nullptr, &jsCb);
    napi_value resourceName = nullptr;
    napi_create_string_utf8(env, "ohsharkRun", NAPI_AUTO_LENGTH, &resourceName);
    napi_create_threadsafe_function(env, jsCb, nullptr, resourceName, 0, 1, nullptr,
                                    nullptr, req, CallJsDone, &req->tsfn);

    std::thread([req]() {
        auto *r = new RunOutcome(runEngine(req->args, req->listIfaces));
        napi_call_threadsafe_function(req->tsfn, r, napi_tsfn_blocking);
    }).detach();
    return promise;
}

static napi_value RunTshark(napi_env env, napi_callback_info info)
{
    return StartRun(env, info, false);
}

static napi_value Interfaces(napi_env env, napi_callback_info info)
{
    return StartRun(env, info, true);
}

static napi_value Version(napi_env env, napi_callback_info info)
{
    (void)info;
    std::string err;
    napi_value v = nullptr;
    if (loadEngine(err)) {
        napi_create_string_utf8(env, g_version(), NAPI_AUTO_LENGTH, &v);
    } else {
        std::string msg = "not loaded: " + err;
        napi_create_string_utf8(env, msg.c_str(), msg.size(), &v);
    }
    return v;
}

// Writes the embedded capture (real traffic taken on this device by the ported
// dumpcap) into the app sandbox so the engine has a file to dissect.
static napi_value WriteSample(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value argv[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
    if (argc < 1) {
        napi_throw_type_error(env, nullptr, "destination path expected");
        return nullptr;
    }
    size_t slen = 0;
    napi_get_value_string_utf8(env, argv[0], nullptr, 0, &slen);
    std::string path(slen, '\0');
    napi_get_value_string_utf8(env, argv[0], &path[0], slen + 1, &slen);

    int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        napi_value result = nullptr;
        napi_get_boolean(env, false, &result);
        return result;
    }
    const ssize_t written = write(fd, kSamplePcap, kSamplePcapLen);
    close(fd);
    napi_value result = nullptr;
    napi_get_boolean(env, written == static_cast<ssize_t>(kSamplePcapLen), &result);
    return result;
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports)
{
    static napi_property_descriptor desc[] = {
        { "runTshark", nullptr, RunTshark, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "interfaces", nullptr, Interfaces, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "version", nullptr, Version, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "writeSample", nullptr, WriteSample, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    return exports;
}
EXTERN_C_END

static napi_module g_module = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "ohsharkapi",
    .nm_priv = ((void *)0),
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void OhSharkApiRegister(void)
{
    napi_module_register(&g_module);
}
