// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.xpeng.samples_public.master;

import android.content.Context;

import com.xpeng.samples.master.ChildProcessLauncherHelperImpl;

/**
 * Interface for helper launching child processes.
 */
public final class ChildProcessLauncherHelper {
    private ChildProcessLauncherHelper() {}

    /**
     * Creates a ready to use sandboxed child process. Should be called early during startup so the
     * child process is created while other startup work is happening.
     * @param context the application context used for the connection.
     */
    public static void warmUp(Context context) {
        ChildProcessLauncherHelperImpl.warmUp(context);
    }

    /**
     * Starts the moderate binding management that adjust a process priority in response to various
     * signals (app sent to background/foreground for example).
     * Note: WebAPKs and non WebAPKs share the same moderate binding pool, so the size of the
     * shared moderate binding pool is always set based on the number of sandboxes processes
     * used by Chrome.
     * @param context Android's context.
     */
    public static void startModerateBindingManagement(Context context) {
        ChildProcessLauncherHelperImpl.startModerateBindingManagement(context);
    }
}
