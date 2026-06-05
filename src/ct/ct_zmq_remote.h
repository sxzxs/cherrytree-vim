/*
 * ct_zmq_remote.h
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

#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include <glib.h>
#include <glibmm/dispatcher.h>
#include <sigc++/connection.h>

class CtApp;

class CtZmqRemote
{
public:
    explicit CtZmqRemote(CtApp& app);
    ~CtZmqRemote();

    CtZmqRemote(const CtZmqRemote&) = delete;
    CtZmqRemote& operator=(const CtZmqRemote&) = delete;

    void start();
    void stop();

private:
    void _thread_main();
    void _queue_command(gint64 command);
    void _dispatch_pending();

private:
    CtApp& _app;
    Glib::Dispatcher _dispatcher;
    sigc::connection _dispatcherConnection;
    std::thread _thread;
    std::atomic_bool _stop{false};
    std::atomic_bool _running{false};
    std::mutex _pendingMutex;
    std::vector<gint64> _pendingCommands;
};
