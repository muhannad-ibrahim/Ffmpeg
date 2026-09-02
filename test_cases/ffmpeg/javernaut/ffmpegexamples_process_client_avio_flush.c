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
 * avio_flush test.
 */

#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libavutil/opt.h"
#include "libavutil/log.h"
#include "libavutil/mem.h"
#include "libavutil/error.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int write_packet_called = 0;
static int last_write_size = 0;

static int write_packet(void *opaque, uint8_t *buf, int buf_size)
{
    printf("[DEBUG] write_packet() called - buf_size: %d\n", buf_size);
    write_packet_called++;
    last_write_size = buf_size;
    printf("[DEBUG] write_packet() - write_packet_called counter: %d, last_write_size: %d\n", 
           write_packet_called, last_write_size);
    return buf_size;
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
    printf("[DEBUG] read_packet() called - buf_size: %d\n", buf_size);
    memset(buf, 0, buf_size);
    printf("[DEBUG] read_packet() - buffer cleared, returning buf_size: %d\n", buf_size);
    return buf_size;
}

static int64_t seek_packet(void *opaque, int64_t offset, int whence)
{
    printf("[DEBUG] seek_packet() called - offset: %lld, whence: %d\n", 
           (long long)offset, whence);
    return 0;
}

static int avio_flush_test(void)
{
    printf("[DEBUG] avio_flush_test() START\n");
    
    AVIOContext *client = NULL;
    uint8_t buf[1024];
    int result = 0;
    int n = 0;
    unsigned char *avio_ctx_buffer = NULL;
    size_t avio_ctx_buffer_size = 4096;
    int64_t pos_before_flush;
    int64_t pos_after_flush;
    
    printf("[DEBUG] Allocating buffer for custom AVIO context - size: %zu\n", avio_ctx_buffer_size);
    // Allocate buffer for custom AVIO context
    avio_ctx_buffer = av_malloc(avio_ctx_buffer_size);
    if (!avio_ctx_buffer) {
        printf("[DEBUG] ERROR: Failed to allocate avio context buffer\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate avio context buffer\n");
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully allocated avio context buffer at %p\n", (void*)avio_ctx_buffer);
    
    printf("[DEBUG] Creating custom AVIO context for client\n");
    // Create custom AVIO context for client
    client = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size,
                                1, NULL, read_packet, write_packet, seek_packet);
    if (!client) {
        printf("[DEBUG] ERROR: Failed to allocate client AVIO context\n");
        av_log(NULL, AV_LOG_ERROR, "Can't allocate client AVIO context\n");
        av_free(avio_ctx_buffer);
        return AVERROR(ENOMEM);
    }
    printf("[DEBUG] Successfully created client AVIO context at %p\n", (void*)client);
    
    printf("[DEBUG] Initializing buffer with test data (0xAB pattern)\n");
    // Initialize buffer with some test data
    memset(buf, 0xAB, sizeof(buf));
    printf("[DEBUG] Buffer initialized - size: %zu\n", sizeof(buf));
    
    // Write some data to the client context
    n = 512;
    printf("[DEBUG] Writing %d bytes to client context\n", n);
    avio_write(client, buf, n);
    printf("[DEBUG] avio_write() completed - wrote %d bytes\n", n);
    
    // Reset write_packet_called counter before flush
    printf("[DEBUG] Resetting write_packet_called counter before flush\n");
    write_packet_called = 0;
    last_write_size = 0;
    printf("[DEBUG] Counters reset - write_packet_called: %d, last_write_size: %d\n", 
           write_packet_called, last_write_size);
    
    // Get position before flush
    printf("[DEBUG] Getting position before flush\n");
    pos_before_flush = avio_tell(client);
    printf("[DEBUG] Position before flush: %lld\n", (long long)pos_before_flush);
    
    // avio_flush
    printf("[DEBUG] Calling avio_flush()\n");
    avio_flush(client);
    printf("[DEBUG] avio_flush() completed\n");
    
    printf("[DEBUG] Checking if write_packet was called - write_packet_called: %d\n", 
           write_packet_called);
    // Verify flush worked by checking that write_packet was called
    if (write_packet_called == 0) {
        printf("[DEBUG] ERROR: avio_flush did not trigger write_packet callback\n");
        av_log(NULL, AV_LOG_ERROR, "avio_flush did not trigger write_packet callback\n");
        result = -1;
        goto cleanup;
    }
    printf("[DEBUG] SUCCESS: write_packet was called %d time(s)\n", write_packet_called);
    
    printf("[DEBUG] Checking if data was written - last_write_size: %d\n", last_write_size);
    // Verify that data was actually written
    if (last_write_size == 0) {
        printf("[DEBUG] ERROR: avio_flush did not write any data\n");
        av_log(NULL, AV_LOG_ERROR, "avio_flush did not write any data\n");
        result = -1;
        goto cleanup;
    }
    printf("[DEBUG] SUCCESS: Data was written - last_write_size: %d bytes\n", last_write_size);
    
    // Verify flush worked by checking buffer position
    printf("[DEBUG] Getting position after flush\n");
    pos_after_flush = avio_tell(client);
    printf("[DEBUG] Position after flush: %lld\n", (long long)pos_after_flush);
    
    printf("[DEBUG] Comparing positions - before: %lld, after: %lld\n", 
           (long long)pos_before_flush, (long long)pos_after_flush);
    // Position should remain the same after flush (flush doesn't change position)
    if (pos_after_flush != pos_before_flush) {
        printf("[DEBUG] ERROR: Position changed after flush (before: %lld, after: %lld)\n",
               (long long)pos_before_flush, (long long)pos_after_flush);
        av_log(NULL, AV_LOG_ERROR, "Position changed after flush\n");
        result = -1;
        goto cleanup;
    }
    printf("[DEBUG] SUCCESS: Position remained the same after flush\n");
    
    result = 0;
    printf("[DEBUG] All tests passed - result: %d\n", result);
    
cleanup:
    printf("[DEBUG] Entering cleanup section\n");
    // avio_close (client)
    if (client) {
        printf("[DEBUG] Freeing client AVIO context buffer\n");
        av_freep(&client->buffer);
        printf("[DEBUG] Freeing client AVIO context\n");
        avio_context_free(&client);
        printf("[DEBUG] Client AVIO context freed\n");
    } else {
        printf("[DEBUG] Client AVIO context is NULL, skipping cleanup\n");
    }
    
    printf("[DEBUG] avio_flush_test() END - returning result: %d\n", result);
    return result;
}

int main(int argc, char **argv)
{
    printf("[DEBUG] ========== TEST START ==========\n");
    printf("[DEBUG] main() called with argc: %d\n", argc);
    
    for (int i = 0; i < argc; i++) {
        printf("[DEBUG] argv[%d]: %s\n", i, argv[i]);
    }
    
    printf("[DEBUG] Calling avio_flush_test()\n");
    if (avio_flush_test() != 0) {
        printf("[DEBUG] ERROR: avio_flush_test() returned non-zero\n");
        printf("[DEBUG] ========== TEST FAILED ==========\n");
        return 1;
    }
    printf("[DEBUG] avio_flush_test() returned 0 (success)\n");

    printf("[DEBUG] ========== TEST COMPLETED SUCCESSFULLY ==========\n");
    return 0;
}