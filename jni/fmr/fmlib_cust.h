/*
 * Copyright (C) 2020 The Android Open Source Project
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

#ifndef __FMLIB_CUST_H__
#define __FMLIB_CUST_H__

#define FM_CHIP_MT6616 0x6616
#define FM_CHIP_MT6620 0x6620
#define FM_CHIP_MT6626 0x6626
#define FM_CHIP_MT6628 0x6628
#define FM_CHIP_MT6627 0x6627
#define FM_CHIP_MT6580 0x6580
#define FM_CHIP_MT6630 0x6630
#define FM_CHIP_MT6631 0x6631
#define FM_CHIP_MT6632 0x6632
#define FM_CHIP_MT6635 0x6635

#define FM_CHIP_UNSUPPORTED -1

#define FM_JNI_SCAN_SPACE_50K 5
#define FM_JNI_SCAN_SPACE_100K 1
#define FM_JNI_SCAN_SPACE_200K 2

/*implement fm scan by soft mute tune
 change to 0 will scan by orginal way*/
#define FMR_SOFT_MUTE_TUEN_SCAN 1
#define FMR_NOISE_FLOORT_DETECT 1
#define RSSI_TH -296
#define FM_SEVERE_RSSI_TH -107  // 67dBuV
#define FM_NOISE_FLOOR_OFFSET 10

#endif  // __FMLIB_CUST_H__
