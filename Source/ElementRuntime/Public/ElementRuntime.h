#pragma once

#include "CoreMinimal.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "elm/archetype.hpp"
#include "elm/entity.hpp"
#include "elm/registry.hpp"
#include "elm/system.hpp"

#include "Tasks/Task.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <limits>
#include <vector>

class FElementRuntimeScheduler {
public:
    FElementRuntimeScheduler() = default;

    // Call once after systems are built. Pre-computes batch assignments and
    // trace name strings so run() pays no per-frame scheduling overhead.
    void prepare(std::vector<elm::System>& systems)
    {
        Batches.clear();
        TraceNames.Reset();
        PreparedFingerprint = compute_fingerprint(systems);

        std::vector<size_t> pending;
        pending.reserve(systems.size());
        for (size_t i = 0; i < systems.size(); ++i) {
            pending.push_back(i);
        }

        while (!pending.empty()) {
            Batches.push_back(take_parallel_batch_indices(systems, pending));
        }

        TraceNames.SetNum(static_cast<int32>(systems.size()));
        for (size_t i = 0; i < systems.size(); ++i) {
            TraceNames[static_cast<int32>(i)] = FString::Printf(
                TEXT("ElementRuntime::%s"), UTF8_TO_TCHAR(systems[i].name.c_str()));
        }
    }

    void run(std::vector<elm::System>& systems, elm::Registry& registry)
    {
        if (compute_fingerprint(systems) != PreparedFingerprint) {
            prepare(systems);
        }

        enum class EWorkKind { System, Chunk };
        struct FWorkItem {
            elm::System* System = nullptr;
            EWorkKind Kind = EWorkKind::System;
            elm::ChunkView Chunk;
            FString TraceName; // owns the string so lambda copies are safe
        };

        // Declared outside the loop so the internal buffer is reused across batches.
        std::vector<FWorkItem> WorkItems;

        for (const auto& BatchIndices : Batches) {
            WorkItems.clear();

            for (const size_t idx : BatchIndices) {
                auto* System = &systems[idx];
                const FString& BaseTraceName = TraceNames[static_cast<int32>(idx)];

                if (System->supports_chunking && System->run_chunk) {
                    int32 ChunkIndex = 0;
                    for (const auto& Chunk : registry.chunks_for(System->required_components, 256)) {
                        WorkItems.push_back({ System, EWorkKind::Chunk, Chunk,
                            FString::Printf(TEXT("%s [chunk %d]"), *BaseTraceName, ChunkIndex++) });
                    }
                } else {
                    WorkItems.push_back({ System, EWorkKind::System, {}, BaseTraceName });
                }
            }

            if (WorkItems.empty()) {
                continue;
            }

            FEvent* DoneEvent = FPlatformProcess::GetSynchEventFromPool();
            std::atomic<int32> Remaining(static_cast<int32>(WorkItems.size()));

            for (const auto& WorkItem : WorkItems) {
                UE::Tasks::Launch(
                    TEXT("ElementRuntime.Work"),
                    [WorkItem, &registry, &Remaining, DoneEvent] {
                        TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*WorkItem.TraceName);

                        switch (WorkItem.Kind) {
                        case EWorkKind::Chunk:
                            WorkItem.System->run_chunk(registry, WorkItem.Chunk);
                            break;
                        case EWorkKind::System:
                        default:
                            WorkItem.System->run(registry);
                            break;
                        }

                        if (Remaining.fetch_sub(1, std::memory_order_acq_rel) == 1) {
                            DoneEvent->Trigger();
                        }
                    },
                    UE::Tasks::ETaskPriority::Normal,
                    UE::Tasks::EExtendedTaskPriority::None,
                    UE::Tasks::ETaskFlags::None);
            }

            DoneEvent->Wait();
            FPlatformProcess::ReturnSynchEventToPool(DoneEvent);
        }
    }

private:
    static std::vector<size_t> take_parallel_batch_indices(
        const std::vector<elm::System>& systems,
        std::vector<size_t>& pending)
    {
        std::vector<size_t> batch;
        std::vector<size_t> deferred;
        batch.reserve(pending.size());
        deferred.reserve(pending.size());

        for (const size_t idx : pending) {
            bool can_run = true;
            for (const size_t sel_idx : batch) {
                if (elm::conflicts(systems[idx].access, systems[sel_idx].access)) {
                    can_run = false;
                    break;
                }
            }
            if (can_run) {
                for (const size_t def_idx : deferred) {
                    if (elm::conflicts(systems[idx].access, systems[def_idx].access)) {
                        can_run = false;
                        break;
                    }
                }
            }
            if (can_run) {
                batch.push_back(idx);
            } else {
                deferred.push_back(idx);
            }
        }

        pending = std::move(deferred);
        return batch;
    }

    // Fingerprint of system names + access sets. Detects any rebuild that changes
    // system count, identity, or read/write access — not just count changes.
    static size_t compute_fingerprint(const std::vector<elm::System>& systems)
    {
        size_t h = systems.size();
        for (const auto& s : systems) {
            auto combine = [&](size_t v) {
                h ^= v + 0x9e3779b9u + (h << 6) + (h >> 2);
            };
            combine(std::hash<std::string>{}(s.name));
            for (auto id : s.access.readable_access)          { combine(std::hash<const void*>{}(id)); }
            for (auto id : s.access.writable_access)          { combine(std::hash<const void*>{}(id)); }
            for (auto id : s.access.resource_readable_access) { combine(std::hash<const void*>{}(id)); }
            for (auto id : s.access.resource_writable_access) { combine(std::hash<const void*>{}(id)); }
        }
        return h;
    }

    // Pre-computed, stable across frames (systems never change after prepare()).
    std::vector<std::vector<size_t>> Batches;
    TArray<FString> TraceNames;
    size_t PreparedFingerprint = std::numeric_limits<size_t>::max(); // sentinel: no prepare yet
};
