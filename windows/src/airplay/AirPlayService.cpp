// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Satanshu Mishra

#include "airplay/AirPlayService.h"
#include "mdns/MdnsServiceRecord.h"
#include "utilities/Logger.h"

namespace reflection {

AirPlayService::AirPlayService(
    std::unique_ptr<IAirPlayCore> core,
    std::unique_ptr<IMdnsAdvertiser> mdns)
    : core_(std::move(core))
    , mdns_(std::move(mdns))
{
}

AirPlayService::~AirPlayService() {
    stop();
}

bool AirPlayService::start(const AirPlayServiceConfig& config) {
    if (running_) return true;

    Logger::info("Starting AirPlay service as '{}'", config.server_name);

    // Step 1: Initialize core
    const AirPlayCoreConfig core_config{
        .server_name = config.server_name,
        .hardware_address = config.hardware_address,
        .raop_port = config.raop_port,
        .airplay_port = config.airplay_port,
    };

    if (!core_->init(core_config)) {
        Logger::error("Failed to initialize AirPlay core");
        return false;
    }

    // Step 2: Wire callbacks
    wire_callbacks_to_core();

    // Step 3: Start core (begins accepting RTSP connections)
    if (!core_->start()) {
        Logger::error("Failed to start AirPlay core");
        core_->stop();
        return false;
    }

    // Step 4: Advertise via mDNS (use the core's generated public key)
    const auto hw_hex = config.hw_address_hex();
    const auto pk = core_->get_public_key();
    if (!pk.empty()) {
        Logger::info("Using generated public key for mDNS ({}...)", pk.substr(0, 16));
    }
    const auto airplay_record = MdnsServiceRecord::make_airplay_record(
        config.server_name, config.airplay_port, hw_hex, pk);
    const auto raop_record = MdnsServiceRecord::make_raop_record(
        config.server_name, config.raop_port, hw_hex, pk);

    if (!mdns_->advertise(airplay_record)) {
        Logger::error("Failed to advertise AirPlay mDNS service");
        core_->stop();
        return false;
    }

    if (!mdns_->advertise(raop_record)) {
        Logger::error("Failed to advertise RAOP mDNS service");
        mdns_->withdraw_all();
        core_->stop();
        return false;
    }

    running_ = true;
    Logger::info("AirPlay service started -- advertising as '{}'", config.server_name);
    return true;
}

void AirPlayService::stop() {
    if (!running_) return;

    Logger::info("Stopping AirPlay service");

    // Reverse order: withdraw mDNS first, then stop core
    mdns_->withdraw_all();
    core_->stop();

    running_ = false;
    Logger::info("AirPlay service stopped");
}

bool AirPlayService::restart(const AirPlayServiceConfig& config) {
    Logger::info("Restarting AirPlay service with new name '{}'", config.server_name);
    stop();
    return start(config);
}

bool AirPlayService::is_running() const {
    return running_;
}

void AirPlayService::force_reannounce() {
    if (running_ && mdns_) {
        mdns_->force_reannounce();
    }
}

void AirPlayService::set_client_connected_callback(ClientConnectedCallback callback) {
    client_connected_cb_ = std::move(callback);
}

void AirPlayService::set_client_disconnected_callback(ClientDisconnectedCallback callback) {
    client_disconnected_cb_ = std::move(callback);
}

void AirPlayService::set_video_frame_callback(VideoFrameCallback callback) {
    video_frame_cb_ = std::move(callback);
}

void AirPlayService::set_audio_frame_callback(AudioFrameCallback callback) {
    audio_frame_cb_ = std::move(callback);
}

void AirPlayService::wire_callbacks_to_core() {
    // Video: adapt from AirPlayVideoFrame to our (data, size, timestamp, frame_type) callback
    if (video_frame_cb_) {
        core_->set_video_callback(
            [cb = video_frame_cb_](const AirPlayVideoFrame& frame) {
                cb(frame.data, frame.size, frame.timestamp, frame.nal_type);
            });
    }

    // Audio: adapt from AirPlayAudioFrame to our (data, size, timestamp) callback
    if (audio_frame_cb_) {
        core_->set_audio_callback(
            [cb = audio_frame_cb_](const AirPlayAudioFrame& frame) {
                cb(frame.data, frame.size, frame.timestamp);
            });
    }

    // Connection: adapt from AirPlayConnectionEvent to AirPlayClientInfo
    if (client_connected_cb_) {
        core_->set_connection_callback(
            [cb = client_connected_cb_](const AirPlayConnectionEvent& event) {
                cb(AirPlayClientInfo{
                    event.device_id,
                    event.device_name,
                    event.device_model,
                    event.width,
                    event.height,
                });
            });
    }

    // Disconnection: direct pass-through
    if (client_disconnected_cb_) {
        core_->set_disconnection_callback(client_disconnected_cb_);
    }
}

} // namespace reflection
