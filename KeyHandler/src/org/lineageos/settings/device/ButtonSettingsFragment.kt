/*
 * SPDX-FileCopyrightText: 2021-2025 The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.settings.device

import android.os.Bundle
import androidx.preference.Preference
import com.android.settingslib.widget.SettingsBasePreferenceFragment

class ButtonSettingsFragment : SettingsBasePreferenceFragment() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.button_panel, rootKey)

        val ctx = requireContext()
        mapOf(
                KeyHandler.POSITION_TOP to KeyHandler.ALERT_SLIDER_TOP_KEY,
                KeyHandler.POSITION_MIDDLE to KeyHandler.ALERT_SLIDER_MIDDLE_KEY,
                KeyHandler.POSITION_BOTTOM to KeyHandler.ALERT_SLIDER_BOTTOM_KEY,
            )
            .forEach { (position, key) ->
                if (!KeyHandler.isPositionSupported(ctx, position)) {
                    findPreference<Preference>(key)?.let { pref ->
                        pref.parent?.removePreference(pref)
                    }
                }
            }
    }
}
