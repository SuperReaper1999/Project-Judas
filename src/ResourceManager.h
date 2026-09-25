#pragma once

#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "AssetDatabase.h"
#include "JobSystem.h"
#include "MeshData.h"
#include "Renderer.h"
#include "TextureData.h"

class RadialTerrain;

// Milestone 30/31: the one engine boundary that turns project assets into
// runtime resources.
//
//   AssetId -> AssetDatabase (where the file is) -> ResourceManager -> Renderer resource
//
// Milestone 31 makes the middle asynchronous. Per asset id:
//
//   Unloaded
//     -> Queued        a load job is submitted (Request*, or Get* in async mode)
//     -> Loading       a worker is reading the file and decoding it
//                      (ReadWholeFile + ParseObjMesh / DecodeTextureFromMemory:
//                      CPU work only, no GL, no engine state)
//     -> CpuReady      decoded data waits for the GL thread
//     -> Ready         Pump() on the GL-owning thread uploaded it through
//                      Renderer (the ONLY place a GPU object is created)
//   Failed             unknown id, missing file, undecodable data, no renderer;
//                      remembered with its message, never retried by itself
//   Cancelled          demand went away before the work became unavoidable
//                      (treated as Unloaded on the next request)
//
// Coalescing: repeated requests for one id join the same in-flight load;
// nothing is decoded twice. Stale protection: every entry carries a
// generation that Release/Invalidate/ReleaseAll bump; a load that finishes
// for an older generation is discarded in Pump, so a late worker can never
// resurrect a released or superseded resource.
//
// Demand: AddRef/ReleaseRef count the consumers that need an id (a built
// RuntimeWorld, the editor's open scene). A referenced resource is never
// evicted; when the last reference goes while a load is queued/loading the
// job is cancelled. Budget: SetBudgetBytes; when Ready bytes exceed it,
// Pump evicts unreferenced Ready resources least-recently-used first (a
// hit through Get*/TryGet* is a use). An evicted resource is Unloaded and
// simply loads again on the next request; the AssetDatabase is untouched.
//
// Blocking mode (SetBlockingMode(true)) makes Get* do the whole load on the
// calling thread as M30 did — for tools, the harness and the M31
// synchronous-versus-asynchronous measurement. In async mode Get* never
// blocks: it starts the load and returns an invalid handle until Ready.
//
// Thread ownership: every public method is called from the thread that
// owns the manager (the GL thread); workers touch only their own LoadTask.
// Shutdown order (EngineHost): ResourceManager::Shutdown (cancel, wait for
// its jobs, drop decoded data, destroy GPU resources) -> JobSystem
// shutdown -> Renderer shutdown -> window/context.
enum class ResourceState { Unloaded, Queued, Loading, CpuReady, Ready, Failed, Cancelled };
const char* ResourceStateName(ResourceState state);

struct ResourceStats {
    std::size_t loadedMeshes = 0;
    std::size_t loadedTextures = 0;
    std::size_t loadedTerrainMeshes = 0;
    std::size_t failed = 0;
    std::size_t loading = 0;   // Queued + Loading + CpuReady
    std::size_t ready = 0;     // Ready mesh + texture entries
    unsigned long long hits = 0;
    unsigned long long misses = 0;  // loads started
    unsigned long long uploads = 0;  // GPU objects created by Pump
    unsigned long long evictions = 0;
    unsigned long long cancelled = 0;
    unsigned long long staleDiscarded = 0;  // completions rejected by generation
    std::uint64_t bytesResident = 0;   // estimated bytes of Ready resources
    std::uint64_t budgetBytes = 0;
    std::uint64_t peakBytesResident = 0;
};

class ResourceManager {
public:
    // `jobs` may be null: then every asynchronous request is executed
    // synchronously on the caller (equivalent to blocking mode).
    ResourceManager(Renderer* renderer, const AssetDatabase* assets, JobSystem* jobs = nullptr);
    ~ResourceManager();
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    void SetAssetDatabase(const AssetDatabase* assets) { m_assets = assets; }
    const AssetDatabase* Assets() const { return m_assets; }
    void SetBlockingMode(bool blocking) { m_blocking = blocking; }
    bool BlockingMode() const { return m_blocking; }
    // Test seam: with no Renderer, a successful decode becomes Ready with an
    // INVALID handle and real byte accounting, so residency, budget and
    // eviction can be exercised headlessly. No GPU object ever exists in
    // this mode; it is never enabled by the runtime or the editor.
    void SetHeadlessResidency(bool enabled) { m_headlessResidency = enabled; }

    // Non-blocking demand: starts (or joins) the load and returns the
    // state now. Ready is answered from cache (a hit).
    ResourceState RequestMesh(const AssetId& id, JobPriority priority = JobPriority::Normal);
    ResourceState RequestTexture(const AssetId& id, JobPriority priority = JobPriority::Normal);

    // The M30 accessors. Async mode: request and return the handle only if
    // Ready (an invalid handle with outError "loading" otherwise). Blocking
    // mode: load synchronously and return the handle or the failure.
    MeshHandle GetMesh(const AssetId& id, std::string& outError, JobPriority priority = JobPriority::Normal);
    TextureHandle GetTexture(const AssetId& id, std::string& outError, JobPriority priority = JobPriority::Normal);
    // Pure lookups: a valid handle only if Ready; never start a load.
    MeshHandle TryGetMesh(const AssetId& id);
    TextureHandle TryGetTexture(const AssetId& id);

    // Terrain surfaces are engine-constructed (TerrainLibrary), keyed by
    // identifier rather than asset id, and still built synchronously.
    MeshHandle GetTerrainMesh(const std::string& identifier, const RadialTerrain& surface);

    // Demand lifetime. Reference counts protect from eviction; the last
    // ReleaseRef of an in-flight load cancels it.
    void AddRef(const AssetId& id);
    void ReleaseRef(const AssetId& id);
    unsigned int RefCount(const AssetId& id) const;

    // The GL-thread handoff: uploads finished decodes (at most
    // `maxUploads` this call — GPU upload is main-thread work and a
    // multi-megabyte mesh costs milliseconds, so the default spreads a
    // burst of completions over frames), discards stale/cancelled
    // completions, then enforces the budget. Call once per frame.
    void Pump(std::size_t maxUploads = 2);
    // Blocking: waits for every in-flight load and pumps it (tests, tools).
    void WaitForAll();

    ResourceState StateOf(const AssetId& id) const;
    std::string ErrorOf(const AssetId& id) const;
    // Estimated bytes of a Ready entry (0 otherwise).
    std::uint64_t BytesOf(const AssetId& id) const;
    // Which thread decoded this entry (for evidence/tests).
    std::thread::id DecodeThreadOf(const AssetId& id) const;
    std::thread::id OwnerThread() const { return m_ownerThread; }

    // Release destroys the GPU resource (or cancels/discards a load) and
    // bumps the generation; the next request loads afresh. Invalidate is
    // the same plus forgetting a failure (a re-imported or moved file).
    void Release(const AssetId& id);
    void Invalidate(const AssetId& id) { Release(id); }
    void ReleaseAll();
    void Shutdown();

    void SetBudgetBytes(std::uint64_t bytes) { m_budgetBytes = bytes; }
    std::uint64_t BudgetBytes() const { return m_budgetBytes; }
    // Evicts unreferenced Ready resources, LRU first, until resident bytes
    // fit `targetBytes` (or nothing evictable remains). Returns how many.
    std::size_t EvictToFit(std::uint64_t targetBytes);
    static std::uint64_t EstimateMeshBytes(const MeshData& data);
    static std::uint64_t EstimateTextureBytes(const TextureData& data);

    const ResourceStats& Stats() const;
    Renderer* GetRenderer() const { return m_renderer; }
    JobSystem* Jobs() const { return m_jobs; }
    // Snapshot of every entry for the editor (id, state, bytes, refs).
    struct EntryView {
        AssetId id;
        AssetType type = AssetType::Mesh;
        ResourceState state = ResourceState::Unloaded;
        std::uint64_t bytes = 0;
        unsigned int refs = 0;
        double loadMilliseconds = 0.0;  // request -> Ready, once Ready
    };
    std::vector<EntryView> Entries() const;

private:
    // Everything a worker touches. Owned jointly by the entry and the job.
    struct LoadTask {
        AssetId id;
        AssetType type = AssetType::Mesh;
        std::string path;
        unsigned int generation = 0;
        std::atomic<int> stage{0};  // 0 queued, 1 running, 2 done
        bool succeeded = false;
        bool cancelled = false;
        std::string error;
        MeshData mesh;
        TextureData texture;
        std::thread::id decodeThread;
        JobHandle job;
        std::chrono::steady_clock::time_point requested;
    };
    struct Entry {
        ResourceState state = ResourceState::Unloaded;
        AssetType type = AssetType::Mesh;
        MeshHandle mesh;
        TextureHandle texture;
        std::string error;
        unsigned int generation = 0;
        unsigned int refs = 0;
        std::uint64_t bytes = 0;
        unsigned long long lastUse = 0;
        std::shared_ptr<LoadTask> task;
        std::thread::id decodeThread;
        double loadMilliseconds = 0.0;
    };

    Entry& Begin(const AssetId& id, AssetType expected, JobPriority priority);
    bool Resolve(const AssetId& id, AssetType expected, Entry& entry, std::string& outPath);
    static void RunLoadTask(LoadTask& task, const JobContext* context);
    void CompleteTask(Entry& entry, const std::shared_ptr<LoadTask>& task);
    void FinishSynchronously(Entry& entry, const AssetId& id, AssetType expected);
    void DestroyGpu(Entry& entry);
    void Fail(Entry& entry, const std::string& message);
    void RefreshCounts() const;

    Renderer* m_renderer = nullptr;
    const AssetDatabase* m_assets = nullptr;
    JobSystem* m_jobs = nullptr;
    bool m_blocking = false;
    bool m_headlessResidency = false;
    std::thread::id m_ownerThread;
    std::map<AssetId, Entry> m_entries;
    std::map<std::string, MeshHandle> m_terrainMeshes;
    std::vector<std::shared_ptr<LoadTask>> m_inFlight;  // tasks with a job outstanding
    mutable ResourceStats m_stats;
    std::uint64_t m_budgetBytes = 256ull * 1024ull * 1024ull;
    unsigned long long m_useClock = 0;
    bool m_shutDown = false;
};
