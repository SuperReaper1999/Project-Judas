#include "BrowserPlatform.h"
#include <atomic>
#include <emscripten.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>

namespace {
EM_JS(void, BeginSync, (int* result), {
    const finish = (() => {
        let settled = false;
        return value => {
            if (settled) return;
            settled = true;
            Atomics.store(HEAP32, result >> 2, value);
            Atomics.notify(HEAP32, result >> 2);
        };
    })();
    if (!Module.judasStorageReady || Module.judasStorageFault) {
        finish(-1);
        return;
    }
    const timer = setTimeout(() => {
        // Fail closed: a late transaction must not race a newer save. Reload is
        // required after an uncertain timeout. The settled guard prevents any
        // later callback from accessing the caller's released stack memory.
        Module.judasStorageFault = true;
        finish(-2);
    }, 15000);
    try {
        FS.syncfs(false, error => {
            clearTimeout(timer);
            if (error) Module.judasStorageFault = true;
            if (!error) Module.judasSaveFlushes = (Module.judasSaveFlushes || 0) + 1;
            finish(error ? -1 : 1);
        });
    } catch (error) {
        clearTimeout(timer);
        Module.judasStorageFault = true;
        finish(-1);
    }
});
void StartSync(void* result) { BeginSync(static_cast<int*>(result)); }
}

bool FlushBrowserStorage(std::string& error) {
    if(emscripten_is_main_runtime_thread()) {
        error="Browser persistence must complete on a save worker, not block the browser UI";
        return false;
    }
    static_assert(sizeof(std::atomic<int>)==sizeof(int),"Shared browser completion word");
    std::atomic<int> result{0};
    if(!emscripten_proxy_sync(emscripten_proxy_get_system_queue(),
                             emscripten_main_runtime_thread_id(),StartSync,&result)) {
        error="Cannot schedule browser persistent storage synchronization";
        return false;
    }
    while(result.load(std::memory_order_acquire)==0)
        emscripten_futex_wait(&result,0,100);
    if(result.load()!=1) {
        error="Save publication failed or timed out in browser persistent storage; reload before retrying";
        return false;
    }
    return true;
}
