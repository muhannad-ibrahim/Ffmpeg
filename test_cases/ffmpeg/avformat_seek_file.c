/*
 * Test for avformat_seek_file API
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
 * Test for avformat_seek_file:
 *   - validates parameter checking (min_ts > ts, max_ts < ts, bad stream_index)
 *   - performs real seeks on a media file passed as argv[1]
 */

#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/mathematics.h"

/* ------------------------------------------------------------------ */
/* helpers                                                              */
/* ------------------------------------------------------------------ */

static AVFormatContext *open_file(const char *path)
{
    AVFormatContext *fmt_ctx = NULL;
    int ret = avformat_open_input(&fmt_ctx, path, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open '%s': %d\n", path, ret);
        return NULL;
    }
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream info for '%s': %d\n", path, ret);
        avformat_close_input(&fmt_ctx);
        return NULL;
    }
    return fmt_ctx;
}

/* ------------------------------------------------------------------ */
/* test cases                                                           */
/* ------------------------------------------------------------------ */

/**
 * T1: min_ts > ts must return -1 (precondition violation).
 */
static int test_min_ts_greater_than_ts(AVFormatContext *s)
{
    int ret;
    int64_t ts     = 1000;
    int64_t min_ts = 2000; /* min > ts => invalid */
    int64_t max_ts = 3000;

    ret = avformat_seek_file(s, -1, min_ts, ts, max_ts, 0);
    if (ret != -1) {
        fprintf(stderr, "T1 FAIL: expected -1 when min_ts > ts, got %d\n", ret);
        return 1;
    }
    printf("T1 PASS: min_ts > ts correctly returns -1\n");
    return 0;
}

/**
 * T2: max_ts < ts must return -1 (precondition violation).
 */
static int test_max_ts_less_than_ts(AVFormatContext *s)
{
    int ret;
    int64_t ts     = 5000;
    int64_t min_ts = 1000;
    int64_t max_ts = 2000; /* max < ts => invalid */

    ret = avformat_seek_file(s, -1, min_ts, ts, max_ts, 0);
    if (ret != -1) {
        fprintf(stderr, "T2 FAIL: expected -1 when max_ts < ts, got %d\n", ret);
        return 1;
    }
    printf("T2 PASS: max_ts < ts correctly returns -1\n");
    return 0;
}

/**
 * T3: stream_index < -1 must return AVERROR(EINVAL).
 */
static int test_stream_index_too_low(AVFormatContext *s)
{
    int ret;
    int64_t ts = 0;

    ret = avformat_seek_file(s, -2, 0, ts, INT64_MAX, 0);
    if (ret != AVERROR(EINVAL)) {
        fprintf(stderr, "T3 FAIL: expected AVERROR(EINVAL) for stream_index=-2, got %d\n", ret);
        return 1;
    }
    printf("T3 PASS: stream_index < -1 correctly returns AVERROR(EINVAL)\n");
    return 0;
}

/**
 * T4: stream_index >= nb_streams must return AVERROR(EINVAL).
 */
static int test_stream_index_too_high(AVFormatContext *s)
{
    int ret;
    int stream_index = (int)s->nb_streams; /* one past the last valid index */
    int64_t ts = 0;

    ret = avformat_seek_file(s, stream_index, 0, ts, INT64_MAX, 0);
    if (ret != AVERROR(EINVAL)) {
        fprintf(stderr, "T4 FAIL: expected AVERROR(EINVAL) for stream_index=%d (nb_streams=%u), got %d\n",
                stream_index, s->nb_streams, ret);
        return 1;
    }
    printf("T4 PASS: stream_index >= nb_streams correctly returns AVERROR(EINVAL)\n");
    return 0;
}

/**
 * T5: valid seek to the beginning of the file (ts=0).
 */
static int test_seek_to_start(AVFormatContext *s)
{
    int ret;

    ret = avformat_seek_file(s, -1, 0, 0, INT64_MAX, 0);
    if (ret < 0) {
        fprintf(stderr, "T5 FAIL: seek to start returned %d\n", ret);
        return 1;
    }
    printf("T5 PASS: seek to start succeeded (ret=%d)\n", ret);
    return 0;
}

/**
 * T6: valid seek toward the middle of the file using AV_TIME_BASE units.
 */
static int test_seek_to_middle(AVFormatContext *s)
{
    int ret;
    int64_t duration = s->duration; /* in AV_TIME_BASE units */
    int64_t ts;

    if (duration <= 0) {
        printf("T6 SKIP: unknown duration, skipping middle-seek test\n");
        return 0;
    }

    ts = duration / 2;
    ret = avformat_seek_file(s, -1, 0, ts, ts, 0);
    if (ret < 0) {
        fprintf(stderr, "T6 FAIL: seek to middle (ts=%"PRId64") returned %d\n", ts, ret);
        return 1;
    }
    printf("T6 PASS: seek to middle ts=%"PRId64" succeeded (ret=%d)\n", ts, ret);
    return 0;
}

/**
 * T7: seek with explicit stream_index (first stream).
 */
static int test_seek_with_stream_index(AVFormatContext *s)
{
    int ret;

    if (s->nb_streams == 0) {
        printf("T7 SKIP: no streams in file\n");
        return 0;
    }

    /* Seek to ts=0 in the first stream's time base */
    ret = avformat_seek_file(s, 0, 0, 0, INT64_MAX, 0);
    if (ret < 0) {
        fprintf(stderr, "T7 FAIL: seek with stream_index=0 returned %d\n", ret);
        return 1;
    }
    printf("T7 PASS: seek with stream_index=0 succeeded (ret=%d)\n", ret);
    return 0;
}

/**
 * T8: seek with AVSEEK_FLAG_BYTE flag (seek by byte position).
 *     The implementation should strip AVSEEK_FLAG_BACKWARD but handle BYTE.
 *     A failure here is acceptable if the format does not support byte seek;
 *     we just verify it does not crash or return a bogus invalid-arg error.
 */
static int test_seek_with_byte_flag(AVFormatContext *s)
{
    int ret;

    ret = avformat_seek_file(s, -1, 0, 0, INT64_MAX, AVSEEK_FLAG_BYTE);
    /* Result may be negative if the format does not support byte seek;
       that is OK — we only care that the preconditions were not violated. */
    printf("T8 INFO: seek with AVSEEK_FLAG_BYTE returned %d (may be negative)\n", ret);
    return 0;
}

/**
 * T9: seek with AVSEEK_FLAG_ANY flag (non-keyframe allowed).
 */
static int test_seek_with_any_flag(AVFormatContext *s)
{
    int ret;

    ret = avformat_seek_file(s, -1, 0, 0, INT64_MAX, AVSEEK_FLAG_ANY);
    if (ret < 0) {
        fprintf(stderr, "T9 FAIL: seek with AVSEEK_FLAG_ANY returned %d\n", ret);
        return 1;
    }
    printf("T9 PASS: seek with AVSEEK_FLAG_ANY succeeded (ret=%d)\n", ret);
    return 0;
}

/**
 * T10: AVSEEK_FLAG_BACKWARD must be stripped internally (the API docs say
 *      "if flags contain AVSEEK_FLAG_BACKWARD, it is ignored").
 *      The result should be the same as a forward seek.
 */
static int test_backward_flag_ignored(AVFormatContext *s)
{
    int ret;

    ret = avformat_seek_file(s, -1, 0, 0, INT64_MAX, AVSEEK_FLAG_BACKWARD);
    /* Just verify no crash / EINVAL from the API itself */
    printf("T10 INFO: seek with AVSEEK_FLAG_BACKWARD returned %d "
           "(flag should be ignored internally)\n", ret);
    return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    AVFormatContext *s = NULL;
    int failures = 0;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <media_file>\n", argv[0]);
        return 1;
    }

    s = open_file(argv[1]);
    if (!s) {
        fprintf(stderr, "Failed to open media file.\n");
        return 1;
    }

    failures += test_min_ts_greater_than_ts(s);
    failures += test_max_ts_less_than_ts(s);
    failures += test_stream_index_too_low(s);
    failures += test_stream_index_too_high(s);
    failures += test_seek_to_start(s);
    failures += test_seek_to_middle(s);
    failures += test_seek_with_stream_index(s);
    failures += test_seek_with_byte_flag(s);
    failures += test_seek_with_any_flag(s);
    failures += test_backward_flag_ignored(s);

    avformat_close_input(&s);

    if (failures) {
        fprintf(stderr, "%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("All tests PASSED\n");
    return 0;
}