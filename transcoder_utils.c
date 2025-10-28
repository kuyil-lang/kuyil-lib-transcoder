#include "transcoder_utils.h"
#include <stdio.h>
#include <string.h>
#include <zlib.h>
#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>

// Global error state
static char g_transcoder_error[512] = {0};

// Helper function to set error message
static void set_transcoder_error(const char* message) {
    snprintf(g_transcoder_error, sizeof(g_transcoder_error), "%s", message);
}

// ZIP Archive structure
struct ZipArchive {
    struct archive* archive;
    struct archive* writer;
    char* filename;
    bool is_writer;
};

// TAR Archive structure
struct TarArchive {
    struct archive* archive;
    struct archive* writer;
    char* filename;
    bool is_writer;
};

// Compression stream structure
struct CompressStream {
    z_stream stream;
    CompressionLevel level;
    bool gzip_format;
    bool initialized;
};

// GZIP Compression Implementation
char* gzip_compress(const char* data, size_t input_length, size_t* output_length, CompressionLevel level) {
    if (!data || !output_length) {
        set_transcoder_error("Invalid parameters for gzip compression");
        return NULL;
    }
    
    // Estimate output size (input + 12 bytes header + 0.1% + 12 bytes)
    size_t max_output = compressBound(input_length) + 18;
    char* output = malloc(max_output);
    if (!output) {
        set_transcoder_error("Memory allocation failed for gzip compression");
        return NULL;
    }
    
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = input_length;
    stream.next_in = (Bytef*)data;
    stream.avail_out = max_output;
    stream.next_out = (Bytef*)output;
    
    // Initialize deflate with gzip format
    int ret = deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        free(output);
        set_transcoder_error("Failed to initialize gzip compression");
        return NULL;
    }
    
    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&stream);
        free(output);
        set_transcoder_error("Gzip compression failed");
        return NULL;
    }
    
    *output_length = stream.total_out;
    deflateEnd(&stream);
    
    // Resize to actual size
    char* result = realloc(output, *output_length);
    return result ? result : output;
}

char* gzip_decompress(const char* compressed_data, size_t input_length, size_t* output_length) {
    if (!compressed_data || !output_length) {
        set_transcoder_error("Invalid parameters for gzip decompression");
        return NULL;
    }
    
    // Start with estimated size (input * 4)
    size_t buffer_size = input_length * 4;
    char* output = malloc(buffer_size);
    if (!output) {
        set_transcoder_error("Memory allocation failed for gzip decompression");
        return NULL;
    }
    
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = input_length;
    stream.next_in = (Bytef*)compressed_data;
    stream.avail_out = buffer_size;
    stream.next_out = (Bytef*)output;
    
    // Initialize inflate with gzip format
    int ret = inflateInit2(&stream, 15 + 16);
    if (ret != Z_OK) {
        free(output);
        set_transcoder_error("Failed to initialize gzip decompression");
        return NULL;
    }
    
    do {
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            free(output);
            set_transcoder_error("Gzip decompression failed");
            return NULL;
        }
        
        // If we need more output space, expand the buffer
        if (ret != Z_STREAM_END && stream.avail_out == 0) {
            size_t old_size = buffer_size;
            buffer_size *= 2;
            char* new_output = realloc(output, buffer_size);
            if (!new_output) {
                inflateEnd(&stream);
                free(output);
                set_transcoder_error("Memory reallocation failed during decompression");
                return NULL;
            }
            output = new_output;
            stream.next_out = (Bytef*)(output + old_size);
            stream.avail_out = buffer_size - old_size;
        }
    } while (ret != Z_STREAM_END);
    
    *output_length = stream.total_out;
    inflateEnd(&stream);
    
    // Resize to actual size
    char* result = realloc(output, *output_length);
    return result ? result : output;
}

bool gzip_compress_file(const char* input_file, const char* output_file, CompressionLevel level) {
    FILE* input = fopen(input_file, "rb");
    if (!input) {
        set_transcoder_error("Cannot open input file for compression");
        return false;
    }
    
    gzFile output = gzopen(output_file, "wb");
    if (!output) {
        fclose(input);
        set_transcoder_error("Cannot create output gzip file");
        return false;
    }
    
    gzsetparams(output, level, Z_DEFAULT_STRATEGY);
    
    char buffer[8192];
    size_t bytes_read;
    bool success = true;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if (gzwrite(output, buffer, bytes_read) != (int)bytes_read) {
            set_transcoder_error("Error writing to gzip file");
            success = false;
            break;
        }
    }
    
    fclose(input);
    gzclose(output);
    
    return success;
}

bool gzip_decompress_file(const char* input_file, const char* output_file) {
    gzFile input = gzopen(input_file, "rb");
    if (!input) {
        set_transcoder_error("Cannot open input gzip file");
        return false;
    }
    
    FILE* output = fopen(output_file, "wb");
    if (!output) {
        gzclose(input);
        set_transcoder_error("Cannot create output file");
        return false;
    }
    
    char buffer[8192];
    int bytes_read;
    bool success = true;
    
    while ((bytes_read = gzread(input, buffer, sizeof(buffer))) > 0) {
        if (fwrite(buffer, 1, bytes_read, output) != (size_t)bytes_read) {
            set_transcoder_error("Error writing decompressed data");
            success = false;
            break;
        }
    }
    
    if (bytes_read < 0) {
        set_transcoder_error("Error reading from gzip file");
        success = false;
    }
    
    gzclose(input);
    fclose(output);
    
    return success;
}

// DEFLATE Implementation (raw compression)
char* deflate_compress(const char* data, size_t input_length, size_t* output_length, CompressionLevel level) {
    if (!data || !output_length) {
        set_transcoder_error("Invalid parameters for deflate compression");
        return NULL;
    }
    
    size_t max_output = compressBound(input_length);
    char* output = malloc(max_output);
    if (!output) {
        set_transcoder_error("Memory allocation failed for deflate compression");
        return NULL;
    }
    
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = input_length;
    stream.next_in = (Bytef*)data;
    stream.avail_out = max_output;
    stream.next_out = (Bytef*)output;
    
    // Initialize deflate (raw, without headers)
    int ret = deflateInit2(&stream, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK) {
        free(output);
        set_transcoder_error("Failed to initialize deflate compression");
        return NULL;
    }
    
    ret = deflate(&stream, Z_FINISH);
    if (ret != Z_STREAM_END) {
        deflateEnd(&stream);
        free(output);
        set_transcoder_error("Deflate compression failed");
        return NULL;
    }
    
    *output_length = stream.total_out;
    deflateEnd(&stream);
    
    char* result = realloc(output, *output_length);
    return result ? result : output;
}

char* deflate_decompress(const char* compressed_data, size_t input_length, size_t* output_length) {
    if (!compressed_data || !output_length) {
        set_transcoder_error("Invalid parameters for deflate decompression");
        return NULL;
    }
    
    size_t buffer_size = input_length * 4;
    char* output = malloc(buffer_size);
    if (!output) {
        set_transcoder_error("Memory allocation failed for deflate decompression");
        return NULL;
    }
    
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    stream.avail_in = input_length;
    stream.next_in = (Bytef*)compressed_data;
    stream.avail_out = buffer_size;
    stream.next_out = (Bytef*)output;
    
    // Initialize inflate (raw, without headers)
    int ret = inflateInit2(&stream, -15);
    if (ret != Z_OK) {
        free(output);
        set_transcoder_error("Failed to initialize deflate decompression");
        return NULL;
    }
    
    do {
        ret = inflate(&stream, Z_NO_FLUSH);
        
        if (ret == Z_NEED_DICT || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&stream);
            free(output);
            set_transcoder_error("Deflate decompression failed");
            return NULL;
        }
        
        if (ret != Z_STREAM_END && stream.avail_out == 0) {
            size_t old_size = buffer_size;
            buffer_size *= 2;
            char* new_output = realloc(output, buffer_size);
            if (!new_output) {
                inflateEnd(&stream);
                free(output);
                set_transcoder_error("Memory reallocation failed during decompression");
                return NULL;
            }
            output = new_output;
            stream.next_out = (Bytef*)(output + old_size);
            stream.avail_out = buffer_size - old_size;
        }
    } while (ret != Z_STREAM_END);
    
    *output_length = stream.total_out;
    inflateEnd(&stream);
    
    char* result = realloc(output, *output_length);
    return result ? result : output;
}

// URL Encoding Implementation
char* url_encode(const char* data) {
    if (!data) {
        set_transcoder_error("Invalid data for URL encoding");
        return NULL;
    }
    
    size_t length = strlen(data);
    size_t max_output = length * 3 + 1;  // Worst case: every char becomes %XX
    char* encoded = malloc(max_output);
    if (!encoded) {
        set_transcoder_error("Memory allocation failed for URL encoding");
        return NULL;
    }
    
    size_t pos = 0;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)data[i];
        
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded[pos++] = c;
        } else {
            pos += sprintf(encoded + pos, "%%%02X", c);
        }
    }
    
    encoded[pos] = '\0';
    
    char* result = realloc(encoded, pos + 1);
    return result ? result : encoded;
}

char* url_decode(const char* encoded_data) {
    if (!encoded_data) {
        set_transcoder_error("Invalid data for URL decoding");
        return NULL;
    }
    
    size_t length = strlen(encoded_data);
    char* decoded = malloc(length + 1);
    if (!decoded) {
        set_transcoder_error("Memory allocation failed for URL decoding");
        return NULL;
    }
    
    size_t pos = 0;
    for (size_t i = 0; i < length; i++) {
        if (encoded_data[i] == '%' && i + 2 < length) {
            int value;
            if (sscanf(encoded_data + i + 1, "%2x", &value) == 1) {
                decoded[pos++] = (char)value;
                i += 2;
            } else {
                decoded[pos++] = encoded_data[i];
            }
        } else if (encoded_data[i] == '+') {
            decoded[pos++] = ' ';
        } else {
            decoded[pos++] = encoded_data[i];
        }
    }
    
    decoded[pos] = '\0';
    return decoded;
}

// HTML Entity Encoding
char* html_encode(const char* data) {
    if (!data) {
        set_transcoder_error("Invalid data for HTML encoding");
        return NULL;
    }
    
    size_t length = strlen(data);
    size_t max_output = length * 6 + 1;  // Worst case: every char becomes &#XXX;
    char* encoded = malloc(max_output);
    if (!encoded) {
        set_transcoder_error("Memory allocation failed for HTML encoding");
        return NULL;
    }
    
    size_t pos = 0;
    for (size_t i = 0; i < length; i++) {
        switch (data[i]) {
            case '<':
                pos += sprintf(encoded + pos, "&lt;");
                break;
            case '>':
                pos += sprintf(encoded + pos, "&gt;");
                break;
            case '&':
                pos += sprintf(encoded + pos, "&amp;");
                break;
            case '"':
                pos += sprintf(encoded + pos, "&quot;");
                break;
            case '\'':
                pos += sprintf(encoded + pos, "&#39;");
                break;
            default:
                encoded[pos++] = data[i];
                break;
        }
    }
    
    encoded[pos] = '\0';
    
    char* result = realloc(encoded, pos + 1);
    return result ? result : encoded;
}

// Hex Encoding
char* hex_encode(const char* data, size_t length) {
    if (!data) {
        set_transcoder_error("Invalid data for hex encoding");
        return NULL;
    }
    
    char* hex = malloc(length * 2 + 1);
    if (!hex) {
        set_transcoder_error("Memory allocation failed for hex encoding");
        return NULL;
    }
    
    for (size_t i = 0; i < length; i++) {
        sprintf(hex + i * 2, "%02x", (unsigned char)data[i]);
    }
    hex[length * 2] = '\0';
    
    return hex;
}

char* hex_decode(const char* hex_data, size_t* output_length) {
    if (!hex_data || !output_length) {
        set_transcoder_error("Invalid parameters for hex decoding");
        return NULL;
    }
    
    size_t hex_length = strlen(hex_data);
    if (hex_length % 2 != 0) {
        set_transcoder_error("Invalid hex string length");
        return NULL;
    }
    
    size_t data_length = hex_length / 2;
    char* data = malloc(data_length);
    if (!data) {
        set_transcoder_error("Memory allocation failed for hex decoding");
        return NULL;
    }
    
    for (size_t i = 0; i < data_length; i++) {
        int value;
        if (sscanf(hex_data + i * 2, "%2x", &value) != 1) {
            free(data);
            set_transcoder_error("Invalid hex character");
            return NULL;
        }
        data[i] = (char)value;
    }
    
    *output_length = data_length;
    return data;
}

// CRC32 Checksum
uint32_t crc32_checksum(const char* data, size_t length) {
    return crc32(0L, (const Bytef*)data, length);
}

// Utility Functions
size_t estimate_gzip_size(size_t input_size, CompressionLevel level) {
    // Rough estimation based on compression level
    double ratio = 0.5;  // Default 50% compression
    
    switch (level) {
        case COMPRESS_LEVEL_FASTEST:
            ratio = 0.7;
            break;
        case COMPRESS_LEVEL_FAST:
            ratio = 0.6;
            break;
        case COMPRESS_LEVEL_DEFAULT:
            ratio = 0.5;
            break;
        case COMPRESS_LEVEL_BEST:
            ratio = 0.4;
            break;
    }
    
    return (size_t)(input_size * ratio) + 18;  // Add gzip header overhead
}

double get_compression_ratio(size_t original_size, size_t compressed_size) {
    if (original_size == 0) return 0.0;
    return (double)compressed_size / (double)original_size;
}

// Error Handling
const char* transcoder_get_last_error(void) {
    return g_transcoder_error;
}

void transcoder_clear_error(void) {
    g_transcoder_error[0] = '\0';
}