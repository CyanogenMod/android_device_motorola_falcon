/*
 * Copyright (c) 2016 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.cyanogenmod.cdmacarrierconfig;

import android.content.DialogInterface;
import android.os.Bundle;
import android.os.SystemProperties;
import android.support.v14.preference.PreferenceFragment;
import android.app.AlertDialog;
import android.support.v7.preference.ListPreference;
import android.support.v7.preference.Preference;
import android.support.v7.preference.PreferenceScreen;
import android.text.InputType;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.EditText;

public class CarrierPreferenceFragment extends PreferenceFragment implements
        Preference.OnPreferenceChangeListener {

    private static final String OPERATOR_NAME_PERSIST_PROP = "persist.operator.alpha";
    private static final String OPERATOR_NAME_RO_PROP = "ro.cdma.home.operator.alpha";
    private static final String OPERATOR_NUMERIC_PERSIST_PROP = "persist.operator.numeric";
    private static final String OPERATOR_NUMERIC_RO_PROP = "ro.cdma.home.operator.numeric";

    private static final String KEY_PREFERENCE_SCREEN = "carrier_preference_screen";

    private static final String KEY_OPERATOR_LIST = "operator_list";
    private static final String KEY_OPERATOR_NAME = "operator_name";
    private static final String KEY_OPERATOR_NUMERIC = "operator_numeric";

    private static final String KEY_REBOOT_NEEDED = "reboot_needed_pref";

    private PreferenceScreen mPreferenceScreen;

    private ListPreference mOperatorListPref;
    private Preference mOperatorNamePref;
    private Preference mOperatorNumericPref;

    private String mCurrentName;
    private String mCurrentNumeric;

    private Preference mRebootMessage;

    private boolean mRebootMessageShown = true;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        addPreferencesFromResource(R.xml.carrier_settings);

        mPreferenceScreen = (PreferenceScreen) findPreference(KEY_PREFERENCE_SCREEN);

        mOperatorListPref = (ListPreference) findPreference(KEY_OPERATOR_LIST);
        mOperatorNamePref = findPreference(KEY_OPERATOR_NAME);
        mOperatorNumericPref = findPreference(KEY_OPERATOR_NUMERIC);

        mRebootMessage = findPreference(KEY_REBOOT_NEEDED);

        mOperatorNamePref.setOnPreferenceChangeListener(this);
        mOperatorListPref.setOnPreferenceChangeListener(this);
        mOperatorNumericPref.setOnPreferenceChangeListener(this);

        /* Read the system property to get the current values */
        mCurrentNumeric = SystemProperties.get(OPERATOR_NUMERIC_PERSIST_PROP);
        if (!mCurrentNumeric.isEmpty()) {
            mOperatorNumericPref.setSummary(mCurrentNumeric);
        }
        mCurrentName = SystemProperties.get(OPERATOR_NAME_PERSIST_PROP);
        if (!mCurrentName.isEmpty()) {
            mOperatorNamePref.setSummary(mCurrentName);
        }

        /* If the user changed the property with setprop, this can be incorrect */
        if (mOperatorListPref.getValue() == null) {
            /* Default to 'Manual' */
            mOperatorListPref.setValueIndex(0);
        }
        if (!mOperatorListPref.getValue().isEmpty()) {
            mOperatorNamePref.setEnabled(false);
            mOperatorNumericPref.setEnabled(false);
        }

        if (!isRebootNeeded()) {
            mRebootMessageShown = false;
            mPreferenceScreen.removePreference(mRebootMessage);
        }
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (KEY_OPERATOR_NUMERIC.equals(preference.getKey())) {
            showDialogForNumeric();
        } else if (KEY_OPERATOR_NAME.equals(preference.getKey())) {
            showDialogForName();
        }
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (KEY_OPERATOR_LIST.equals(preference.getKey())) {
            String operatorNumeric = (String) newValue;
            if (operatorNumeric.isEmpty()) {
                mOperatorNamePref.setEnabled(true);
                mOperatorNumericPref.setEnabled(true);
            } else {
                mOperatorNamePref.setEnabled(false);
                mOperatorNumericPref.setEnabled(false);

                int index = mOperatorListPref.findIndexOfValue(operatorNumeric);
                String operatorName = mOperatorListPref.getEntries()[index].toString();
                setOperatorNumeric(operatorNumeric);
                setOperatorName(operatorName);
            }
            return true;
        }
        return false;
    }

    private boolean isRebootNeeded() {
        String roNumeric = SystemProperties.get(OPERATOR_NUMERIC_RO_PROP);
        String roName = SystemProperties.get(OPERATOR_NAME_RO_PROP);
        return !(roName.equals(mCurrentName) && roNumeric.equals(mCurrentNumeric));
    }

    private void updateRebootMessage() {
        if (!isRebootNeeded() && !mRebootMessageShown) {
            mRebootMessageShown = true;
            mPreferenceScreen.addPreference(mRebootMessage);
        }
    }

    private void setOperatorNumeric(String numeric) {
        mCurrentNumeric = numeric;
        mOperatorNumericPref.setSummary(numeric);
        SystemProperties.set(OPERATOR_NUMERIC_PERSIST_PROP, numeric);
        updateRebootMessage();
    }

    private void setOperatorName(String name) {
        mCurrentName = name;
        mOperatorNamePref.setSummary(name);
        SystemProperties.set(OPERATOR_NAME_PERSIST_PROP, name);
        updateRebootMessage();
    }

    private void showDialogForNumeric() {
        LayoutInflater li = LayoutInflater.from(getActivity());
        View dialogView = li.inflate(R.layout.carrier_dialog, null);
        final EditText editText = (EditText) dialogView.findViewById(R.id.editText);
        editText.setText(mCurrentNumeric);
        editText.setSelection(editText.getText().length());
        editText.setInputType(InputType.TYPE_CLASS_NUMBER);

        new AlertDialog.Builder(getActivity())
                .setTitle(R.string.operator_numeric_dialog_title)
                .setView(dialogView)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        setOperatorNumeric(editText.getText().toString());
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void showDialogForName() {
        LayoutInflater li = LayoutInflater.from(getActivity());
        View dialogView = li.inflate(R.layout.carrier_dialog, null);
        final EditText editText = (EditText) dialogView.findViewById(R.id.editText);
        editText.setText(mCurrentName);
        editText.setSelection(editText.getText().length());

        new AlertDialog.Builder(getActivity())
                .setTitle(R.string.operator_name_dialog_title)
                .setView(dialogView)
                .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int which) {
                        setOperatorName(editText.getText().toString());
                    }
                })
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }
}
