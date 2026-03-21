#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace reflection {

/// Owned copy of a video frame's NAL unit data.
/// RPiPlay's data pointers are only valid during the callback, so we copy here.
struct OwnedVideoFrame {
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
};

/// Thread-safe SPSC queue for bridging video frames from the RAOP callback
/// thread to the main/render thread.
///
/// Producer (RAOP thread): push() copies NAL data into the queue.
/// Consumer (main thread): try_pop() retrieves the next frame.
///
/// Drop-oldest policy: if the queue is full, the oldest frame is discarded
/// to keep latency low. This is correct for real-time video — displaying
/// a stale frame is worse than dropping it.
class VideoFrameQueue {
public:
    static constexpr size_t kDefaultCapacity = 16;

    explicit VideoFrameQueue(size_t capacity = kDefaultCapacity)
        : capacity_(capacity)
    {
    }

    /// Push a frame into the queue, copying the data.
    /// If the queue is full, the oldest frame is dropped.
    /// Thread-safe: called from RAOP callback thread.
    void push(const uint8_t* data, size_t size, uint64_t timestamp) {
        if (!data || size == 0) return;

        OwnedVideoFrame frame;
        frame.data.assign(data, data + size);
        frame.timestamp = timestamp;

        std::lock_guard lock(mutex_);
        if (frames_.size() >= capacity_) {
            frames_.pop_front();  // Drop oldest
        }
        frames_.push_back(std::move(frame));
    }

    /// Try to pop the next frame from the queue.
    /// Returns true if a frame was available, false if the queue is empty.
    /// Thread-safe: called from main/render thread.
    bool try_pop(OwnedVideoFrame& out) {
        std::lock_guard lock(mutex_);
        if (frames_.empty()) return false;

        out = std::move(frames_.front());
        frames_.pop_front();
        return true;
    }

    /// Clear all queued frames.
    /// Thread-safe.
    void clear() {
        std::lock_guard lock(mutex_);
        frames_.clear();
    }

    /// Current number of queued frames.
    /// Thread-safe.
    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return frames_.size();
    }

    /// Whether the queue is empty.
    /// Thread-safe.
    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return frames_.empty();
    }

private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<OwnedVideoFrame> frames_;
};

} // namespace reflection
