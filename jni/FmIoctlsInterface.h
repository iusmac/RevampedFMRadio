/*
Copyright (c) 2015, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef __FM_IOCTL_INTERFACE_H__
#define __FM_IOCTL_INTERFACE_H__

#include "FM_Const.h"

#include <linux/videodev2.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

// FM HAL Callback Types
typedef void (*enb_result_cb)(void);
typedef void (*tune_rsp_cb)(int Freq);
typedef void (*seek_rsp_cb)(int Freq);
typedef void (*scan_rsp_cb)(void);
typedef void (*srch_list_rsp_cb)(uint16_t *scan_tbl);
typedef void (*stereo_mode_cb)(bool status);
typedef void (*rds_avl_sts_cb)(bool status);
typedef void (*af_list_cb)(uint16_t *af_list);
typedef void (*rt_cb)(char *rt);
typedef void (*ps_cb)(char *ps);
typedef void (*oda_cb)(void);
typedef void (*rt_plus_cb)(char *rt_plus);
typedef void (*ert_cb)(char *ert);
typedef void (*disable_cb)(void);
typedef void (*callback_thread_event)(unsigned int evt);
typedef void (*rds_grp_cntrs_cb)(char *rds_params);
typedef void (*rds_grp_cntrs_ext_cb)(char *rds_params);
typedef void (*fm_peek_cb)(char *peek_rsp);
typedef void (*fm_ssbi_peek_cb)(char *ssbi_peek_rsp);
typedef void (*fm_agc_gain_cb)(char *agc_gain_rsp);
typedef void (*fm_ch_det_th_cb)(char *ch_det_rsp);
typedef void (*fm_ecc_evt_cb)(char *ecc_rsp);
typedef void (*fm_sig_thr_cb) (int val, int status);
typedef void (*fm_get_ch_det_thrs_cb) (int val, int status);
typedef void (*fm_def_data_rd_cb) (int val, int status);
typedef void (*fm_get_blnd_cb) (int val, int status);
typedef void (*fm_set_ch_det_thrs_cb) (int status);
typedef void (*fm_def_data_wrt_cb) (int status);
typedef void (*fm_set_blnd_cb) (int status);
typedef void (*fm_get_stn_prm_cb) (int val, int status);
typedef void (*fm_get_stn_dbg_prm_cb) (int val, int status);
typedef void (*fm_enable_slimbus_cb) (int status);
typedef void (*fm_enable_softmute_cb) (int status);

typedef struct {
    size_t  size;

    enb_result_cb  enabled_cb;
    tune_rsp_cb tune_cb;
    seek_rsp_cb  seek_cmpl_cb;
    scan_rsp_cb  scan_next_cb;
    srch_list_rsp_cb  srch_list_cb;
    stereo_mode_cb  stereo_status_cb;
    rds_avl_sts_cb  rds_avail_status_cb;
    af_list_cb  af_list_update_cb;
    rt_cb  rt_update_cb;
    ps_cb  ps_update_cb;
    oda_cb  oda_update_cb;
    rt_plus_cb  rt_plus_update_cb;
    ert_cb  ert_update_cb;
    disable_cb  disabled_cb;
    rds_grp_cntrs_cb rds_grp_cntrs_rsp_cb;
    rds_grp_cntrs_ext_cb rds_grp_cntrs_ext_rsp_cb;
    fm_peek_cb fm_peek_rsp_cb;
    fm_ssbi_peek_cb fm_ssbi_peek_rsp_cb;
    fm_agc_gain_cb fm_agc_gain_rsp_cb;
    fm_ch_det_th_cb fm_ch_det_th_rsp_cb;
    fm_ecc_evt_cb  ext_country_code_cb;
    callback_thread_event thread_evt_cb;
    fm_sig_thr_cb fm_get_sig_thres_cb;
    fm_get_ch_det_thrs_cb fm_get_ch_det_thr_cb;
    fm_def_data_rd_cb fm_def_data_read_cb;
    fm_get_blnd_cb fm_get_blend_cb;
    fm_set_ch_det_thrs_cb fm_set_ch_det_thr_cb;
    fm_def_data_wrt_cb fm_def_data_write_cb;
    fm_set_blnd_cb fm_set_blend_cb;
    fm_get_stn_prm_cb fm_get_station_param_cb;
    fm_get_stn_dbg_prm_cb fm_get_station_debug_param_cb;
    fm_enable_slimbus_cb enable_slimbus_cb;
    fm_enable_softmute_cb enable_softmute_cb;
} fm_hal_callbacks_t;

struct fm_interface_t {
    int (*hal_init)(const fm_hal_callbacks_t *p_cb);
    int (*set_fm_ctrl)(int opcode, int val);
    int (*get_fm_ctrl) (int opcode, int *val);
};

class FmIoctlsInterface
{
    public:
        static bool is_hal_mode;
        static void *vendor_interface;
        static char g_ps_buffer[256];
        static int g_ps_len;
        static char g_rt_buffer[256];
        static int g_rt_len;
        static char g_station_list[256];
        static pthread_mutex_t rds_lock;

        static int start_fm_patch_dl(UINT fd);
        static int close_fm_patch_dl(void);
        static int get_cur_freq(UINT fd, long &freq);
        static int set_freq(UINT fd, ULINT freq);
        static int set_control(UINT fd, UINT id, int val);
        static int set_calibration(UINT fd);
        static int get_control(UINT fd, UINT id, long &val);
        static int start_search(UINT fd, UINT dir);
        static int set_band(UINT fd, ULINT low, ULINT high);
        static int get_upperband_limit(UINT fd, ULINT &freq);
        static int get_lowerband_limit(UINT fd, ULINT &freq);
        static int set_audio_mode(UINT fd, enum AUDIO_MODE mode);
        static int get_buffer(UINT fd, char *buff, UINT len, UINT index);
        static int get_rmssi(UINT fd, long &rmssi);
        static int set_ext_control(UINT fd, struct v4l2_ext_controls *v4l2_ctls);
};

#endif //__FM_IOCTL_INTERFACE_H__
