#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "transcoder_utils.h"

// Test data
static const char* test_string = "Hello, World! This is a test string for compression.";
static const char* test_html = "Hello <world> & \"friends\"!";
static const char* test_url = "Hello World! Test@example.com?param=value&other=data";

// Test result tracking
static int tests_run = 0;
static int tests_passed = 0;

#define TEST_ASSERT(condition, message) do { \
    tests_run++; \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", message); \
    } else { \
        printf("✗ %s\n", message); \
        printf("  Error: %s\n", transcoder_get_last_error()); \
    } \
} while(0)

void test_gzip_compression() {
    printf("\n=== Testing GZIP Compression ===\n");
    
    size_t input_len = strlen(test_string);
    size_t compressed_len;
    
    // Test compression
    char* compressed = gzip_compress(test_string, input_len, &compressed_len, COMPRESS_LEVEL_DEFAULT);
    TEST_ASSERT(compressed != NULL, "GZIP compression succeeds");
    TEST_ASSERT(compressed_len > 0, "GZIP produces non-zero output");
    
    if (compressed) {
        printf("  Original size: %zu bytes\n", input_len);
        printf("  Compressed size: %zu bytes\n", compressed_len);
        printf("  Compression ratio: %.2f%%\n", 
               get_compression_ratio(input_len, compressed_len) * 100);
        
        // Test decompression
        size_t decompressed_len;
        char* decompressed = gzip_decompress(compressed, compressed_len, &decompressed_len);
        
        TEST_ASSERT(decompressed != NULL, "GZIP decompression succeeds");
        TEST_ASSERT(decompressed_len == input_len, "GZIP decompressed size matches original");
        
        if (decompressed) {
            TEST_ASSERT(memcmp(test_string, decompressed, input_len) == 0, 
                       "GZIP decompressed data matches original");
            free(decompressed);
        }
        
        free(compressed);
    }
}

void test_deflate_compression() {
    printf("\n=== Testing DEFLATE Compression ===\n");
    
    size_t input_len = strlen(test_string);
    size_t compressed_len;
    
    // Test compression
    char* compressed = deflate_compress(test_string, input_len, &compressed_len, COMPRESS_LEVEL_DEFAULT);
    TEST_ASSERT(compressed != NULL, "DEFLATE compression succeeds");
    TEST_ASSERT(compressed_len > 0, "DEFLATE produces non-zero output");
    
    if (compressed) {
        printf("  Original size: %zu bytes\n", input_len);
        printf("  Compressed size: %zu bytes\n", compressed_len);
        printf("  Compression ratio: %.2f%%\n", 
               get_compression_ratio(input_len, compressed_len) * 100);
        
        // Test decompression
        size_t decompressed_len;
        char* decompressed = deflate_decompress(compressed, compressed_len, &decompressed_len);
        
        TEST_ASSERT(decompressed != NULL, "DEFLATE decompression succeeds");
        TEST_ASSERT(decompressed_len == input_len, "DEFLATE decompressed size matches original");
        
        if (decompressed) {
            TEST_ASSERT(memcmp(test_string, decompressed, input_len) == 0, 
                       "DEFLATE decompressed data matches original");
            free(decompressed);
        }
        
        free(compressed);
    }
}

void test_url_encoding() {
    printf("\n=== Testing URL Encoding ===\n");
    
    // Test encoding
    char* encoded = url_encode(test_url);
    TEST_ASSERT(encoded != NULL, "URL encoding succeeds");
    
    if (encoded) {
        printf("  Original: %s\n", test_url);
        printf("  Encoded:  %s\n", encoded);
        
        // Test decoding
        char* decoded = url_decode(encoded);
        TEST_ASSERT(decoded != NULL, "URL decoding succeeds");
        
        if (decoded) {
            TEST_ASSERT(strcmp(test_url, decoded) == 0, "URL decoded data matches original");
            printf("  Decoded:  %s\n", decoded);
            free(decoded);
        }
        
        free(encoded);
    }
}

void test_html_encoding() {
    printf("\n=== Testing HTML Encoding ===\n");
    
    char* encoded = html_encode(test_html);
    TEST_ASSERT(encoded != NULL, "HTML encoding succeeds");
    
    if (encoded) {
        printf("  Original: %s\n", test_html);
        printf("  Encoded:  %s\n", encoded);
        
        // Check for expected encodings
        TEST_ASSERT(strstr(encoded, "&lt;") != NULL, "HTML encodes < to &lt;");
        TEST_ASSERT(strstr(encoded, "&gt;") != NULL, "HTML encodes > to &gt;");
        TEST_ASSERT(strstr(encoded, "&amp;") != NULL, "HTML encodes & to &amp;");
        TEST_ASSERT(strstr(encoded, "&quot;") != NULL, "HTML encodes \" to &quot;");
        
        free(encoded);
    }
}

void test_hex_encoding() {
    printf("\n=== Testing Hex Encoding ===\n");
    
    const char* test_data = "Hello, World!";
    size_t test_len = strlen(test_data);
    
    // Test encoding
    char* hex = hex_encode(test_data, test_len);
    TEST_ASSERT(hex != NULL, "Hex encoding succeeds");
    
    if (hex) {
        printf("  Original: %s\n", test_data);
        printf("  Hex:      %s\n", hex);
        
        // Test decoding
        size_t decoded_len;
        char* decoded = hex_decode(hex, &decoded_len);
        
        TEST_ASSERT(decoded != NULL, "Hex decoding succeeds");
        TEST_ASSERT(decoded_len == test_len, "Hex decoded size matches original");
        
        if (decoded) {
            TEST_ASSERT(memcmp(test_data, decoded, test_len) == 0, 
                       "Hex decoded data matches original");
            free(decoded);
        }
        
        free(hex);
    }
}

void test_crc32_checksum() {
    printf("\n=== Testing CRC32 Checksum ===\n");
    
    uint32_t crc1 = crc32_checksum(test_string, strlen(test_string));
    uint32_t crc2 = crc32_checksum(test_string, strlen(test_string));
    
    TEST_ASSERT(crc1 != 0, "CRC32 produces non-zero result");
    TEST_ASSERT(crc1 == crc2, "CRC32 is deterministic");
    
    printf("  CRC32 of '%s': 0x%08X\n", test_string, crc1);
    
    // Test different data produces different checksum
    uint32_t crc3 = crc32_checksum("Different data", 14);
    TEST_ASSERT(crc1 != crc3, "Different data produces different CRC32");
}

void test_file_operations() {
    printf("\n=== Testing File Operations ===\n");
    
    // Create test file
    const char* test_file = "/tmp/test_input.txt";
    const char* gzip_file = "/tmp/test_output.gz";
    const char* restored_file = "/tmp/test_restored.txt";
    
    // Write test data
    FILE* f = fopen(test_file, "w");
    if (f) {
        fputs(test_string, f);
        fclose(f);
        
        // Test file compression
        bool compress_ok = gzip_compress_file(test_file, gzip_file, COMPRESS_LEVEL_DEFAULT);
        TEST_ASSERT(compress_ok, "GZIP file compression succeeds");
        
        // Test file decompression
        bool decompress_ok = gzip_decompress_file(gzip_file, restored_file);
        TEST_ASSERT(decompress_ok, "GZIP file decompression succeeds");
        
        // Verify restored file
        if (compress_ok && decompress_ok) {
            f = fopen(restored_file, "r");
            if (f) {
                char buffer[1024];
                fgets(buffer, sizeof(buffer), f);
                fclose(f);
                
                TEST_ASSERT(strcmp(buffer, test_string) == 0, 
                           "Restored file content matches original");
            }
        }
        
        // Cleanup
        unlink(test_file);
        unlink(gzip_file);
        unlink(restored_file);
    }
}

void test_compression_levels() {
    printf("\n=== Testing Compression Levels ===\n");
    
    size_t input_len = strlen(test_string);
    CompressionLevel levels[] = {
        COMPRESS_LEVEL_FASTEST,
        COMPRESS_LEVEL_FAST,
        COMPRESS_LEVEL_DEFAULT,
        COMPRESS_LEVEL_BEST
    };
    const char* level_names[] = {
        "FASTEST",
        "FAST", 
        "DEFAULT",
        "BEST"
    };
    
    for (int i = 0; i < 4; i++) {
        size_t compressed_len;
        char* compressed = gzip_compress(test_string, input_len, &compressed_len, levels[i]);
        
        if (compressed) {
            printf("  %s: %zu bytes (%.2f%%)\n", 
                   level_names[i], compressed_len,
                   get_compression_ratio(input_len, compressed_len) * 100);
            free(compressed);
        }
    }
}

void test_error_handling() {
    printf("\n=== Testing Error Handling ===\n");
    
    // Test NULL parameters
    char* result = gzip_compress(NULL, 0, NULL, COMPRESS_LEVEL_DEFAULT);
    TEST_ASSERT(result == NULL, "NULL data parameter handled correctly");
    
    result = url_encode(NULL);
    TEST_ASSERT(result == NULL, "NULL URL data handled correctly");
    
    size_t len;
    result = hex_decode("invalid_hex", &len);
    TEST_ASSERT(result == NULL, "Invalid hex string handled correctly");
    
    // Test odd length hex
    result = hex_decode("abc", &len);
    TEST_ASSERT(result == NULL, "Odd length hex string handled correctly");
}

int main() {
    printf("Transcoder Utils Library Test Suite\n");
    printf("===================================\n");
    
    test_gzip_compression();
    test_deflate_compression();
    test_url_encoding();
    test_html_encoding();
    test_hex_encoding();
    test_crc32_checksum();
    test_file_operations();
    test_compression_levels();
    test_error_handling();
    
    printf("\n=== Test Results ===\n");
    printf("Tests run: %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);
    printf("Success rate: %.1f%%\n", 
           tests_run > 0 ? (double)tests_passed / tests_run * 100 : 0);
    
    if (tests_passed == tests_run) {
        printf("\n🎉 All tests passed!\n");
        return 0;
    } else {
        printf("\n❌ Some tests failed!\n");
        return 1;
    }
}