// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.xpeng.samples.master;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

/**
 * Java accessor for base/feature_list.h state.
 */
@JNINamespace("samples::android")
@MainDex
public abstract class SamplesFeatureList {
    // Prevent instantiation.
    private SamplesFeatureList() {}

    /**
     * Returns whether the specified feature is enabled or not.
     *
     * Note: Features queried through this API must be added to the array
     * |kFeaturesExposedToJava| in samples/master/android/samples_feature_list.cc
     *
     * @param featureName The name of the feature to query.
     * @return Whether the feature is enabled or not.
     */
    public static boolean isEnabled(String featureName) {
        return nativeIsEnabled(featureName);
    }

    // Alphabetical:
    public static final String ENHANCED_SELECTION_INSERTION_HANDLE =
            "EnhancedSelectionInsertionHandle";

    public static final String BACKGROUND_MEDIA_RENDERER_HAS_MODERATE_BINDING =
            "BackgroundMediaRendererHasModerateBinding";

    private static native boolean nativeIsEnabled(String featureName);
}
