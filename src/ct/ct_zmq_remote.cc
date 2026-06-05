/*
 * ct_zmq_remote.cc
 *
 * Copyright 2009-2026
 * Giuseppe Penone <giuspen@gmail.com>
 * Evgenii Gurianov <https://github.com/txe>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "ct_zmq_remote.h"

#include "ct_app.h"
#include "ct_logging.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

#include <zmq.h>

namespace {

constexpr const char* kPullBindEndpoint = "tcp://127.0.0.1:19935";
constexpr const char* kSubscriberEndpoint = "tcp://localhost:5556";
constexpr const char* kSubscriberPrefix = "10001 ";

std::string trim_ascii(const std::string& value)
{
    auto is_trim_char = [](const unsigned char ch) {
        return ch == '\0' or std::isspace(ch);
    };

    const auto first = std::find_if_not(value.begin(), value.end(), [&](const char ch) {
        return is_trim_char(static_cast<unsigned char>(ch));
    });
    if (first == value.end()) {
        return {};
    }

    const auto last = std::find_if_not(value.rbegin(), value.rend(), [&](const char ch) {
        return is_trim_char(static_cast<unsigned char>(ch));
    }).base();
    return std::string{first, last};
}

std::optional<gint64> parse_command(const std::string& message)
{
    const std::string trimmed = trim_ascii(message);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    errno = 0;
    char* end = nullptr;
    const long long value = std::strtoll(trimmed.c_str(), &end, 10);
    if (end == trimmed.c_str() or errno == ERANGE) {
        return std::nullopt;
    }

    while (end and *end) {
        if (not std::isspace(static_cast<unsigned char>(*end))) {
            spdlog::debug("ZMQ remote command has trailing data: {}", trimmed);
            break;
        }
        ++end;
    }

    return static_cast<gint64>(value);
}

void set_linger_zero(void* socket)
{
    int linger = 0;
    if (0 != zmq_setsockopt(socket, ZMQ_LINGER, &linger, sizeof(linger))) {
        spdlog::warn("ZMQ remote failed to set linger: {}", zmq_strerror(zmq_errno()));
    }
}

void close_socket(void*& socket)
{
    if (socket) {
        zmq_close(socket);
        socket = nullptr;
    }
}

} // namespace

CtZmqRemote::CtZmqRemote(CtApp& app)
 : _app{app}
{
    _dispatcherConnection = _dispatcher.connect(sigc::mem_fun(*this, &CtZmqRemote::_dispatch_pending));
}

CtZmqRemote::~CtZmqRemote()
{
    stop();
}

void CtZmqRemote::start()
{
    if (_running.exchange(true)) {
        return;
    }

    _stop.store(false);
    _thread = std::thread{&CtZmqRemote::_thread_main, this};
}

void CtZmqRemote::stop()
{
    _stop.store(true);
    if (_thread.joinable()) {
        _thread.join();
    }
    _running.store(false);
    if (_dispatcherConnection.connected()) {
        _dispatcherConnection.disconnect();
    }
}

void CtZmqRemote::_thread_main()
{
    void* context = zmq_ctx_new();
    if (not context) {
        spdlog::error("ZMQ remote failed to create context: {}", zmq_strerror(zmq_errno()));
        _running.store(false);
        return;
    }

    void* receiver = zmq_socket(context, ZMQ_PULL);
    if (not receiver) {
        spdlog::error("ZMQ remote failed to create PULL socket: {}", zmq_strerror(zmq_errno()));
        zmq_ctx_destroy(context);
        _running.store(false);
        return;
    }
    set_linger_zero(receiver);

    if (0 != zmq_bind(receiver, kPullBindEndpoint)) {
        spdlog::error("ZMQ remote failed to bind {}: {}", kPullBindEndpoint, zmq_strerror(zmq_errno()));
        close_socket(receiver);
        zmq_ctx_destroy(context);
        _running.store(false);
        return;
    }
    spdlog::info("ZMQ remote listening on {}", kPullBindEndpoint);

    void* subscriber = zmq_socket(context, ZMQ_SUB);
    if (subscriber) {
        set_linger_zero(subscriber);
        if (0 != zmq_connect(subscriber, kSubscriberEndpoint)) {
            spdlog::warn("ZMQ remote failed to connect subscriber {}: {}", kSubscriberEndpoint, zmq_strerror(zmq_errno()));
            close_socket(subscriber);
        }
        else if (0 != zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, kSubscriberPrefix, std::strlen(kSubscriberPrefix))) {
            spdlog::warn("ZMQ remote failed to set subscriber prefix: {}", zmq_strerror(zmq_errno()));
            close_socket(subscriber);
        }
    }
    else {
        spdlog::warn("ZMQ remote failed to create subscriber socket: {}", zmq_strerror(zmq_errno()));
    }

    while (not _stop.load()) {
        zmq_pollitem_t items[] = {
            {receiver,   0, ZMQ_POLLIN, 0},
            {subscriber, 0, ZMQ_POLLIN, 0},
        };
        const int itemCount = subscriber ? 2 : 1;
        const int pollResult = zmq_poll(items, itemCount, 200);
        if (pollResult < 0) {
            if (_stop.load()) {
                break;
            }
            spdlog::warn("ZMQ remote poll failed: {}", zmq_strerror(zmq_errno()));
            continue;
        }

        if (items[0].revents & ZMQ_POLLIN) {
            char buffer[256];
            const int size = zmq_recv(receiver, buffer, sizeof(buffer), 0);
            if (size < 0) {
                spdlog::warn("ZMQ remote receive failed: {}", zmq_strerror(zmq_errno()));
                continue;
            }

            const std::string message{buffer, buffer + size};
            if (const std::optional<gint64> command = parse_command(message)) {
                _queue_command(*command);
            }
            else {
                spdlog::warn("ZMQ remote ignored invalid command: {}", trim_ascii(message));
            }
        }

        if (subscriber and (items[1].revents & ZMQ_POLLIN)) {
            char buffer[256];
            const int size = zmq_recv(subscriber, buffer, sizeof(buffer), 0);
            if (size >= 0) {
                spdlog::debug("ZMQ remote subscriber message received");
            }
        }
    }

    close_socket(receiver);
    close_socket(subscriber);
    zmq_ctx_destroy(context);
    _running.store(false);
}

void CtZmqRemote::_queue_command(gint64 command)
{
    {
        std::lock_guard<std::mutex> lock{_pendingMutex};
        _pendingCommands.push_back(command);
    }
    _dispatcher.emit();
}

void CtZmqRemote::_dispatch_pending()
{
    std::vector<gint64> commands;
    {
        std::lock_guard<std::mutex> lock{_pendingMutex};
        commands.swap(_pendingCommands);
    }

    for (const gint64 command : commands) {
        _app.zmq_remote_command_received(command);
    }
}
