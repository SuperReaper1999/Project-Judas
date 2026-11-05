#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace JudasRender {
struct StreamSlice {
    unsigned buffer = 0;
    std::size_t offset = 0;
};
// Context-owned upload storage. Each persistent page is fenced after its last
// consumer before reuse. Portable uploads use driver orphaning; callers retain
// identical draw order and never retain a mapped pointer. Valid uploads acquire
// the current context/thread; subsequent use and explicit Shutdown must stay on
// that owner. ARRAY_BUFFER is left bound to the returned slice's buffer.
class StreamArena {
  public:
    StreamArena();
    ~StreamArena();
    StreamArena(const StreamArena&) = delete;
    StreamArena& operator=(const StreamArena&) = delete;
    // Optional total live upload demand (including slice alignment) for a pass.
    // CPU-only, bounded by PageBytes; zero restores sizing from individual
    // uploads. Storage is allocated/grown lazily and is never shrunk here.
    void SetWorkingSetHint(std::size_t bytes);
    bool Upload(const void* data, std::size_t bytes, bool persistent, StreamSlice&, std::string& error);
    void Shutdown();
    std::size_t AllocatedBytes() const;
    std::uint64_t WaitNanoseconds() const;
    static constexpr std::size_t PageBytes = 4 * 1024 * 1024;
  private:
    struct Storage;
    std::unique_ptr<Storage> storage;
};
} // namespace JudasRender
