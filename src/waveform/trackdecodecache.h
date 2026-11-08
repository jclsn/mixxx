#pragma once

#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QtConcurrentRun>
#include <algorithm>
#include <memory>

#include "track/track.h"

/// Computes data from the decoded audio of a track in the background and
/// keeps the results of the most recently decoded tracks.
///
/// Pointer is a (shared) pointer to the result, which is nullptr while the
/// track is still decoded or if decoding failed. All functions are
/// thread-safe, so they can be called from the render thread.
template<typename Pointer>
class TrackDecodeCache {
  public:
    using Decoder = Pointer (*)(const TrackPointer& pTrack);

    TrackDecodeCache(Decoder decoder, int capacity)
            : m_pState(std::make_shared<State>()),
              m_decoder(decoder),
              m_capacity(capacity) {
    }

    /// Returns the result for the track if it is available. Otherwise
    /// schedules decoding it in the background and returns nullptr.
    Pointer request(const TrackPointer& pTrack) {
        if (!pTrack) {
            return nullptr;
        }

        const QString location = pTrack->getLocation();
        const QMutexLocker locker(&m_pState->mutex);

        const auto it = std::find_if(m_pState->entries.begin(),
                m_pState->entries.end(),
                [&location](const Entry& entry) {
                    return entry.location == location;
                });
        if (it != m_pState->entries.end()) {
            return it->pResult;
        }

        if (m_pState->pending.contains(location)) {
            return nullptr;
        }

        m_pState->pending.insert(location);
        // Capturing the track keeps it alive while decoding, the same way the
        // cover art loading does. The state is captured as well, so it
        // outlives the cache if the decoding finishes later.
        (void)QtConcurrent::run([pTrack,
                                        location,
                                        pState = m_pState,
                                        decoder = m_decoder,
                                        capacity = m_capacity] {
            Pointer pResult = decoder(pTrack);

            const QMutexLocker locker(&pState->mutex);
            pState->pending.remove(location);
            pState->entries.prepend(Entry{location, std::move(pResult)});
            while (pState->entries.size() > capacity) {
                pState->entries.removeLast();
            }
        });

        return nullptr;
    }

  private:
    struct Entry {
        QString location;
        // nullptr if decoding failed, to avoid retrying it on every frame
        Pointer pResult;
    };

    struct State {
        QMutex mutex;
        // Most recently decoded first
        QList<Entry> entries;
        QSet<QString> pending;
    };

    const std::shared_ptr<State> m_pState;
    const Decoder m_decoder;
    const int m_capacity;
};
