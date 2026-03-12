/*
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

#include "libavutil/adler32.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int Test_open_output_file(const char *output_filename)
{
    printf("[DEBUG] Test_open_output_file: START with filename='%s'\n", output_filename);
    
    struct ffmpeg_mux {
        struct {
            char file[256];
        } params;
        AVFormatContext *output;
        AVStream *video_stream;
    } *ffm = NULL;
    const AVOutputFormat *format = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVCodecContext *codec_ctx2 = NULL;
    AVDictionary *dict = NULL;
    AVDictionaryEntry *entry = NULL;
    AVPacket pkt;
    char error_buf[256];
    void *dup_data = NULL;
    int dict_count = 0;
    int ret = 0;
    
    printf("[DEBUG] Allocating ffmpeg_mux structure\n");
    ffm = (struct ffmpeg_mux *)calloc(1, sizeof(struct ffmpeg_mux));
    if (!ffm) {
        printf("[DEBUG] Failed to allocate ffmpeg_mux\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate ffmpeg_mux\n");
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully allocated ffmpeg_mux at %p\n", (void*)ffm);
    
    snprintf(ffm->params.file, sizeof(ffm->params.file), "%s", output_filename);
    printf("[DEBUG] Set output filename to: '%s'\n", ffm->params.file);
    
    printf("[DEBUG] Calling avformat_network_init()\n");
    avformat_network_init();
    printf("[DEBUG] avformat_network_init() completed\n");
    
    printf("[DEBUG] Calling av_guess_format() with filename='%s'\n", ffm->params.file);
    format = av_guess_format(NULL, ffm->params.file, NULL);
    printf("[DEBUG] av_guess_format() returned format=%p\n", (void*)format);
    
    printf("[DEBUG] Calling avformat_alloc_output_context2()\n");
    ret = avformat_alloc_output_context2(&ffm->output, format, NULL, ffm->params.file);
    printf("[DEBUG] avformat_alloc_output_context2() returned ret=%d, output=%p\n", ret, (void*)ffm->output);
    if (ret < 0) {
        printf("[DEBUG] avformat_alloc_output_context2() failed with ret=%d\n", ret);
        av_log(NULL, AV_LOG_ERROR, "Can't allocate output context\n");
        free(ffm);
        return ret;
    }
    
    if (!ffm->output) {
        printf("[DEBUG] Output context is NULL after allocation\n");
        av_log(NULL, AV_LOG_ERROR, "Output context is NULL\n");
        free(ffm);
        return -1;
    }
    printf("[DEBUG] Output context successfully allocated\n");
    
    printf("[DEBUG] Calling avformat_new_stream()\n");
    ffm->video_stream = avformat_new_stream(ffm->output, NULL);
    printf("[DEBUG] avformat_new_stream() returned video_stream=%p\n", (void*)ffm->video_stream);
    if (!ffm->video_stream) {
        printf("[DEBUG] Failed to create video stream\n");
        av_log(NULL, AV_LOG_ERROR, "Can't create video stream\n");
        avformat_free_context(ffm->output);
        free(ffm);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Video stream successfully created\n");
    
    printf("[DEBUG] Setting video stream parameters\n");
    ffm->video_stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    ffm->video_stream->codecpar->codec_id = AV_CODEC_ID_H264;
    ffm->video_stream->codecpar->width = 1920;
    ffm->video_stream->codecpar->height = 1080;
    ffm->video_stream->codecpar->format = AV_PIX_FMT_YUV420P;
    ffm->video_stream->codecpar->bit_rate = 400000;
    ffm->video_stream->time_base = (AVRational){1, 25};
    printf("[DEBUG] Video stream parameters set: codec_type=%d, codec_id=%d, width=%d, height=%d\n",
           ffm->video_stream->codecpar->codec_type,
           ffm->video_stream->codecpar->codec_id,
           ffm->video_stream->codecpar->width,
           ffm->video_stream->codecpar->height);
    
    printf("[DEBUG] Calling av_memdup() for 'test'\n");
    dup_data = av_memdup("test", 4);
    printf("[DEBUG] av_memdup() returned dup_data=%p\n", dup_data);
    if (!dup_data) {
        printf("[DEBUG] Failed to allocate dup_data\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate dup_data\n");
        avformat_free_context(ffm->output);
        free(ffm);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] dup_data successfully allocated\n");
    
    printf("[DEBUG] Calling avcodec_alloc_context3() for codec_ctx\n");
    codec_ctx = avcodec_alloc_context3(NULL);
    printf("[DEBUG] avcodec_alloc_context3() returned codec_ctx=%p\n", (void*)codec_ctx);
    if (!codec_ctx) {
        printf("[DEBUG] Failed to allocate codec context\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate codec context\n");
        avformat_free_context(ffm->output);
        free(ffm);
        av_free(dup_data);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] codec_ctx successfully allocated\n");
    
    /* Set codec_ctx parameters to match the stream */
    codec_ctx->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx->codec_id = AV_CODEC_ID_H264;
    codec_ctx->width = 1920;
    codec_ctx->height = 1080;
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 400000;
    codec_ctx->time_base = (AVRational){1, 25};
    
    printf("[DEBUG] Checking if output and nb_streams > 0: output=%p, nb_streams=%u\n",
           (void*)ffm->output, ffm->output ? ffm->output->nb_streams : 0);
    if (ffm->output && ffm->output->nb_streams > 0) {
        printf("[DEBUG] Calling avcodec_parameters_from_context() for stream 0\n");
        ret = avcodec_parameters_from_context(ffm->output->streams[0]->codecpar, codec_ctx);
        printf("[DEBUG] avcodec_parameters_from_context() returned ret=%d\n", ret);
    } else {
        printf("[DEBUG] Skipping avcodec_parameters_from_context() - condition not met\n");
    }
    
    printf("[DEBUG] Calling av_dict_set() with key='key', value='value'\n");
    av_dict_set(&dict, "key", "value", 0);
    printf("[DEBUG] av_dict_set() completed, dict=%p\n", (void*)dict);
    
    printf("[DEBUG] Freeing dup_data\n");
    av_free(dup_data);
    printf("[DEBUG] Calling av_memdup() for 'test2'\n");
    dup_data = av_memdup("test2", 5);
    printf("[DEBUG] av_memdup() returned dup_data=%p\n", dup_data);
    if (!dup_data) {
        printf("[DEBUG] Failed to reallocate dup_data\n");
        av_log(NULL, AV_LOG_ERROR, "Can't reallocate dup_data\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(ffm->output);
        free(ffm);
        av_dict_free(&dict);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] dup_data successfully reallocated\n");
    
    printf("[DEBUG] Calling avcodec_alloc_context3() for codec_ctx2\n");
    codec_ctx2 = avcodec_alloc_context3(NULL);
    printf("[DEBUG] avcodec_alloc_context3() returned codec_ctx2=%p\n", (void*)codec_ctx2);
    if (!codec_ctx2) {
        printf("[DEBUG] Failed to allocate second codec context\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate second codec context\n");
        avcodec_free_context(&codec_ctx);
        avformat_free_context(ffm->output);
        free(ffm);
        av_free(dup_data);
        av_dict_free(&dict);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] codec_ctx2 successfully allocated\n");
    
    /* Set codec_ctx2 parameters to match the stream */
    codec_ctx2->codec_type = AVMEDIA_TYPE_VIDEO;
    codec_ctx2->codec_id = AV_CODEC_ID_H264;
    codec_ctx2->width = 1920;
    codec_ctx2->height = 1080;
    codec_ctx2->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx2->bit_rate = 400000;
    codec_ctx2->time_base = (AVRational){1, 25};
    
    printf("[DEBUG] Checking if output and nb_streams > 0 for codec_ctx2: output=%p, nb_streams=%u\n",
           (void*)ffm->output, ffm->output ? ffm->output->nb_streams : 0);
    if (ffm->output && ffm->output->nb_streams > 0) {
        printf("[DEBUG] Calling avcodec_parameters_from_context() for stream 0 with codec_ctx2\n");
        ret = avcodec_parameters_from_context(ffm->output->streams[0]->codecpar, codec_ctx2);
        printf("[DEBUG] avcodec_parameters_from_context() returned ret=%d\n", ret);
    } else {
        printf("[DEBUG] Skipping avcodec_parameters_from_context() - condition not met\n");
    }
    
    printf("[DEBUG] Checking if ffm and ffm->output are valid: ffm=%p, output=%p\n",
           (void*)ffm, ffm ? (void*)ffm->output : NULL);
    if (ffm && ffm->output) {
        format = ffm->output->oformat;
        printf("[DEBUG] Got oformat=%p\n", (void*)format);
        printf("[DEBUG] Checking if format is valid and AVFMT_NOFILE flag: format=%p, flags=%d\n",
               (void*)format, format ? format->flags : 0);
        if (format && (format->flags & AVFMT_NOFILE) == 0) {
            printf("[DEBUG] Calling avio_open() with file='%s', flags=AVIO_FLAG_WRITE\n", ffm->params.file);
            ret = avio_open(&ffm->output->pb, ffm->params.file, AVIO_FLAG_WRITE);
            printf("[DEBUG] avio_open() returned ret=%d, pb=%p\n", ret, (void*)ffm->output->pb);
            
            if (ret < 0) {
                printf("[DEBUG] avio_open() failed with ret=%d\n", ret);
                av_log(NULL, AV_LOG_ERROR, "Can't open output file\n");
                
                if (ffm->output->pb != NULL) {
                    printf("[DEBUG] ERROR: avio_open failed but pb was not set to NULL (pb=%p)\n",
                           (void*)ffm->output->pb);
                    av_log(NULL, AV_LOG_ERROR, "avio_open failed but pb was not set to NULL\n");
                    av_dict_free(&dict);
                    avcodec_free_context(&codec_ctx);
                    avcodec_free_context(&codec_ctx2);
                    avformat_free_context(ffm->output);
                    av_free(dup_data);
                    free(ffm);
                    return -1;
                } else {
                    printf("[DEBUG] pb is NULL as expected after avio_open failure\n");
                }
                
                av_dict_free(&dict);
                avcodec_free_context(&codec_ctx);
                avcodec_free_context(&codec_ctx2);
                avformat_free_context(ffm->output);
                av_free(dup_data);
                free(ffm);
                return ret;
            } else {
                printf("[DEBUG] avio_open() succeeded with ret=%d\n", ret);
                if (ret < 0) {
                    printf("[DEBUG] ERROR: This branch should never execute (ret=%d)\n", ret);
                    av_log(NULL, AV_LOG_ERROR, "avio_open returned negative value but was expected to succeed\n");
                    if (ffm->output->pb) {
                        avio_closep(&ffm->output->pb);
                    }
                    av_dict_free(&dict);
                    avcodec_free_context(&codec_ctx);
                    avcodec_free_context(&codec_ctx2);
                    avformat_free_context(ffm->output);
                    av_free(dup_data);
                    free(ffm);
                    return -1;
                }
                
                if (ffm->output->pb == NULL) {
                    printf("[DEBUG] ERROR: avio_open succeeded but pb is NULL\n");
                    av_log(NULL, AV_LOG_ERROR, "avio_open succeeded but pb is NULL\n");
                    av_dict_free(&dict);
                    avcodec_free_context(&codec_ctx);
                    avcodec_free_context(&codec_ctx2);
                    avformat_free_context(ffm->output);
                    av_free(dup_data);
                    free(ffm);
                    return -1;
                } else {
                    printf("[DEBUG] pb is valid after avio_open success (pb=%p)\n", (void*)ffm->output->pb);
                }
            }
        } else {
            printf("[DEBUG] Skipping avio_open() - format requires AVFMT_NOFILE or format is NULL\n");
        }
    } else {
        printf("[DEBUG] Skipping avio_open() - ffm or ffm->output is NULL\n");
    }
    
    printf("[DEBUG] Calling av_dict_parse_string() with 'key2=value2'\n");
    ret = av_dict_parse_string(&dict, "key2=value2", "=", ":", 0);
    printf("[DEBUG] av_dict_parse_string() returned ret=%d\n", ret);
    if (ret < 0) {
        printf("[DEBUG] av_dict_parse_string() failed with ret=%d\n", ret);
        av_log(NULL, AV_LOG_ERROR, "Can't parse dictionary string\n");
    } else {
        printf("[DEBUG] av_dict_parse_string() succeeded\n");
    }
    
    printf("[DEBUG] Calling av_dict_count()\n");
    dict_count = av_dict_count(dict);
    printf("[DEBUG] av_dict_count() returned dict_count=%d\n", dict_count);
    
    printf("[DEBUG] Calling av_dict_get() with key='key'\n");
    entry = av_dict_get(dict, "key", NULL, 0);
    printf("[DEBUG] av_dict_get() returned entry=%p\n", (void*)entry);
    
    printf("[DEBUG] Checking if ffm, ffm->output, and ffm->output->pb are valid for avformat_write_header\n");
    printf("[DEBUG] ffm=%p, output=%p, pb=%p\n",
           (void*)ffm, ffm ? (void*)ffm->output : NULL,
           (ffm && ffm->output) ? (void*)ffm->output->pb : NULL);
    if (ffm && ffm->output && ffm->output->pb) {
        printf("[DEBUG] Calling avformat_write_header()\n");
        ret = avformat_write_header(ffm->output, &dict);
        printf("[DEBUG] avformat_write_header() returned ret=%d\n", ret);
        if (ret < 0) {
            printf("[DEBUG] avformat_write_header() failed with ret=%d\n", ret);
            av_log(NULL, AV_LOG_ERROR, "Can't write format header\n");
            av_dict_free(&dict);
            avio_closep(&ffm->output->pb);
            avcodec_free_context(&codec_ctx);
            avcodec_free_context(&codec_ctx2);
            avformat_free_context(ffm->output);
            av_free(dup_data);
            free(ffm);
            return ret;
        } else {
            printf("[DEBUG] avformat_write_header() succeeded\n");
        }
    } else {
        printf("[DEBUG] Skipping avformat_write_header() - condition not met\n");
    }
    
    printf("[DEBUG] Calling av_dict_free()\n");
    av_dict_free(&dict);
    printf("[DEBUG] av_dict_free() completed, dict=%p\n", (void*)dict);
    
    printf("[DEBUG] Calling av_init_packet()\n");
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;
    pkt.stream_index = 0;
    pkt.pts = 0;
    pkt.dts = 0;
    pkt.duration = 1;
    printf("[DEBUG] av_init_packet() completed, pkt.data=%p, pkt.size=%d\n", pkt.data, pkt.size);
    
    AVRational src_tb = {1, 1000};
    AVRational dst_tb = {1, 90000};
    printf("[DEBUG] Calling av_rescale_q_rnd() with ts=1000, src_tb={%d,%d}, dst_tb={%d,%d}\n",
           src_tb.num, src_tb.den, dst_tb.num, dst_tb.den);
    int64_t ts = av_rescale_q_rnd(1000, src_tb, dst_tb, AV_ROUND_NEAR_INF);
    printf("[DEBUG] av_rescale_q_rnd() returned ts=%lld\n", (long long)ts);
    
    printf("[DEBUG] Skipping av_interleaved_write_frame() - packet has no data\n");
    
    printf("[DEBUG] Checking if ffm, ffm->output, and ffm->output->pb are valid for avio_closep\n");
    printf("[DEBUG] ffm=%p, output=%p, pb=%p\n",
           (void*)ffm, ffm ? (void*)ffm->output : NULL,
           (ffm && ffm->output) ? (void*)ffm->output->pb : NULL);
    if (ffm && ffm->output && ffm->output->pb) {
        printf("[DEBUG] Calling avio_closep()\n");
        avio_closep(&ffm->output->pb);
        printf("[DEBUG] avio_closep() completed, pb=%p\n", (void*)ffm->output->pb);
    } else {
        printf("[DEBUG] Skipping avio_closep() - condition not met\n");
    }
    
    printf("[DEBUG] Calling avcodec_free_context() for codec_ctx\n");
    avcodec_free_context(&codec_ctx);
    printf("[DEBUG] avcodec_free_context() completed, codec_ctx=%p\n", (void*)codec_ctx);
    
    printf("[DEBUG] Calling avcodec_free_context() for codec_ctx2\n");
    avcodec_free_context(&codec_ctx2);
    printf("[DEBUG] avcodec_free_context() completed, codec_ctx2=%p\n", (void*)codec_ctx2);
    
    printf("[DEBUG] Checking if ffm and ffm->output are valid for avformat_free_context\n");
    printf("[DEBUG] ffm=%p, output=%p\n", (void*)ffm, ffm ? (void*)ffm->output : NULL);
    if (ffm && ffm->output) {
        printf("[DEBUG] Calling avformat_free_context()\n");
        avformat_free_context(ffm->output);
        printf("[DEBUG] avformat_free_context() completed\n");
    } else {
        printf("[DEBUG] Skipping avformat_free_context() - condition not met\n");
    }
    
    printf("[DEBUG] Calling av_free() for dup_data\n");
    av_free(dup_data);
    printf("[DEBUG] av_free() completed\n");
    
    printf("[DEBUG] Calling free() for ffm\n");
    free(ffm);
    printf("[DEBUG] free() completed\n");
    
    printf("[DEBUG] Test_open_output_file: END - returning 0\n");
    return 0;
}

int main(int argc, char **argv)
{
    printf("[DEBUG] main: START with argc=%d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] main: argv[%d]='%s'\n", i, argv[i]);
    }
    
    const char *output_filename = "/tmp/test_output.mp4";
    
    if (argc >= 2) {
        output_filename = argv[1];
        printf("[DEBUG] main: Using provided filename: %s\n", output_filename);
    } else {
        printf("[DEBUG] main: No filename provided, using default: %s\n", output_filename);
    }

    printf("[DEBUG] main: Calling Test_open_output_file()\n");
    int result = Test_open_output_file(output_filename);
    printf("[DEBUG] main: Test_open_output_file() returned %d\n", result);
    
    if (result != 0) {
        printf("[DEBUG] main: Test failed, returning 1\n");
        return 1;
    }

    printf("[DEBUG] main: Test succeeded, returning 0\n");
    printf("[DEBUG] main: END\n");
    return 0;
}
