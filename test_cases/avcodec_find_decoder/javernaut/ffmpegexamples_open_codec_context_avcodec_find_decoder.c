/*
 * Copyright (c) 2015 Ludmila Glinskih
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * avcodec_find_decoder test.
 */

#include "libavutil/adler32.h"
#include "libavutil/mem.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libavutil/timestamp.h"
#include "libavutil/samplefmt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_VIDEO_WIDTH 320
#define TEST_VIDEO_HEIGHT 240
#define TEST_VIDEO_FRAMES 10

static int create_test_video_file(const char *filename)
{
    printf("[DEBUG] === Creating test video file: %s ===\n", filename);
    
    AVFormatContext *fmt_ctx = NULL;
    const AVCodec *codec = NULL;
    AVCodecContext *codec_ctx = NULL;
    AVStream *stream = NULL;
    AVPacket *pkt = NULL;
    AVFrame *frame = NULL;
    int ret = 0;
    int i;

    // Allocate output format context
    ret = avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not allocate output format context\n");
        return ret;
    }

    // Find encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!codec) {
        printf("[DEBUG] ERROR: Codec not found\n");
        ret = -1;
        goto cleanup;
    }

    // Add stream
    stream = avformat_new_stream(fmt_ctx, NULL);
    if (!stream) {
        printf("[DEBUG] ERROR: Could not create stream\n");
        ret = -1;
        goto cleanup;
    }

    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        printf("[DEBUG] ERROR: Could not allocate codec context\n");
        ret = -1;
        goto cleanup;
    }

    // Set codec parameters
    codec_ctx->width = TEST_VIDEO_WIDTH;
    codec_ctx->height = TEST_VIDEO_HEIGHT;
    codec_ctx->time_base = (AVRational){1, 25};
    codec_ctx->framerate = (AVRational){25, 1};
    codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx->bit_rate = 400000;
    codec_ctx->gop_size = 10;
    codec_ctx->max_b_frames = 1;

    // Open codec
    ret = avcodec_open2(codec_ctx, codec, NULL);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not open codec\n");
        goto cleanup;
    }

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, codec_ctx);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not copy codec parameters\n");
        goto cleanup;
    }

    // Open output file
    ret = avio_open(&fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not open output file\n");
        goto cleanup;
    }

    // Write header
    ret = avformat_write_header(fmt_ctx, NULL);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not write header\n");
        goto cleanup;
    }

    // Allocate frame
    frame = av_frame_alloc();
    if (!frame) {
        printf("[DEBUG] ERROR: Could not allocate frame\n");
        ret = -1;
        goto cleanup;
    }

    frame->format = codec_ctx->pix_fmt;
    frame->width = codec_ctx->width;
    frame->height = codec_ctx->height;

    ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Could not allocate frame buffer\n");
        goto cleanup;
    }

    // Allocate packet
    pkt = av_packet_alloc();
    if (!pkt) {
        printf("[DEBUG] ERROR: Could not allocate packet\n");
        ret = -1;
        goto cleanup;
    }

    // Encode frames
    for (i = 0; i < TEST_VIDEO_FRAMES; i++) {
        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            printf("[DEBUG] ERROR: Frame not writable\n");
            goto cleanup;
        }

        // Generate dummy frame data
        int y, x;
        for (y = 0; y < codec_ctx->height; y++) {
            for (x = 0; x < codec_ctx->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }
        for (y = 0; y < codec_ctx->height / 2; y++) {
            for (x = 0; x < codec_ctx->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        // Send frame to encoder
        ret = avcodec_send_frame(codec_ctx, frame);
        if (ret < 0) {
            printf("[DEBUG] ERROR: Error sending frame to encoder\n");
            goto cleanup;
        }

        // Receive packets from encoder
        while (ret >= 0) {
            ret = avcodec_receive_packet(codec_ctx, pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                printf("[DEBUG] ERROR: Error receiving packet from encoder\n");
                goto cleanup;
            }

            av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
            pkt->stream_index = stream->index;

            ret = av_interleaved_write_frame(fmt_ctx, pkt);
            av_packet_unref(pkt);
            if (ret < 0) {
                printf("[DEBUG] ERROR: Error writing packet\n");
                goto cleanup;
            }
        }
    }

    // Flush encoder
    ret = avcodec_send_frame(codec_ctx, NULL);
    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            goto cleanup;
        }

        av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        ret = av_interleaved_write_frame(fmt_ctx, pkt);
        av_packet_unref(pkt);
    }

    // Write trailer
    av_write_trailer(fmt_ctx);
    ret = 0;

cleanup:
    if (pkt)
        av_packet_free(&pkt);
    if (frame)
        av_frame_free(&frame);
    if (codec_ctx)
        avcodec_free_context(&codec_ctx);
    if (fmt_ctx) {
        if (fmt_ctx->pb)
            avio_closep(&fmt_ctx->pb);
        avformat_free_context(fmt_ctx);
    }

    printf("[DEBUG] === Test file creation completed with result: %d ===\n", ret);
    return ret;
}

static int test_avcodec_find_decoder(const char *input_filename)
{
    printf("[DEBUG] === Starting test_avcodec_find_decoder ===\n");
    printf("[DEBUG] Input filename: %s\n", input_filename);
    
    const AVCodec *codec = NULL;
    AVCodecContext *ctx = NULL;
    AVCodecParameters *origin_par = NULL;
    AVFrame *fr = NULL;
    AVPacket *pkt = NULL;
    AVFormatContext *fmt_ctx = NULL;
    AVCodecParserContext *parser = NULL;
    int video_stream;
    int result;
    enum AVSampleFormat sample_fmt = AV_SAMPLE_FMT_NONE;
    const char *fmt_name = NULL;
    enum AVSampleFormat packed_fmt = AV_SAMPLE_FMT_NONE;
    int is_planar = 0;

    printf("[DEBUG] Calling avformat_open_input...\n");
    result = avformat_open_input(&fmt_ctx, input_filename, NULL, NULL);
    printf("[DEBUG] avformat_open_input returned: %d\n", result);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file\n");
        printf("[DEBUG] ERROR: Failed to open file, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully opened input file\n");

    printf("[DEBUG] Calling avformat_find_stream_info...\n");
    result = avformat_find_stream_info(fmt_ctx, NULL);
    printf("[DEBUG] avformat_find_stream_info returned: %d\n", result);
    if (result < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't get stream info\n");
        printf("[DEBUG] ERROR: Failed to get stream info, returning %d\n", result);
        return result;
    }
    printf("[DEBUG] Successfully retrieved stream info\n");

    printf("[DEBUG] Calling av_find_best_stream for video...\n");
    video_stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    printf("[DEBUG] av_find_best_stream returned: %d\n", video_stream);
    if (video_stream < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find video stream in input file\n");
        printf("[DEBUG] ERROR: No video stream found, returning -1\n");
        return -1;
    }
    printf("[DEBUG] Found video stream at index: %d\n", video_stream);

    origin_par = fmt_ctx->streams[video_stream]->codecpar;
    printf("[DEBUG] Retrieved codec parameters, codec_id: %d\n", origin_par->codec_id);

    // avcodec_find_decoder
    // Test 1: Valid codec ID should return a non-NULL decoder
    printf("[DEBUG] TEST 1: Calling avcodec_find_decoder with codec_id: %d\n", origin_par->codec_id);
    codec = avcodec_find_decoder(origin_par->codec_id);
    printf("[DEBUG] avcodec_find_decoder returned: %p\n", (void*)codec);
    if (!codec) {
        av_log(NULL, AV_LOG_ERROR, "Can't find decoder\n");
        printf("[DEBUG] ERROR: Decoder not found, returning -1\n");
        return -1;
    }
    printf("Decoder found: %s\n", codec->name);
    printf("[DEBUG] Decoder name: %s, type: %d\n", codec->name, codec->type);

    // Verify the decoder has the correct codec ID
    printf("[DEBUG] Verifying decoder codec ID...\n");
    printf("[DEBUG] Expected codec_id: %d, Got codec_id: %d\n", origin_par->codec_id, codec->id);
    if (codec->id != origin_par->codec_id) {
        av_log(NULL, AV_LOG_ERROR, "Decoder codec ID mismatch: expected %d, got %d\n", origin_par->codec_id, codec->id);
        printf("[DEBUG] ERROR: Codec ID mismatch!\n");
        result = -1;
        goto cleanup;
    }
    printf("Decoder codec ID verified: %d\n", codec->id);
    printf("[DEBUG] Codec ID verification passed\n");

    // Test 2: Invalid/unknown codec ID should return NULL
    printf("[DEBUG] TEST 2: Calling avcodec_find_decoder with AV_CODEC_ID_NONE\n");
    const AVCodec *invalid_dec = avcodec_find_decoder(AV_CODEC_ID_NONE);
    printf("[DEBUG] avcodec_find_decoder(AV_CODEC_ID_NONE) returned: %p\n", (void*)invalid_dec);
    if (invalid_dec != NULL) {
        av_log(NULL, AV_LOG_ERROR, "Expected NULL for invalid codec ID, but got a decoder\n");
        printf("[DEBUG] ERROR: Expected NULL but got decoder: %s\n", invalid_dec->name);
        result = -1;
        goto cleanup;
    }
    printf("Correctly returned NULL for invalid codec ID\n");
    printf("[DEBUG] Invalid codec ID test passed\n");

    // Test 3: Another valid codec ID
    printf("[DEBUG] TEST 3: Calling avcodec_find_decoder with AV_CODEC_ID_H264\n");
    const AVCodec *dec2 = avcodec_find_decoder(AV_CODEC_ID_H264);
    printf("[DEBUG] avcodec_find_decoder(AV_CODEC_ID_H264) returned: %p\n", (void*)dec2);
    if (!dec2) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for valid codec ID AV_CODEC_ID_H264\n");
        printf("[DEBUG] ERROR: H264 decoder not found\n");
        result = -1;
        goto cleanup;
    }
    printf("[DEBUG] H264 decoder found: %s\n", dec2->name);
    printf("[DEBUG] Verifying H264 decoder codec ID...\n");
    if (dec2->id != AV_CODEC_ID_H264) {
        av_log(NULL, AV_LOG_ERROR, "Decoder codec ID mismatch for H264: expected %d, got %d\n", AV_CODEC_ID_H264, dec2->id);
        printf("[DEBUG] ERROR: H264 codec ID mismatch!\n");
        result = -1;
        goto cleanup;
    }
    printf("Decoder found for H264: %s with codec ID: %d\n", dec2->name, dec2->id);
    printf("[DEBUG] H264 decoder test passed\n");

    printf("[DEBUG] Calling avcodec_alloc_context3...\n");
    ctx = avcodec_alloc_context3(codec);
    printf("[DEBUG] avcodec_alloc_context3 returned: %p\n", (void*)ctx);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate decoder context\n");
        printf("[DEBUG] ERROR: Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully allocated decoder context\n");

    printf("[DEBUG] Calling avcodec_parameters_to_context...\n");
    result = avcodec_parameters_to_context(ctx, origin_par);
    printf("[DEBUG] avcodec_parameters_to_context returned: %d\n", result);
    if (result) {
        av_log(NULL, AV_LOG_ERROR, "Can't copy decoder context\n");
        printf("[DEBUG] ERROR: Failed to copy decoder context\n");
        goto cleanup;
    }
    printf("[DEBUG] Successfully copied parameters to context\n");

    printf("[DEBUG] Calling avcodec_open2...\n");
    result = avcodec_open2(ctx, codec, NULL);
    printf("[DEBUG] avcodec_open2 returned: %d\n", result);
    if (result < 0) {
        av_log(ctx, AV_LOG_ERROR, "Can't open decoder\n");
        printf("[DEBUG] ERROR: Failed to open decoder\n");
        goto cleanup;
    }
    printf("[DEBUG] Successfully opened decoder\n");

    printf("[DEBUG] Calling av_frame_alloc...\n");
    fr = av_frame_alloc();
    printf("[DEBUG] av_frame_alloc returned: %p\n", (void*)fr);
    if (!fr) {
        av_log(NULL, AV_LOG_ERROR, "Can't allocate frame\n");
        printf("[DEBUG] ERROR: Failed to allocate frame\n");
        result = AVERROR(ENOMEM);
        goto cleanup;
    }
    printf("[DEBUG] Successfully allocated frame\n");

    printf("[DEBUG] Calling av_packet_alloc...\n");
    pkt = av_packet_alloc();
    printf("[DEBUG] av_packet_alloc returned: %p\n", (void*)pkt);
    if (!pkt) {
        av_log(NULL, AV_LOG_ERROR, "Cannot allocate packet\n");
        printf("[DEBUG] ERROR: Failed to allocate packet\n");
        result = AVERROR(ENOMEM);
        goto cleanup;
    }
    printf("[DEBUG] Successfully allocated packet\n");

    // av_parser_init
    printf("[DEBUG] Calling av_parser_init with codec_id: %d\n", codec->id);
    parser = av_parser_init(codec->id);
    printf("[DEBUG] av_parser_init returned: %p\n", (void*)parser);
    if (!parser) {
        av_log(NULL, AV_LOG_ERROR, "Failed to initialize parser\n");
        printf("[DEBUG] WARNING: Parser initialization failed (may be expected for some codecs)\n");
    } else {
        printf("[DEBUG] Successfully initialized parser\n");
    }

    // av_sample_fmt_is_planar
    sample_fmt = AV_SAMPLE_FMT_S16P;
    printf("[DEBUG] Calling av_sample_fmt_is_planar with sample_fmt: %d\n", sample_fmt);
    is_planar = av_sample_fmt_is_planar(sample_fmt);
    printf("[DEBUG] av_sample_fmt_is_planar returned: %d\n", is_planar);
    av_log(NULL, AV_LOG_INFO, "Sample format is planar: %d\n", is_planar);

    // av_get_sample_fmt_name
    printf("[DEBUG] Calling av_get_sample_fmt_name with sample_fmt: %d\n", sample_fmt);
    fmt_name = av_get_sample_fmt_name(sample_fmt);
    printf("[DEBUG] av_get_sample_fmt_name returned: %p\n", (void*)fmt_name);
    if (fmt_name) {
        av_log(NULL, AV_LOG_INFO, "Sample format name: %s\n", fmt_name);
        printf("[DEBUG] Sample format name: %s\n", fmt_name);
    } else {
        printf("[DEBUG] WARNING: Sample format name is NULL\n");
    }

    // av_get_packed_sample_fmt
    printf("[DEBUG] Calling av_get_packed_sample_fmt with sample_fmt: %d\n", sample_fmt);
    packed_fmt = av_get_packed_sample_fmt(sample_fmt);
    printf("[DEBUG] av_get_packed_sample_fmt returned: %d\n", packed_fmt);
    av_log(NULL, AV_LOG_INFO, "Packed sample format: %d\n", packed_fmt);

    result = 0;
    printf("Test completed successfully\n");
    printf("[DEBUG] All tests passed, result = 0\n");

cleanup:
    printf("[DEBUG] === Entering cleanup section ===\n");
    if (parser) {
        printf("[DEBUG] Calling av_parser_close...\n");
        av_parser_close(parser);
        printf("[DEBUG] Parser closed\n");
    } else {
        printf("[DEBUG] Parser is NULL, skipping av_parser_close\n");
    }
    
    printf("[DEBUG] Calling av_packet_free...\n");
    av_packet_free(&pkt);
    printf("[DEBUG] Packet freed\n");
    
    printf("[DEBUG] Calling av_frame_free...\n");
    av_frame_free(&fr);
    printf("[DEBUG] Frame freed\n");
    
    printf("[DEBUG] Calling avformat_close_input...\n");
    avformat_close_input(&fmt_ctx);
    printf("[DEBUG] Format context closed\n");
    
    printf("[DEBUG] Calling avcodec_free_context...\n");
    avcodec_free_context(&ctx);
    printf("[DEBUG] Codec context freed\n");
    
    printf("[DEBUG] === Cleanup completed, returning result: %d ===\n", result);
    return result;
}

int main(int argc, char **argv)
{
    printf("[DEBUG] ========================================\n");
    printf("[DEBUG] === Program started ===\n");
    printf("[DEBUG] argc: %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] argv[%d]: %s\n", i, argv[i]);
    }
    printf("[DEBUG] ========================================\n");
    
    const char *test_file = "/tmp/test_video.mp4";
    int ret;

    // Create test video file
    printf("[DEBUG] Creating test video file...\n");
    ret = create_test_video_file(test_file);
    if (ret < 0) {
        printf("[DEBUG] ERROR: Failed to create test video file\n");
        printf("[DEBUG] === Program exiting with error code 1 ===\n");
        return 1;
    }
    printf("[DEBUG] Test video file created successfully\n");

    printf("[DEBUG] Calling test_avcodec_find_decoder...\n");
    int test_result = test_avcodec_find_decoder(test_file);
    printf("[DEBUG] test_avcodec_find_decoder returned: %d\n", test_result);
    
    // Clean up test file
    remove(test_file);
    
    if (test_result != 0) {
        printf("[DEBUG] === Test FAILED with result: %d ===\n", test_result);
        printf("[DEBUG] === Program exiting with error code 1 ===\n");
        return 1;
    }
    
    printf("[DEBUG] === Test PASSED ===\n");
    printf("[DEBUG] === Program exiting with success code 0 ===\n");
    return 0;
}
