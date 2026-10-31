#pragma once
#include <string>

// A browser save is not durable merely because MEMFS accepted a write. This
// boundary must complete IndexedDB synchronization before publishing success.
bool FlushBrowserStorage(std::string& error);
