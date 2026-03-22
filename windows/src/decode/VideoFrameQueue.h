#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

namespace reflection {

/// Owned copy of a video frame's NAL unit data with H.264 frame type info.
struct OwnedVideoFrame {
    std::vector<uint8_t> data;
    uint64_t timestamp = 0;
    uint8_t frame_type = 0;       // RPiPlay: 0=SPS/PPS, 1=video frame
    bool is_idr_or_sps = false;   // True if this contains keyframe data (IDR/SPS/PPS)
};

/// Thread-safe queue for video frames with IDR-aware drop policy.
///
/// Industry-standard approach for real-time video when the decoder can't
/// keep up: preserve keyframe (IDR/SPS/PPS) data, drop P-frames first.
/// This ensures the decoder can always restart from a clean reference
/// frame instead of producing corrupted output from broken reference chains.
///
/// Drop policy:
///   - When full and new frame is IDR/SPS/PPS: drop oldest P-frame, keep IDR
///   - When full and new frame is P-frame: drop the new P-frame (don't evict IDR)
///   - SPS/PPS and IDR frames are NEVER dropped from the queue
class VideoFrameQueue {
public:
    static constexpr size_t kDefaultCapacity = 60;  // ~2 seconds at 30fps

    explicit VideoFrameQueue(size_t capacity = kDefaultCapacity)
        : capacity_(capacity)
    {
    }

    /// Push a frame with IDR-aware drop policy.
    /// @param data       Raw H.264 NAL unit bytes
    /// @param size       Size of data
    /// @param timestamp  Presentation timestamp
    /// @param frame_type RPiPlay frame type (0=SPS/PPS, 1=video)
    void push(const uint8_t* data, size_t size, uint64_t timestamp,
              uint8_t frame_type) {
        if (!data || size == 0) return;

        OwnedVideoFrame frame;
        frame.data.assign(data, data + size);
        frame.timestamp = timestamp;
        frame.frame_type = frame_type;

        // Detect IDR/SPS/PPS by checking H.264 NAL unit type.
        // After the Annex B start code (00 00 00 01), the NAL type is in
        // bits 0-4 of the next byte. Type 5=IDR, 7=SPS, 8=PPS.
        frame.is_idr_or_sps = detect_keyframe(data, size, frame_type);

        std::lock_guard lock(mutex_);

        if (frames_.size() >= capacity_) {
            if (frame.is_idr_or_sps) {
                // New frame is keyframe — drop oldest P-frame to make room
                drop_oldest_non_idr();
                ++frames_dropped_;
            } else {
                // New frame is P-frame and queue is full — drop this P-frame
                ++frames_dropped_;
                return;
            }
        }

        ++frames_received_;
        frames_.push_back(std::move(frame));
    }

    bool try_pop(OwnedVideoFrame& out) {
        std::lock_guard lock(mutex_);
        if (frames_.empty()) return false;
        out = std::move(frames_.front());
        frames_.pop_front();
        return true;
    }

    void clear() {
        std::lock_guard lock(mutex_);
        frames_.clear();
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard lock(mutex_);
        return frames_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return frames_.empty();
    }

    /// Get and reset drop counter (for metrics logging).
    uint64_t take_drop_count() {
        std::lock_guard lock(mutex_);
        const auto count = frames_dropped_;
        frames_dropped_ = 0;
        return count;
    }

    uint64_t take_receive_count() {
        std::lock_guard lock(mutex_);
        const auto count = frames_received_;
        frames_received_ = 0;
        return count;
    }

private:
    size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<OwnedVideoFrame> frames_;
    uint64_t frames_dropped_ = 0;
    uint64_t frames_received_ = 0;

    /// Detect if a NAL unit contains keyframe data (IDR, SPS, or PPS).
    static bool detect_keyframe(const uint8_t* data, size_t size, uint8_t frame_type) {
        // RPiPlay frame_type 0 = SPS/PPS parameter sets
        if (frame_type == 0) return true;

        // Check H.264 NAL unit type after Annex B start code
        if (size >= 5 && data[0] == 0x00 && data[1] == 0x00 &&
            data[2] == 0x00 && data[3] == 0x01) {
            const uint8_t nal_type = data[4] & 0x1F;
            // 5=IDR slice, 7=SPS, 8=PPS
            return (nal_type == 5 || nal_type == 7 || nal_type == 8);
        }

        return false;
    }

    /// Drop the oldest non-IDR frame from the queue.
    /// If ALL frames are IDR/SPS (unlikely), drops the oldest anyway.
    void drop_oldest_non_idr() {
        for (auto it = frames_.begin(); it != frames_.end(); ++it) {
            if (!it->is_idr_or_sps) {
                frames_.erase(it);
                return;
            }
        }
        // All frames are keyframes — drop oldest as last resort
        if (!frames_.empty()) {
            frames_.pop_front();
        }
    }
};

} // namespace reflection
