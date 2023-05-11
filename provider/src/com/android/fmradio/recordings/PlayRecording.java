/*
 * Copyright (C) 2014 The Android Open Source Project
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
package com.android.fmradio.recordings;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.widget.Toast;

import androidx.core.content.FileProvider;

import java.io.File;

public class PlayRecording extends Activity {

    private static final int REQUEST_CODE = 1;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean hasPermissionAudio =
                checkSelfPermission(android.Manifest.permission.READ_MEDIA_AUDIO)
                == PackageManager.PERMISSION_GRANTED;
        boolean hasPermissionVideo =
                checkSelfPermission(android.Manifest.permission.READ_MEDIA_VIDEO)
                == PackageManager.PERMISSION_GRANTED;
        boolean shouldShowRationale = false;
        if (!hasPermissionAudio) {
            shouldShowRationale |= shouldShowRequestPermissionRationale(
                    android.Manifest.permission.READ_MEDIA_AUDIO);
        }
        if (!hasPermissionVideo) {
            shouldShowRationale |= shouldShowRequestPermissionRationale(
                    android.Manifest.permission.READ_MEDIA_VIDEO);
        }

        if (hasPermissionAudio && hasPermissionVideo) {
            startPlayerNow();
            return;
        } else if (shouldShowRationale) {
            Toast.makeText(this, R.string.info_toast, Toast.LENGTH_LONG).show();
        }

        requestPermissions(new String[] {
            android.Manifest.permission.READ_MEDIA_AUDIO,
            android.Manifest.permission.READ_MEDIA_VIDEO
        }, REQUEST_CODE);
    }

    private void startPlayerNow() {
        if (checkSelfPermission(android.Manifest.permission.READ_MEDIA_AUDIO)
                   != PackageManager.PERMISSION_GRANTED
                || checkSelfPermission(android.Manifest.permission.READ_MEDIA_VIDEO)
                   != PackageManager.PERMISSION_GRANTED) {
            Toast.makeText(this, R.string.denied_toast, Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        final Intent oldIntent = getIntent();
        if (oldIntent != null && oldIntent.hasExtra("type") && oldIntent.hasExtra("path")) {
            Intent nextIntent = new Intent(Intent.ACTION_VIEW);
            Uri uri = FileProvider.getUriForFile(this,
                    getApplicationContext().getPackageName(),
                    new File(Uri.parse(oldIntent.getStringExtra("path")).getPath()));
            nextIntent.setDataAndType(uri, oldIntent.getStringExtra("type"));
            nextIntent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            startActivity(nextIntent);
        }

        finish();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions,
                                           int[] grantResults) {
        switch (requestCode) {
            case REQUEST_CODE:
                // If not granted, we show toast, so just go for it whatever happens
                startPlayerNow();
                break;
        }
   }
}
