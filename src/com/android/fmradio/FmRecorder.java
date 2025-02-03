/*
 * Copyright (C) 2014,2022 The Android Open Source Project
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

package com.android.fmradio;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.res.Resources;
import android.database.Cursor;
import android.media.AudioFormat;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Environment;
import android.os.SystemClock;
import android.os.UserHandle;
import android.provider.MediaStore;
import android.text.format.DateFormat;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * This class provider interface to recording, stop recording, save recording
 * file, play recording file
 */
public class FmRecorder implements MediaRecorder.OnErrorListener, MediaRecorder.OnInfoListener {
    private static final String TAG = "FmRecorder";
    // file prefix
    public static final String RECORDING_FILE_PREFIX = "FM";
    // file extension
    public static final String RECORDING_FILE_EXTENSION = ".mp3";
    // recording file type
    private static final String RECORDING_FILE_TYPE = "audio/mpeg";
    // error type no sdcard
    public static final int ERROR_SDCARD_NOT_PRESENT = 0;
    // error type sdcard not have enough space
    public static final int ERROR_SDCARD_INSUFFICIENT_SPACE = 1;
    // error type can't write sdcard
    public static final int ERROR_SDCARD_WRITE_FAILED = 2;
    // error type recorder internal error occur
    public static final int ERROR_RECORDER_INTERNAL = 3;

    // FM Recorder state not recording and not playing
    public static final int STATE_IDLE = 5;
    // FM Recorder state recording
    public static final int STATE_RECORDING = 6;
    // FM Recorder state playing
    public static final int STATE_PLAYBACK = 7;
    // FM Recorder state invalid, need to check
    public static final int STATE_INVALID = -1;

    // use to record current FM recorder state
    public int mInternalState = STATE_IDLE;
    // the recording time after start recording
    private long mRecordTime = 0;
    // record start time
    private long mRecordStartTime = 0;
    // current record file
    private File mRecordFile = null;
    // record current record file is saved by user
    private boolean mIsRecordingFileSaved = false;
    // listener use for notify service the record state or error state
    private OnRecorderStateChangedListener mStateListener = null;
    // recorder use for record file
    private MediaRecorder mRecorder;

    /**
     * Start recording the voice of FM, also check the pre-conditions, if not
     * meet, will return an error message to the caller. if can start recording
     * success, will set FM record state to recording and notify to the caller
     */
    public void startRecording(Context context) {
        mRecordTime = 0;

        // Check external storage
        if (!Environment.MEDIA_MOUNTED.equals(Environment.getExternalStorageState())) {
            Log.e(TAG, "startRecording, no external storage available");
            setError(ERROR_SDCARD_NOT_PRESENT);
            return;
        }

        String recordingSdcard = FmUtils.getDefaultStoragePath();
        // check whether have sufficient storage space, if not will notify
        // caller error message
        if (!FmUtils.hasEnoughSpace(recordingSdcard)) {
            setError(ERROR_SDCARD_INSUFFICIENT_SPACE);
            Log.e(TAG, "startRecording, SD card does not have sufficient space!!");
            return;
        }

        // get external storage directory
        File recordingDir = new File(recordingSdcard, getFmRecordFolder(context));
        // exist a file named FM Recording, so can't create FM recording folder
        if (recordingDir.exists() && !recordingDir.isDirectory()) {
            Log.e(TAG, "startRecording, a file with name \"FM Recording\" already exists!!");
            setError(ERROR_SDCARD_WRITE_FAILED);
            return;
        } else if (!recordingDir.exists()) { // try to create recording folder
            boolean mkdirResult = recordingDir.mkdirs();
            if (!mkdirResult) { // create recording file failed
                setError(ERROR_RECORDER_INTERNAL);
                return;
            }
        }
        // create recording temporary file
        long curTime = System.currentTimeMillis();
        Date date = new Date(curTime);
        SimpleDateFormat simpleDateFormat = new SimpleDateFormat("MMddyyyy_HHmmss",
                Locale.ENGLISH);
        String time = simpleDateFormat.format(date);
        StringBuilder stringBuilder = new StringBuilder();
        stringBuilder.append(time).append(RECORDING_FILE_EXTENSION);
        String name = stringBuilder.toString();
        mRecordFile = new File(recordingDir, name);
        try {
            if (mRecordFile.createNewFile()) {
                Log.d(TAG, "startRecording, createNewFile success with path "
                        + mRecordFile.getPath());
            }
        } catch (IOException e) {
            Log.e(TAG, "startRecording, IOException while createTempFile: " + e);
            e.printStackTrace();
            setError(ERROR_SDCARD_WRITE_FAILED);
            return;
        }

        final long maxFileSize = FmUtils.getAvailableSpace(recordingSdcard) -
            FmUtils.LOW_SPACE_THRESHOLD;

        // set record parameter and start recording
        try {
            if (mRecorder != null && STATE_RECORDING == mInternalState) {
                stopRecording();
            }

            mRecorder = new MediaRecorder();
            mRecorder.setMaxFileSize(maxFileSize);
            mRecorder.setMaxDuration(0);
            mRecorder.setAudioSource(MediaRecorder.AudioSource.RADIO_TUNER);
            mRecorder.setOutputFormat(MediaRecorder.OutputFormat.THREE_GPP);
            mRecorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
            mRecorder.setAudioSamplingRate(44100);
            mRecorder.setAudioEncodingBitRate(128000);
            mRecorder.setAudioChannels(2);
            mRecorder.setOutputFile(mRecordFile.getAbsolutePath());
            mRecorder.prepare();
            Log.d(TAG, "startRecording, recorder.start()");
            mRecorder.start();
            mRecordStartTime = SystemClock.elapsedRealtime();
            mIsRecordingFileSaved = false;
        } catch (RuntimeException|IOException e) {
            Log.e(TAG, "startRecording, error while starting recording!", e);
            setError(ERROR_RECORDER_INTERNAL);
            mRecorder.reset();
            mRecorder.release();
            mRecorder = null;
            return;
        }
        mRecorder.setOnErrorListener(this);
        mRecorder.setOnInfoListener(this);
        setState(STATE_RECORDING);
    }

    /**
     * Stop recording, compute recording time and update FM recorder state
     */
    public void stopRecording() {
        if (STATE_RECORDING != mInternalState) {
            Log.w(TAG, "stopRecording, called in wrong state!!");
            return;
        }

        mRecordTime = SystemClock.elapsedRealtime() - mRecordStartTime;
        stopRecorder();
        setState(STATE_IDLE);
    }

    /**
     * Compute the current record time
     *
     * @return The current record time
     */
    public long getRecordTime() {
        if (STATE_RECORDING == mInternalState) {
            mRecordTime = SystemClock.elapsedRealtime() - mRecordStartTime;
        }
        return mRecordTime;
    }

    /**
     * Get FM recorder current state
     *
     * @return FM recorder current state
     */
    public int getState() {
        return mInternalState;
    }

    /**
     * Get current record file name
     *
     * @return The current record file name
     */
    public String getRecordFileName() {
        if (mRecordFile != null) {
            String fileName = mRecordFile.getName();
            int index = fileName.indexOf(RECORDING_FILE_EXTENSION);
            if (index > 0) {
                fileName = fileName.substring(0, index);
            }
            return fileName;
        }
        return null;
    }

    /**
     * Save recording file with the given name, and insert it's info to database
     *
     * @param context The context
     * @param newName The name to override default recording name
     */
    public void saveRecording(Context context, String newName) {
        if (mRecordFile == null) {
            Log.e(TAG, "saveRecording, recording file is null!");
            return;
        }

        File newRecordFile = new File(mRecordFile.getParent(), newName + RECORDING_FILE_EXTENSION);
        boolean succuss = mRecordFile.renameTo(newRecordFile);
        if (succuss) {
            mRecordFile = newRecordFile;
        }
        mIsRecordingFileSaved = true;
        // insert recording file info to database
        addRecordingToDatabase(context);
    }

    /**
     * Discard current recording file, release recorder and player
     */
    public void discardRecording() {
        if ((STATE_RECORDING == mInternalState)) {
            stopRecorder();
        }

        if (mRecordFile != null && !mIsRecordingFileSaved) {
            if (!mRecordFile.delete()) {
                // deletion failed, possibly due to hot plug out SD card
                Log.d(TAG, "discardRecording, delete file failed!");
            }
            mRecordFile = null;
            mRecordStartTime = 0;
            mRecordTime = 0;
        }
        setState(STATE_IDLE);
    }

    /**
     * Set the callback use to notify FM recorder state and error message
     *
     * @param listener the callback
     */
    public void registerRecorderStateListener(OnRecorderStateChangedListener listener) {
        mStateListener = listener;
    }

    /**
     * Interface to notify FM recorder state and error message
     */
    public interface OnRecorderStateChangedListener {
        /**
         * notify FM recorder state
         *
         * @param state current FM recorder state
         */
        void onRecorderStateChanged(int state);

        /**
         * notify FM recorder error message
         *
         * @param error error type
         */
        void onRecorderError(int error);
    }

    /**
     * Called when an error occurs while recording.
     *
     * @param mr the MediaRecorder that encountered the error
     * @param what    the type of error that has occurred:
     * <ul>
     * <li>{@link #MEDIA_RECORDER_ERROR_UNKNOWN}
     * <li>{@link #MEDIA_ERROR_SERVER_DIED}
     * </ul>
     * @param extra   an extra code, specific to the error type
     */
    @Override
    public void onError(MediaRecorder mr, int what, int extra) {
        Log.e(TAG, "onError, what = " + what + ", extra = " + extra);
        stopRecorder();
        int error = ERROR_RECORDER_INTERNAL;
        if (what == MediaRecorder.MEDIA_RECORDER_ERROR_UNKNOWN) {
            final String recordingSdcard = FmUtils.getDefaultStoragePath();
            if (!FmUtils.hasEnoughSpace(recordingSdcard)) {
                error = ERROR_SDCARD_INSUFFICIENT_SPACE;
            }
        }
        setError(error);
        if (STATE_RECORDING == mInternalState) {
            setState(STATE_IDLE);
        }
    }

    /**
     * Called to indicate an info or a warning during recording.
     *
     * @param mr   the MediaRecorder the info pertains to
     * @param what the type of info or warning that has occurred
     * <ul>
     * <li>{@link #MEDIA_RECORDER_INFO_UNKNOWN}
     * <li>{@link #MEDIA_RECORDER_INFO_MAX_DURATION_REACHED}
     * <li>{@link #MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED}
     * </ul>
     * @param extra   an extra code, specific to the info type
     */
    @Override
    public void onInfo(MediaRecorder mr, int what, int extra) {
        Log.e(TAG, "onInfo, what = " + what + ", extra = " + extra);
        final int error;
        switch (what) {
            case MediaRecorder.MEDIA_RECORDER_INFO_MAX_DURATION_REACHED:
                error = ERROR_RECORDER_INTERNAL;
                break;

            case MediaRecorder.MEDIA_RECORDER_INFO_MAX_FILESIZE_REACHED:
                error = ERROR_SDCARD_INSUFFICIENT_SPACE;
                break;

            default: return;
        }
        stopRecorder();
        setError(error);
        if (STATE_RECORDING == mInternalState) {
            setState(STATE_IDLE);
        }
    }

    /**
     * Reset FM recorder
     */
    public void resetRecorder() {
        stopRecorder();
        mRecordFile = null;
        mRecordStartTime = 0;
        mRecordTime = 0;
        mInternalState = STATE_IDLE;
    }

    /**
     * Notify error message to the callback
     *
     * @param error FM recorder error type
     */
    private void setError(int error) {
        if (mStateListener != null) {
            mStateListener.onRecorderError(error);
        }
    }

    /**
     * Notify FM recorder state message to the callback
     *
     * @param state FM recorder current state
     */
    private void setState(int state) {
        mInternalState = state;
        if (mStateListener != null) {
            mStateListener.onRecorderStateChanged(state);
        }
    }

    /**
     * Save recording file info to database
     *
     * @param context The context
     */
    private void addRecordingToDatabase(final Context context) {
        Resources res = context.getResources();
        ContentValues cv = new ContentValues();
        long current = System.currentTimeMillis();
        long modDate = mRecordFile.lastModified();
        Date date = new Date(current);
        String title = getRecordFileName();

        // Lets label the recorded audio file as NON-MUSIC so that the file
        // won't be displayed automatically, except for in the playlist.
        cv.put(MediaStore.Audio.Media.DURATION, mRecordTime);
        cv.put(MediaStore.Audio.Media.TITLE, title);
        cv.put(MediaStore.Audio.Media.DATA, mRecordFile.getAbsolutePath());
        cv.put(MediaStore.Audio.Media.DATE_ADDED, (int) (current / 1000));
        cv.put(MediaStore.Audio.Media.DATE_MODIFIED, (int) (modDate / 1000));
        cv.put(MediaStore.Audio.Media.MIME_TYPE, RECORDING_FILE_TYPE);
        cv.put(MediaStore.Audio.Media.ARTIST,
                res.getString(R.string.audio_db_artist_name));
        cv.put(MediaStore.Audio.Media.ALBUM,
                res.getString(R.string.audio_db_album_name));
        Log.d(TAG, "Inserting audio record: " + cv.toString());
        ContentResolver resolver = context.getContentResolver();
        Uri base = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        Log.d(TAG, "ContentURI: " + base);
        Uri result = resolver.insert(base, cv);
        if (result == null) {
            Log.e(TAG, "Unable to save recorded audio");
            return;
        }
        if (getPlaylistId(context) == -1) {
            createPlaylist(res, resolver);
        }
        int audioId = Integer.valueOf(result.getLastPathSegment());
        addToPlaylist(resolver, audioId, getPlaylistId(context));

        // Notify those applications such as Music listening to the
        // scanner events that a recorded audio file just created.
        context.sendBroadcastAsUser(new
                Intent(Intent.ACTION_MEDIA_SCANNER_SCAN_FILE, result),
                new UserHandle(UserHandle.USER_CURRENT));
    }

    public static int getPlaylistId(Context context) {
        Uri uri = MediaStore.Audio.Playlists.getContentUri("external");
        final String[] ids = new String[] { MediaStore.Audio.Playlists._ID };
        final String where = MediaStore.Audio.Playlists.NAME + "=?";
        final String[] args = new String[] {
            context.getResources().getString(R.string.audio_db_playlist_name)
        };

        Cursor cursor = null;
        try {
            ContentResolver resolver = context.getContentResolver();
            if (resolver != null) {
                cursor = resolver.query(uri, ids, where, args, null);
            }
        } catch (UnsupportedOperationException ex) { }

        int id = -1;
        if (cursor == null) {
            Log.v(TAG, "query returns null");
        }
        if (cursor != null) {
            cursor.moveToFirst();
            if (!cursor.isAfterLast()) {
                id = cursor.getInt(0);
            }
            cursor.close();
        }
        return id;
    }

    private Uri createPlaylist(Resources res, ContentResolver resolver) {
        ContentValues cv = new ContentValues();
        cv.put(MediaStore.Audio.Playlists.NAME,
                res.getString(R.string.audio_db_playlist_name));
        Uri uri = resolver.insert(MediaStore.Audio.Playlists.getContentUri("external"), cv);
        if (uri == null) {
            Log.e(TAG, "Unable to save recorded audio");
        }
        return uri;
    }

       private void addToPlaylist(ContentResolver resolver, int audioId, long playlistId) {
       Uri uri = MediaStore.Audio.Playlists.Members.getContentUri("external", playlistId);

       ContentValues values = new ContentValues();
       values.put(MediaStore.Audio.Playlists.Members.PLAY_ORDER, audioId);
       values.put(MediaStore.Audio.Playlists.Members.AUDIO_ID, audioId);
       try {
           resolver.insert(uri, values);
       } catch (Exception exception) {
           exception.printStackTrace();
       }
   }

    private void stopRecorder() {
        if (mRecorder != null) {
            try {
                mRecorder.stop();
            } catch(Exception e) {
                e.printStackTrace();
            } finally {
                Log.d(TAG, "stopRecorder, reset and release of mRecorder");
                mRecorder.reset();
                mRecorder.release();
                mRecorder = null;
            }
        }
    }

    public static String getFmRecordFolder(Context ctx) {
        Resources res = ctx.getResources();
        return Environment.DIRECTORY_RECORDINGS + File.separator +
            res.getString(R.string.audio_save_dir_name);
    }

    public long getFileSize() {
        return mRecordFile == null ? 0 : mRecordFile.length();
    }
}
