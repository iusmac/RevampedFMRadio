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

/*******************************************************************
 * FM JNI core
 * return -1 if error occured. else return needed value.
 * if return type is char *, return NULL if error occured.
 * Do NOT return value access paramater.
 *
 * FM JNI core should be independent from lower layer, that means
 * there should be no low layer dependent data struct in FM JNI core
 *
 * Naming rule: FMR_n(paramter Micro), FMR_v(functions), fmr_n(global param)
 * pfmr_n(global paramter pointer)
 *
 *******************************************************************/

#include "fmr.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "FMLIB_CORE"

#define FMR_MAX_IDX 1

struct fmr_ds fmr_data;
struct fmr_ds *pfmr_data[FMR_MAX_IDX] = {0};
#define FMR_fd(idx) ((pfmr_data[idx])->fd)
#define FMR_err(idx) ((pfmr_data[idx])->err)
#define FMR_chip(idx) ((pfmr_data[idx])->cfg_data.chip)
#define FMR_low_band(idx) ((pfmr_data[idx])->cfg_data.low_band)
#define FMR_high_band(idx) ((pfmr_data[idx])->cfg_data.high_band)
#define FMR_seek_space(idx) ((pfmr_data[idx])->cfg_data.seek_space)
#define FMR_max_scan_num(idx) ((pfmr_data[idx])->cfg_data.max_scan_num)
#define FMR_cbk_tbl(idx) ((pfmr_data[idx])->tbl)
#define FMR_cust_hdler(idx) ((pfmr_data[idx])->custom_handler)
#define FMR_get_cfg(idx) ((pfmr_data[idx])->get_cfg)

int FMR_get_cfgs(int idx)
{
    int ret = -1;
    FMR_cust_hdler(idx) = NULL;
    FMR_get_cfg(idx) = NULL;

    FMR_cust_hdler(idx) = dlopen(CUST_LIB_NAME, RTLD_NOW);
    if (FMR_cust_hdler(idx) == NULL) {
        LOGE("%s failed, %s\n", __FUNCTION__, dlerror());
        //FMR_seterr(ERR_LD_LIB);
    } else {
        *(void **) (&FMR_get_cfg(idx)) = dlsym(FMR_cust_hdler(idx), "CUST_get_cfg");
        if (FMR_get_cfg(idx) == NULL) {
            LOGE("%s failed, %s\n", __FUNCTION__, dlerror());
            //FMR_seterr(ERR_FIND_CUST_FNUC);
        } else {
            LOGI("Go to run cust function\n");
            (*FMR_get_cfg(idx))(&(pfmr_data[idx]->cfg_data));
            LOGI("OK\n");
            ret = 0;
        }
        //dlclose(FMR_cust_hdler(idx));
        FMR_cust_hdler(idx) = NULL;
        FMR_get_cfg(idx) = NULL;
    }
    LOGI("%s successfully. chip: 0x%x, lband: %d, hband: %d, seek_space: %d, max_scan_num: %d\n", __FUNCTION__, FMR_chip(idx), FMR_low_band(idx), FMR_high_band(idx), FMR_seek_space(idx), FMR_max_scan_num(idx));

    return ret;
}

int FMR_chk_cfg_data(int idx)
{
    //TODO Need check? how to check?
    return 0;
}

static void sig_alarm(int sig)
{
    LOGI("+++Receive sig %d\n", sig);
    return;
}

int FMR_init()
{
    int idx;
    int ret = 0;
    //signal(4, sig_alarm);

    for (idx=0; idx<FMR_MAX_IDX; idx++) {
        if (pfmr_data[idx] == NULL) {
            break;
        }
    }
    LOGI("FMR idx = %d\n", idx);
    if (idx == FMR_MAX_IDX) {
        //FMR_seterr(ERR_NO_MORE_IDX);
        return -1;
    }

    /*The best way here is to call malloc to alloc mem for each idx,but
    I do not know where to release it, so use static global param instead*/
    pfmr_data[idx] = &fmr_data;
    memset(pfmr_data[idx], 0, sizeof(struct fmr_ds));

    if (FMR_get_cfgs(idx) < 0) {
        LOGI("FMR_get_cfgs failed\n");
        goto fail;
    }

    if (FMR_chk_cfg_data(idx) < 0) {
        LOGI("FMR_chk_cfg_data failed\n");
        goto fail;
    }

    pfmr_data[idx]->init_func = FM_interface_init;
    if (pfmr_data[idx]->init_func == NULL) {
        LOGE("%s init_func error, %s\n", __func__, dlerror());
        goto fail;
    } else {
        LOGI("Go to run init function\n");
        (*pfmr_data[idx]->init_func)(&(pfmr_data[idx]->tbl));
        LOGI("OK\n");
        ret = 0;
    }

    return idx;

fail:
    pfmr_data[idx] = NULL;

    return -1;
}

int FMR_open_dev(int idx)
{
    int ret = 0;
    int real_chip;

    FMR_ASSERT(FMR_cbk_tbl(idx).open_dev);

    ret = FMR_cbk_tbl(idx).open_dev(FM_DEV_NAME, &FMR_fd(idx));
    if (ret || FMR_fd(idx) < 0) {
        LOGE("%s failed, [fd=%d]\n", __func__, FMR_fd(idx));
        return ret;
    }

    //Check if customer's cfg matchs driver.
    ret = FMR_get_chip_id(idx, &real_chip);
    if (FMR_chip(idx) != real_chip) {
        LOGE("%s, Chip config error. 0x%x\n", __func__, real_chip);
        ret = FMR_cbk_tbl(idx).close_dev(FMR_fd(idx));
        return ret;
    }

    LOGD("%s, [fd=%d] [chipid=0x%x] [ret=%d]\n", __func__, FMR_fd(idx), real_chip, ret);
    return ret;
}

int FMR_close_dev(int idx)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).close_dev);
    ret = FMR_cbk_tbl(idx).close_dev(FMR_fd(idx));
    LOGD("%s, [fd=%d] [ret=%d]\n", __func__, FMR_fd(idx), ret);
    return ret;
}

int FMR_pwr_up(int idx, int freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).pwr_up);

    LOGI("%s,[freq=%d]\n", __func__, freq);
    if (freq < fmr_data.cfg_data.low_band || freq > fmr_data.cfg_data.high_band) {
        LOGE("%s error freq: %d\n", __func__, freq);
        ret = -ERR_INVALID_PARA;
        return ret;
    }
    ret = FMR_cbk_tbl(idx).pwr_up(FMR_fd(idx), fmr_data.cfg_data.band, freq);
    if (ret) {
        LOGE("%s failed, [ret=%d]\n", __func__, ret);
    }
    fmr_data.cur_freq = freq;
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_pwr_down(int idx, int type)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).pwr_down);
    ret = FMR_cbk_tbl(idx).pwr_down(FMR_fd(idx), type);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_chip_id(int idx, int *chipid)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_chip_id);
    FMR_ASSERT(chipid);

    ret = FMR_cbk_tbl(idx).get_chip_id(FMR_fd(idx), chipid);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
        *chipid = -1;
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_ps(int idx, uint8_t **ps, int *ps_len)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_ps);
    FMR_ASSERT(ps);
    FMR_ASSERT(ps_len);
    ret = FMR_cbk_tbl(idx).get_ps(FMR_fd(idx), &fmr_data.rds, ps, ps_len);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_get_rt(int idx, uint8_t **rt, int *rt_len)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).get_rt);
    FMR_ASSERT(rt);
    FMR_ASSERT(rt_len);

    ret = FMR_cbk_tbl(idx).get_rt(FMR_fd(idx), &fmr_data.rds, rt, rt_len);
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_tune(int idx, int freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).tune);

    ret = FMR_cbk_tbl(idx).tune(FMR_fd(idx), freq, fmr_data.cfg_data.band);
    if (ret) {
        LOGE("%s failed, [ret=%d]\n", __func__, ret);
    }
    fmr_data.cur_freq = freq;
    LOGD("%s, [freq=%d] [ret=%d]\n", __func__, freq, ret);
    return ret;
}

/*return: fm_true: desense, fm_false: not desene channel*/
fm_bool FMR_DensenseDetect(fm_s32 idx, fm_u16 ChannelNo, fm_s32 RSSI)
{
    fm_u8 bDesenseCh = 0;

    bDesenseCh = FMR_cbk_tbl(idx).desense_check(FMR_fd(idx), ChannelNo, RSSI);
    if (bDesenseCh == 1) {
        return fm_true;
    }
    return fm_false;
}

fm_bool FMR_SevereDensense(fm_u16 ChannelNo, fm_s32 RSSI)
{
    fm_s32 i = 0, j = 0;
    struct fm_fake_channel_t *chan_info = fmr_data.cfg_data.fake_chan;

    ChannelNo /= 10;

    for (i=0; i<chan_info->size; i++) {
        if (ChannelNo == chan_info->chan[i].freq) {
            //if (RSSI < FM_SEVERE_RSSI_TH)
            if (RSSI < chan_info->chan[i].rssi_th) {
                LOGI(" SevereDensense[%d] RSSI[%d]\n", ChannelNo,RSSI);
                return fm_true;
            } else {
                break;
            }
        }
    }
    return fm_false;
}
#if (FMR_NOISE_FLOORT_DETECT==1)
/*return TRUE:get noise floor freq*/
fm_bool FMR_NoiseFloorDetect(fm_bool *rF, fm_s32 rssi, fm_s32 *F_rssi)
{
    if (rF[0] == fm_true) {
        if (rF[1] == fm_true) {
            F_rssi[2] = rssi;
            rF[2] = fm_true;
            return fm_true;
        } else {
            F_rssi[1] = rssi;
            rF[1] = fm_true;
        }
    } else {
        F_rssi[0] = rssi;
        rF[0] = fm_true;
    }
    return fm_false;
}
#endif

/*check the cur_freq->freq is valid or not
return fm_true : need check cur_freq->valid
         fm_false: check faild, should stop seek
*/
static fm_bool FMR_Seek_TuneCheck(int idx, fm_softmute_tune_t *cur_freq)
{
    int ret = 0;
    if (fmr_data.scan_stop == fm_true) {
        ret = FMR_tune(idx,fmr_data.cur_freq);
        LOGI("seek stop!!! tune ret=%d",ret);
        return fm_false;
    }
    ret = FMR_cbk_tbl(idx).soft_mute_tune(FMR_fd(idx), cur_freq);
    if (ret) {
        LOGE("soft mute tune, failed:[%d]\n",ret);
        cur_freq->valid = fm_false;
        return fm_true;
    }
    if (cur_freq->valid == fm_true)/*get valid channel*/ {
        if (FMR_DensenseDetect(idx, cur_freq->freq, cur_freq->rssi) == fm_true) {
            LOGI("desense channel detected:[%d] \n", cur_freq->freq);
            cur_freq->valid = fm_false;
            return fm_true;
        }
        if (FMR_SevereDensense(cur_freq->freq, cur_freq->rssi) == fm_true) {
            LOGI("sever desense channel detected:[%d] \n", cur_freq->freq);
            cur_freq->valid = fm_false;
            return fm_true;
        }
        LOGI("seek result freq:[%d] \n", cur_freq->freq);
    }
    return fm_true;
}
/*
check more 2 freq, curfreq: current freq, seek_dir: 1,forward. 0,backword
*/
static int FMR_Seek_More(int idx, fm_softmute_tune_t *validfreq, fm_u8 seek_dir, fm_u8 step, fm_u16 min_freq, fm_u16 max_freq)
{
    fm_s32 i;
    fm_softmute_tune_t cur_freq;
    cur_freq.freq = validfreq->freq;

    for (i=0; i<2; i++) {
        if (seek_dir)/*forward*/ {
            if (cur_freq.freq + step > max_freq) {
                return 0;
            }
            cur_freq.freq += step;
        } else/*backward*/ {
            if (cur_freq.freq - step < min_freq) {
                return 0;
            }
            cur_freq.freq -= step;
        }
        if (FMR_Seek_TuneCheck(idx, &cur_freq) == fm_true) {
            if (cur_freq.valid == fm_true) {
                if (cur_freq.rssi > validfreq->rssi) {
                    validfreq->freq = cur_freq.freq;
                    validfreq->rssi = cur_freq.rssi;
                    LOGI("seek cover last by freq=%d",cur_freq.freq);
                }
            }
        } else {
            return -1;
        }
    }
    return 0;
}

/*check the a valid channel
return -1 : seek fail
         0: seek success
*/
int FMR_seek_Channel(int idx, int start_freq, int min_freq, int max_freq, int band_channel_no, int seek_space, int dir, int *ret_freq, int *rssi_tmp)
{
    fm_s32 i, ret = 0;
    fm_softmute_tune_t cur_freq;

    if (dir == 1)/*forward*/ {
        for (i=((start_freq-min_freq)/seek_space+1); i<band_channel_no; i++) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----1",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid == fm_false) {
                    continue;
                } else {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
        for (i=0; i<((start_freq-min_freq)/seek_space); i++) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----2",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid == fm_false) {
                    continue;
                } else {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
    } else/*backward*/ {
        for (i=((start_freq-min_freq)/seek_space-1); i>=0; i--) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----3",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid == fm_false) {
                    continue;
                } else {
                    if (FMR_Seek_More(idx, &cur_freq, dir, seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
        for (i=(band_channel_no-1); i>((start_freq-min_freq)/seek_space); i--) {
            cur_freq.freq = min_freq + seek_space*i;
            LOGI("i=%d, freq=%d-----4",i,cur_freq.freq);
            ret = FMR_Seek_TuneCheck(idx, &cur_freq);
            if (ret == fm_false) {
                return -1;
            } else {
                if (cur_freq.valid == fm_false) {
                    continue;
                } else {
                    if (FMR_Seek_More(idx, &cur_freq, dir,seek_space, min_freq, max_freq) == 0) {
                        *ret_freq = cur_freq.freq;
                        *rssi_tmp = cur_freq.rssi;
                        return 0;
                    } else {
                        return -1;
                    }
                }
            }
        }
    }

    *ret_freq = start_freq;
    return 0;
}

int FMR_seek(int idx, int start_freq, int dir, int *ret_freq)
{
    fm_s32 ret = 0, i, j;
    fm_softmute_tune_t cur_freq;
    fm_s32 band_channel_no = 0;
    fm_u8 seek_space = 10;
    fm_u16 min_freq, max_freq;
    int rssi;

    if ((start_freq < 7600) || (start_freq > 10800)) /*need replace by macro*/ {
        LOGE("%s error start_freq: %d\n", __func__, start_freq);
        return -ERR_INVALID_PARA;
    }

    //FM radio seek space,5:50KHZ; 1:100KHZ; 2:200KHZ
    if (fmr_data.cfg_data.seek_space == 5) {
        seek_space = 5;
    } else if (fmr_data.cfg_data.seek_space == 2) {
        seek_space = 20;
    } else {
        seek_space = 10;
    }
    if (fmr_data.cfg_data.band == FM_BAND_JAPAN)/* Japan band	   76MHz ~ 90MHz */ {
        band_channel_no = (9600-7600)/seek_space + 1;
        min_freq = 7600;
        max_freq = 9600;
    } else if (fmr_data.cfg_data.band == FM_BAND_JAPANW)/* Japan wideband  76MHz ~ 108MHz */ {
        band_channel_no = (10800-7600)/seek_space + 1;
        min_freq = 7600;
        max_freq = 10800;
    } else/* US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */ {
        band_channel_no = (10800-8750)/seek_space + 1;
        min_freq = 8750;
        max_freq = 10800;
    }

    fmr_data.scan_stop = fm_false;
    LOGD("seek start freq %d band_channel_no=[%d], seek_space=%d band[%d - %d] dir=%d\n", start_freq, band_channel_no,seek_space,min_freq,max_freq,dir);

    ret = FMR_seek_Channel(idx, start_freq, min_freq, max_freq, band_channel_no, seek_space, dir, ret_freq, &rssi);

    return ret;
}

int FMR_set_mute(int idx, int mute)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).set_mute)

    if ((mute < 0) || (mute > 1)) {
        LOGE("%s error param mute:  %d\n", __func__, mute);
    }

    ret = FMR_cbk_tbl(idx).set_mute(FMR_fd(idx), mute);
    if (ret) {
        LOGE("%s failed, %s\n", __func__, FMR_strerr());
    }
    LOGD("%s, [mute=%d] [ret=%d]\n", __func__, mute, ret);
    return ret;
}

int FMR_is_rdsrx_support(int idx, int *supt)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).is_rdsrx_support);
    FMR_ASSERT(supt);

    ret = FMR_cbk_tbl(idx).is_rdsrx_support(FMR_fd(idx), supt);
    if (ret) {
        *supt = 0;
        LOGE("%s, failed\n", __func__);
    }
    LOGD("%s, [supt=%d] [ret=%d]\n", __func__, *supt, ret);
    return ret;
}

int FMR_Pre_Search(int idx)
{
    //avoid scan stop flag clear if stop cmd send before pre-search finish
    fmr_data.scan_stop = fm_false;
    FMR_ASSERT(FMR_cbk_tbl(idx).pre_search);
    FMR_cbk_tbl(idx).pre_search(FMR_fd(idx));
    return 0;
}

int FMR_Restore_Search(int idx)
{
    FMR_ASSERT(FMR_cbk_tbl(idx).restore_search);
    FMR_cbk_tbl(idx).restore_search(FMR_fd(idx));
    return 0;
}

int FMR_scan_Channels(int idx, uint16_t *scan_tbl, int *max_cnt, fm_s32 band_channel_no, fm_u16 Start_Freq, fm_u8 seek_space, fm_u8 NF_Space)
{
    fm_s32 ret = 0, Num = 0, i, j;
    fm_u32 ChannelNo = 0;
    fm_softmute_tune_t cur_freq;
    static struct fm_cqi SortData[CQI_CH_NUM_MAX];
    fm_bool LastExist = fm_false;
    struct fm_cqi swap;
#if (FMR_NOISE_FLOORT_DETECT==1)
    fm_s32 Pacc = 0, Nacc = 0;
    fm_s32 NF = 0;
    fm_bool F[3] = {fm_false, fm_false, fm_false};
    fm_s32 F_Rssi[3] = {0};
    fm_u8 NF_Idx = 0;
#endif

    memset(SortData, 0, CQI_CH_NUM_MAX*sizeof(struct fm_cqi));
    LOGI("band_channel_no=[%d], seek_space=%d, start freq=%d\n", band_channel_no,seek_space,Start_Freq);
    for (i=0; i<band_channel_no; i++) {
        if (fmr_data.scan_stop == fm_true) {
            FMR_Restore_Search(idx);
            ret = FMR_tune(idx, fmr_data.cur_freq);
            LOGI("scan stop!!! tune ret=%d",ret);
            return -1;
        }
        cur_freq.freq = Start_Freq + seek_space*i;
        ret = FMR_cbk_tbl(idx).soft_mute_tune(FMR_fd(idx), &cur_freq);
        if (ret) {
            LOGE("soft mute tune, failed:[%d]\n",ret);
            LastExist = fm_false;
            continue;
        }
        if (cur_freq.valid == fm_true)/*get valid channel*/ {
#if (FMR_NOISE_FLOORT_DETECT==1)
            memset(F, fm_false, sizeof(F));
#endif
            if (FMR_DensenseDetect(idx, cur_freq.freq, cur_freq.rssi) == fm_true) {
                LOGI("desense channel detected:[%d] \n", cur_freq.freq);
                LastExist = fm_false;
                continue;
            }
            if ((LastExist == fm_true) && (Num > 0)) /*neighbor channel*/ {
                if (cur_freq.rssi>SortData[Num-1].rssi)/*save current freq and cover last channel*/ {
                    if (FMR_SevereDensense(cur_freq.freq, cur_freq.rssi) == fm_true) {
                        LastExist = fm_false;
                        continue;
                    }
                    SortData[Num-1].ch=cur_freq.freq;
                    SortData[Num-1].rssi=cur_freq.rssi;
                    SortData[Num-1].reserve = 1;
                    LOGI("cover last channel \n");
                } else/*ignore current freq*/ {
                    LastExist = fm_false;
                    continue;
                }
            } else/*save current*/ {
                if (FMR_SevereDensense(cur_freq.freq, cur_freq.rssi) == fm_true) {
                    LastExist = fm_false;
                    continue;
                }
                SortData[Num].ch = cur_freq.freq;
                SortData[Num].rssi = cur_freq.rssi;
                SortData[Num].reserve = 1;
                Num++;
                LastExist = fm_true;
                LOGI("Num++:[%d] \n", Num);
            }
        } else {
#if (FMR_NOISE_FLOORT_DETECT==1)
            if (FMR_DensenseDetect(idx, cur_freq.freq, cur_freq.rssi) == fm_false) {
                if (FMR_NoiseFloorDetect(F, cur_freq.rssi, F_Rssi) == fm_true) {
                    Pacc += F_Rssi[1];
                    Nacc++;
                    /*check next freq*/
                    F[0] = F[1];
                    F_Rssi[0] = F_Rssi[1];
                    F[1] = F[2];
                    F_Rssi[1] = F_Rssi[2];
                    F[2] = fm_false;
                    F_Rssi[2] = 0;
                    LOGI("FM Noise FLoor:Pacc=[%d] Nacc=[%d] \n", Pacc,Nacc);
                }
            } else {
                memset(F, fm_false, sizeof(F));
            }
#endif
            LastExist = fm_false;
        }
#if (FMR_NOISE_FLOORT_DETECT==1)
        if (((i%NF_Space) == 0) && (i != 0)) {
            if (Nacc > 0) {
                NF = Pacc/Nacc;
            } else {
                NF = RSSI_TH-FM_NOISE_FLOOR_OFFSET;
            }
            Pacc = 0;
            Nacc = 0;
            for (j=NF_Idx; j<Num; j++) {
                if (SortData[j].rssi < (NF+FM_NOISE_FLOOR_OFFSET)) {
                    LOGI("FM Noise FLoor Detected:freq=[%d] NF=[%d] \n", SortData[j].ch,NF);
                    SortData[j].reserve = 0;
                }
            }
            NF_Idx = j;
            LOGI("FM Noise FLoor NF_Idx[%d] \n", NF_Idx);
        }
#endif
    }
    LOGI("get channel no.[%d] \n", Num);
    if (Num == 0)/*get nothing*/ {
        *max_cnt = 0;
        FMR_Restore_Search(idx);
        return -1;
    }
    for (i=0; i<Num; i++)/*debug*/ {
        LOGI("[%d]:%d \n", i,SortData[i].ch);
    }

    switch (fmr_data.cfg_data.scan_sort)
    {
        case FM_SCAN_SORT_UP:
        case FM_SCAN_SORT_DOWN:
            {
                LOGI("Start sort \n");
                //do sort: insert sort algorithm
                for (i = 1; i < Num; i++) {
                    for (j = i; (j > 0) && ((FM_SCAN_SORT_DOWN == fmr_data.cfg_data.scan_sort) ? (SortData[j-1].rssi \
                        < SortData[j].rssi) : (SortData[j-1].rssi > SortData[j].rssi)); j--) {
                        memcpy(&swap, &SortData[j], sizeof(struct fm_cqi));
                        memcpy(&SortData[j], &SortData[j-1], sizeof(struct fm_cqi));
                        memcpy(&SortData[j-1], &swap, sizeof(struct fm_cqi));
                    }
                }
                LOGI("End sort \n");
                break;
            }
        default:
            break;
    }

    ChannelNo = 0;
    for (i=0; i<Num; i++) {
        if (SortData[i].reserve == 1) {
            SortData[i].ch /= 10;

            scan_tbl[ChannelNo]=SortData[i].ch;
            ChannelNo++;
        }
    }
    *max_cnt=ChannelNo;

    LOGI("return channel no.[%d] \n", ChannelNo);
    return 0;
}

int FMR_scan(int idx, uint16_t *scan_tbl, int *max_cnt)
{
    fm_s32 ret = 0;
    fm_s32 band_channel_no = 0;
    fm_u8 seek_space = 10;
    fm_u16 Start_Freq = 8750;
    fm_u8 NF_Space = 41;

    //FM radio seek space,5:50KHZ; 1:100KHZ; 2:200KHZ
    if (fmr_data.cfg_data.seek_space == 5) {
        seek_space = 5;
    } else if (fmr_data.cfg_data.seek_space == 2) {
        seek_space = 20;
    } else {
        seek_space = 10;
    }
    if (fmr_data.cfg_data.band == FM_BAND_JAPAN)/* Japan band      76MHz ~ 90MHz */ {
        band_channel_no = (9600-7600)/seek_space + 1;
        Start_Freq = 7600;
        NF_Space = 400/seek_space;
    } else if (fmr_data.cfg_data.band == FM_BAND_JAPANW)/* Japan wideband  76MHZ ~ 108MHz */ {
        band_channel_no = (10800-7600)/seek_space + 1;
        Start_Freq = 7600;
        NF_Space = 640/seek_space;
    } else/* US/Europe band  87.5MHz ~ 108MHz (DEFAULT) */ {
        band_channel_no = (10800-8750)/seek_space + 1;
        Start_Freq = 8750;
        NF_Space = 410/seek_space;
    }

    ret = FMR_scan_Channels(idx, scan_tbl, max_cnt, band_channel_no, Start_Freq, seek_space, NF_Space);

    return ret;
}

int FMR_stop_scan(int idx)
{
    fmr_data.scan_stop = fm_true;
    return 0;
}

int FMR_turn_on_off_rds(int idx, int onoff)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).turn_on_off_rds)
    ret = FMR_cbk_tbl(idx).turn_on_off_rds(FMR_fd(idx), onoff);
    if (ret) {
        LOGE("%s, failed\n", __func__);
    }
    LOGD("%s, [onoff=%d] [ret=%d]\n", __func__, onoff, ret);
    return ret;
}

int FMR_read_rds_data(int idx, uint16_t *rds_status)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).read_rds_data);
    FMR_ASSERT(rds_status);

    ret = FMR_cbk_tbl(idx).read_rds_data(FMR_fd(idx), &fmr_data.rds, rds_status);
    /*if (ret) {
        LOGE("%s, get no event\n", __func__);
    }*/
    LOGD("%s, [status=%d] [ret=%d]\n", __func__, *rds_status, ret);
    return ret;
}

int FMR_active_af(int idx, uint16_t *ret_freq)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).active_af);
    FMR_ASSERT(ret_freq);
    ret = FMR_cbk_tbl(idx).active_af(FMR_fd(idx),
                                    &fmr_data.rds,
                                    fmr_data.cfg_data.band,
                                    fmr_data.cur_freq,
                                    ret_freq);
    if ((ret == 0) && (*ret_freq != fmr_data.cur_freq)) {
        fmr_data.cur_freq = *ret_freq;
        LOGI("active AF OK, new channel[freq=%d]\n", fmr_data.cur_freq);
    }
    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

int FMR_ana_switch(int idx, int antenna)
{
    int ret = 0;

    FMR_ASSERT(FMR_cbk_tbl(idx).ana_switch);

    if (fmr_data.cfg_data.short_ana_sup == true) {
        ret = FMR_cbk_tbl(idx).ana_switch(FMR_fd(idx), antenna);
        if (ret) {
            LOGE("%s failed, [ret=%d]\n", __func__, ret);
        }
    } else {
        LOGW("FM antenna switch not support!\n");
        ret = -ERR_UNSUPT_SHORTANA;
    }

    LOGD("%s, [ret=%d]\n", __func__, ret);
    return ret;
}

