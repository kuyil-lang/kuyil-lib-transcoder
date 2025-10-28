// Kuyil Bridge for Transcoder Library
#include "transcoder_utils.h"
#include "../../src/vm.h"
#include <string.h>
#include <stdlib.h>

// Wrapper functions to match Kuyil's expected naming

// GZIP compression
Value compress_gzip(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* data = args[0].as.string;
    size_t input_length = strlen(data);
    CompressionLevel level = COMPRESS_LEVEL_DEFAULT;
    
    if (arg_count >= 2 && args[1].type == VALUE_NUMBER) {
        level = (CompressionLevel)(int)args[1].as.number;
    }
    
    size_t output_length;
    char* compressed = gzip_compress(data, input_length, &output_length, level);
    
    if (!compressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = compressed;
    return result;
}

// GZIP decompression
Value decompress_gzip(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* compressed_data = args[0].as.string;
    size_t input_length = strlen(compressed_data);
    size_t output_length;
    
    char* decompressed = gzip_decompress(compressed_data, input_length, &output_length);
    
    if (!decompressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = decompressed;
    return result;
}

// DEFLATE compression
Value compress_deflate(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* data = args[0].as.string;
    size_t input_length = strlen(data);
    CompressionLevel level = COMPRESS_LEVEL_DEFAULT;
    
    if (arg_count >= 2 && args[1].type == VALUE_NUMBER) {
        level = (CompressionLevel)(int)args[1].as.number;
    }
    
    size_t output_length;
    char* compressed = deflate_compress(data, input_length, &output_length, level);
    
    if (!compressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = compressed;
    return result;
}

// DEFLATE decompression
Value decompress_deflate(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* compressed_data = args[0].as.string;
    size_t input_length = strlen(compressed_data);
    size_t output_length;
    
    char* decompressed = deflate_decompress(compressed_data, input_length, &output_length);
    
    if (!decompressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = decompressed;
    return result;
}

// URL encoding
Value transcode_url_encode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* encoded = url_encode(args[0].as.string);
    
    if (!encoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = encoded;
    return result;
}

// URL decoding
Value transcode_url_decode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* decoded = url_decode(args[0].as.string);
    
    if (!decoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = decoded;
    return result;
}

// HTML encoding
Value transcode_html_encode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* encoded = html_encode(args[0].as.string);
    
    if (!encoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = encoded;
    return result;
}

// Hex encoding
Value transcode_hex_encode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* data = args[0].as.string;
    size_t length = strlen(data);
    
    char* encoded = hex_encode(data, length);
    
    if (!encoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = encoded;
    return result;
}

// Hex decoding
Value transcode_hex_decode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    size_t output_length;
    char* decoded = hex_decode(args[0].as.string, &output_length);
    
    if (!decoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result = {VALUE_STRING};
    result.as.string = decoded;
    return result;
}

// CRC32 checksum
Value transcode_crc32(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NUMBER};
        error.as.number = 0;
        return error;
    }
    
    const char* data = args[0].as.string;
    size_t length = strlen(data);
    
    uint32_t checksum = crc32_checksum(data, length);
    
    Value result = {VALUE_NUMBER};
    result.as.number = (double)checksum;
    return result;
}

// Get compression ratio
Value transcode_compression_ratio(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_NUMBER) {
        Value error = {VALUE_NUMBER};
        error.as.number = 0.0;
        return error;
    }
    
    size_t original_size = (size_t)args[0].as.number;
    size_t compressed_size = (size_t)args[1].as.number;
    
    double ratio = get_compression_ratio(original_size, compressed_size);
    
    Value result = {VALUE_NUMBER};
    result.as.number = ratio;
    return result;
}

// Dummy functions for compatibility
Value compress_zip(int arg_count, Value* args) {
    (void)arg_count; (void)args;
    Value error = {VALUE_NIL};
    return error;
}

Value decompress_zip(int arg_count, Value* args) {
    (void)arg_count; (void)args;
    Value error = {VALUE_NIL};
    return error;
}
