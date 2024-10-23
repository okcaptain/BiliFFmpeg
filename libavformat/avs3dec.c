/*
 * RAW AVS3-P2/IEEE1857.10 video demuxer
 * Copyright (c) 2020 Zhenyu Wang <wangzhenyu@pkusz.edu.cn>
 *                    Bingjie Han <hanbj@pkusz.edu.cn>
 *                    Huiwen Ren  <hwrenx@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "libavcodec/avs3.h"
#include "libavcodec/internal.h"
#include "avformat.h"
#include "rawdec.h"
#include "libavutil/avassert.h"
#include "libavutil/intreadwrite.h"

const uint8_t *avpriv_find_start_code(const uint8_t *av_restrict p,
                                      const uint8_t *end,
                                      uint32_t *av_restrict state)
{
    int i;

    av_assert0(p <= end);
    if (p >= end)
        return end;

    for (i = 0; i < 3; i++) {
        uint32_t tmp = *state << 8;
        *state = tmp + *(p++);
        if (tmp == 0x100 || p == end)
            return p;
    }

    while (p < end) {
        if      (p[-1] > 1      ) p += 3;
        else if (p[-2]          ) p += 2;
        else if (p[-3]|(p[-1]-1)) p++;
        else {
            p++;
            break;
        }
    }

    p = FFMIN(p, end) - 4;
    *state = AV_RB32(p);

    return p + 4;
}

static int avs3video_probe(const AVProbeData *p)
{
    const uint8_t *ptr = p->buf, *end = p->buf + p->buf_size;
    uint32_t code = -1;
    uint8_t state = 0;
    int pic = 0, seq = 0, slice_pos = 0;
    int ret = 0;

    while (ptr < end) {
        ptr = avpriv_find_start_code(ptr, end, &code);
        state = code & 0xFF;
        if ((code & 0xFFFFFF00) == 0x100) {
            if (state < AVS3_SEQ_START_CODE) {
                if (code < slice_pos)
                    return 0;
                slice_pos = code;
            } else {
                slice_pos = 0;
            }
            if (state == AVS3_SEQ_START_CODE) {
                seq++;
                if (*ptr != AVS3_PROFILE_BASELINE_MAIN && *ptr != AVS3_PROFILE_BASELINE_MAIN10)
                    return 0;
            } else if (AVS3_ISPIC(state)) {
                pic++;
            } else if ((state == AVS3_UNDEF_START_CODE) ||
                       (state > AVS3_VIDEO_EDIT_CODE)) {
                return 0;
            }
        }
    }

    if (seq && pic && av_match_ext(p->filename, "avs3")) {
        ret = AVPROBE_SCORE_MAX;
    }

    return ret;
}

FF_DEF_RAWVIDEO_DEMUXER(avs3, "raw AVS3-P2/IEEE1857.10", avs3video_probe, "avs3", AV_CODEC_ID_AVS3)
