// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.xpeng.samples.app;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.SparseArray;

import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.UnguessableToken;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.Linker;
import org.chromium.base.library_loader.ProcessInitException;
import org.chromium.base.memory.MemoryPressureUma;
import org.chromium.base.process_launcher.ChildProcessServiceDelegate;
import com.xpeng.samples.master.ChildProcessCreationParamsImpl;
import com.xpeng.samples.master.SamplesChildProcessConstants;
import com.xpeng.samples_public.common.SamplesProcessInfo;
import com.xpeng.samples_public.common.SamplesSwitches;

import java.util.List;

/**
 * This implementation of {@link ChildProcessServiceDelegate} loads the native library potentially
 * using the custom linker, provides access to view surfaces.
 */
@JNINamespace("samples")
@MainDex
public class SamplesChildProcessServiceDelegate implements ChildProcessServiceDelegate {
    private static final String TAG = "SamplesCPSDelegate";

    // Linker-specific parameters for this child process service.
    private ChromiumLinkerParams mLinkerParams;

    // Child library process type.
    private int mLibraryProcessType;

    private int mCpuCount;
    private long mCpuFeatures;

    private SparseArray<String> mFdsIdsToKeys;

    public SamplesChildProcessServiceDelegate() {
        KillChildUncaughtExceptionHandler.maybeInstallHandler();
    }

    @Override
    public void onServiceCreated() {
        SamplesProcessInfo.setInChildProcess(true);
    }

    @Override
    public void onServiceBound(Intent intent) {
        mLinkerParams = ChromiumLinkerParams.create(intent.getExtras());
        mLibraryProcessType =
                ChildProcessCreationParamsImpl.getLibraryProcessType(intent.getExtras());
    }

    @Override
    public void onConnectionSetup(Bundle connectionBundle, List<IBinder> clientInterfaces) {
        mCpuCount = connectionBundle.getInt(SamplesChildProcessConstants.EXTRA_CPU_COUNT);
        mCpuFeatures = connectionBundle.getLong(SamplesChildProcessConstants.EXTRA_CPU_FEATURES);
        assert mCpuCount > 0;

        if (LibraryLoader.useCrazyLinker()) {
            Bundle sharedRelros = connectionBundle.getBundle(Linker.EXTRA_LINKER_SHARED_RELROS);
            if (sharedRelros != null) {
                getLinker().useSharedRelros(sharedRelros);
            }
        }
    }

    @Override
    public void preloadNativeLibrary(Context hostContext) {
        // This function can be called before command line is set. That is fine because
        // preloading explicitly doesn't run any Chromium code, see NativeLibraryPreloader
        // for more info.
        LibraryLoader.getInstance().preloadNowOverrideApplicationContext(hostContext);
    }

    @Override
    public boolean loadNativeLibrary(Context hostContext) {
        String processType =
                CommandLine.getInstance().getSwitchValue(SamplesSwitches.SWITCH_PROCESS_TYPE);
        String pkgName =
                CommandLine.getInstance().getSwitchValue(BaseSwitches.AFFILIATE_PACKAGE_NAME);
        // Enable selective JNI registration when the process is not the master process.
        if (processType != null && pkgName == null) {
            JNIUtils.enableSelectiveJniRegistration();
        }

        Linker linker = null;
        boolean requestedSharedRelro = false;
        if (LibraryLoader.useCrazyLinker()) {
            assert mLinkerParams != null;
            linker = getLinker();
            if (mLinkerParams.mWaitForSharedRelro) {
                requestedSharedRelro = true;
                linker.initServiceProcess(mLinkerParams.mBaseLoadAddress);
            } else {
                linker.disableSharedRelros();
            }
        }
        boolean isLoaded = false;
        boolean loadAtFixedAddressFailed = false;
        try {
            LibraryLoader.getInstance().loadNowOverrideApplicationContext(hostContext);
            isLoaded = true;
        } catch (ProcessInitException e) {
            if (requestedSharedRelro) {
                Log.w(TAG,
                        "Failed to load native library with shared RELRO, "
                                + "retrying without");
                loadAtFixedAddressFailed = true;
            } else {
                Log.e(TAG, "Failed to load native library", e);
            }
        }
        if (!isLoaded && requestedSharedRelro) {
            linker.disableSharedRelros();
            try {
                LibraryLoader.getInstance().loadNowOverrideApplicationContext(hostContext);
                isLoaded = true;
            } catch (ProcessInitException e) {
                Log.e(TAG, "Failed to load native library on retry", e);
            }
        }
        if (!isLoaded) {
            return false;
        }

        LibraryLoader.getInstance().registerRendererProcessHistogram(
                requestedSharedRelro, loadAtFixedAddressFailed);
        try {
            LibraryLoader.getInstance().initialize(mLibraryProcessType);
        } catch (ProcessInitException e) {
            Log.w(TAG, "startup failed: %s", e);
            return false;
        }

        // Now that the library is loaded, get the FD map,
        // TODO(jcivelli): can this be done in onBeforeMain? We would have to mode onBeforeMain
        // so it's called before FDs are registered.
        nativeRetrieveFileDescriptorsIdsToKeys();

        return true;
    }

    @Override
    public SparseArray<String> getFileDescriptorsIdsToKeys() {
        assert mFdsIdsToKeys != null;
        return mFdsIdsToKeys;
    }

    @Override
    public void onBeforeMain() {
        nativeInitChildProcess(mCpuCount, mCpuFeatures);
        ThreadUtils.postOnUiThread(() -> MemoryPressureUma.initializeForChildService());
    }

    @Override
    public void onDestroy() {
        // Try to shutdown the MainThread gracefully, but it might not have a
        // chance to exit normally.
        nativeShutdownMainThread();
    }

    @Override
    public void runMain() {
        SamplesMain.start(false);
    }

    @Override
    public void runAffiliateThread() {
        SamplesMain.startAffiliateThread();
    }

    // Return a Linker instance. If testing, the Linker needs special setup.
    private Linker getLinker() {
        if (Linker.areTestsEnabled()) {
            // For testing, set the Linker implementation and the test runner
            // class name to match those used by the parent.
            assert mLinkerParams != null;
            Linker.setupForTesting(mLinkerParams.mTestRunnerClassNameForTesting);
        }
        return Linker.getInstance();
    }

    @CalledByNative
    private void setFileDescriptorsIdsToKeys(int[] ids, String[] keys) {
        assert ids.length == keys.length;
        assert mFdsIdsToKeys == null;
        mFdsIdsToKeys = new SparseArray<>();
        for (int i = 0; i < ids.length; ++i) {
            mFdsIdsToKeys.put(ids[i], keys[i]);
        }
    }

    /**
     * Initializes the native parts of the service.
     *
     * @param serviceImpl This ChildProcessService object.
     * @param cpuCount The number of CPUs.
     * @param cpuFeatures The CPU features.
     */
    private native void nativeInitChildProcess(int cpuCount, long cpuFeatures);

    private native void nativeShutdownMainThread();

    // Retrieves the FD IDs to keys map and set it by calling setFileDescriptorsIdsToKeys().
    private native void nativeRetrieveFileDescriptorsIdsToKeys();
}
