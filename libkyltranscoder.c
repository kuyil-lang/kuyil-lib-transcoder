// Kuyil Bridge for Transcoder Library
#include "transcoder_utils.h"
#include "../../src/vm.h"
#include <string.h>
#include <stdlib.h>

// Wrapper functions to match Kuyil's expected naming

// Local safe strdup to avoid portability issues
static char* kstrdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char* out = (char*)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

// GZIP compression - returns object with data (byte array) and length
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
    
    size_t compressed_length;
    char* compressed = gzip_compress(data, input_length, &compressed_length, level);
    
    if (!compressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    // Create byte array from compressed data
    Value* byte_array = malloc(compressed_length * sizeof(Value));
    for (size_t i = 0; i < compressed_length; i++) {
        byte_array[i].type = VALUE_NUMBER;
        byte_array[i].as.number = (unsigned char)compressed[i];
    }
    
    // Create object with data and length fields
    Value result = {VALUE_OBJECT};
    result.as.object.count = 2;
    result.as.object.keys = malloc(2 * sizeof(char*));
    result.as.object.values = malloc(2 * sizeof(Value));
    
    // data field: byte array
    result.as.object.keys[0] = kstrdup("data");
    result.as.object.values[0].type = VALUE_ARRAY;
    result.as.object.values[0].as.array.count = compressed_length;
    result.as.object.values[0].as.array.values = byte_array;
    
    // length field: number
    result.as.object.keys[1] = kstrdup("length");
    result.as.object.values[1].type = VALUE_NUMBER;
    result.as.object.values[1].as.number = compressed_length;
    
    free(compressed);
    return result;
}

// GZIP decompression - expects object with data (byte array) and length
Value decompress_gzip(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_OBJECT) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    ValueObject obj = args[0].as.object;
    
    // Find data and length fields
    Value* data_array = NULL;
    size_t compressed_length = 0;
    
    for (int i = 0; i < obj.count; i++) {
        if (strcmp(obj.keys[i], "data") == 0 && obj.values[i].type == VALUE_ARRAY) {
            data_array = &obj.values[i];
        } else if (strcmp(obj.keys[i], "length") == 0 && obj.values[i].type == VALUE_NUMBER) {
            compressed_length = (size_t)obj.values[i].as.number;
        }
    }
    
    if (!data_array || compressed_length == 0) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    // Convert byte array to binary data
    char* compressed_data = malloc(compressed_length);
    for (size_t i = 0; i < compressed_length; i++) {
        if (data_array->as.array.values[i].type != VALUE_NUMBER) {
            free(compressed_data);
            Value error = {VALUE_NIL};
            return error;
        }
        compressed_data[i] = (char)(int)data_array->as.array.values[i].as.number;
    }
    
    size_t output_length;
    char* raw = gzip_decompress(compressed_data, compressed_length, &output_length);
    free(compressed_data);
    
    if (!raw) {
        Value error = {VALUE_NIL};
        return error;
    }
    // Ensure null-termination for VM string
    char* decompressed = (char*)malloc(output_length + 1);
    if (!decompressed) {
        free(raw);
        Value error = {VALUE_NIL};
        return error;
    }
    memcpy(decompressed, raw, output_length);
    decompressed[output_length] = '\0';
    free(raw);
    
    Value result = {VALUE_STRING};
    result.as.string = decompressed;
    return result;
}

// DEFLATE compression - returns object with data (byte array) and length
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
    
    size_t compressed_length;
    char* compressed = deflate_compress(data, input_length, &compressed_length, level);
    
    if (!compressed) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    // Create byte array from compressed data
    Value* byte_array = malloc(compressed_length * sizeof(Value));
    for (size_t i = 0; i < compressed_length; i++) {
        byte_array[i].type = VALUE_NUMBER;
        byte_array[i].as.number = (unsigned char)compressed[i];
    }
    
    // Create object with data and length fields
    Value result = {VALUE_OBJECT};
    result.as.object.count = 2;
    result.as.object.keys = malloc(2 * sizeof(char*));
    result.as.object.values = malloc(2 * sizeof(Value));
    
    // data field: byte array
    result.as.object.keys[0] = kstrdup("data");
    result.as.object.values[0].type = VALUE_ARRAY;
    result.as.object.values[0].as.array.count = compressed_length;
    result.as.object.values[0].as.array.values = byte_array;
    
    // length field: number
    result.as.object.keys[1] = kstrdup("length");
    result.as.object.values[1].type = VALUE_NUMBER;
    result.as.object.values[1].as.number = compressed_length;
    
    free(compressed);
    return result;
}

// DEFLATE decompression - expects object with data (byte array) and length
Value decompress_deflate(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_OBJECT) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    ValueObject obj = args[0].as.object;
    
    // Find data and length fields
    Value* data_array = NULL;
    size_t compressed_length = 0;
    
    for (int i = 0; i < obj.count; i++) {
        if (strcmp(obj.keys[i], "data") == 0 && obj.values[i].type == VALUE_ARRAY) {
            data_array = &obj.values[i];
        } else if (strcmp(obj.keys[i], "length") == 0 && obj.values[i].type == VALUE_NUMBER) {
            compressed_length = (size_t)obj.values[i].as.number;
        }
    }
    
    if (!data_array || compressed_length == 0) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    // Convert byte array to binary data
    char* compressed_data = malloc(compressed_length);
    for (size_t i = 0; i < compressed_length; i++) {
        if (data_array->as.array.values[i].type != VALUE_NUMBER) {
            free(compressed_data);
            Value error = {VALUE_NIL};
            return error;
        }
        compressed_data[i] = (char)(int)data_array->as.array.values[i].as.number;
    }
    
    size_t output_length;
    char* raw = deflate_decompress(compressed_data, compressed_length, &output_length);
    free(compressed_data);
    
    if (!raw) {
        Value error = {VALUE_NIL};
        return error;
    }
    // Ensure null-termination for VM string
    char* decompressed = (char*)malloc(output_length + 1);
    if (!decompressed) {
        free(raw);
        Value error = {VALUE_NIL};
        return error;
    }
    memcpy(decompressed, raw, output_length);
    decompressed[output_length] = '\0';
    free(raw);
    
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
// Compress a string to a zip archive (returns zip data as string)
Value compress_zip(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    const char* input = args[0].as.string;
    size_t input_length = strlen(input);
    // Create a zip archive in memory
    ZipArchive* archive = zip_create_archive(NULL); // NULL for in-memory
    if (!archive) {
        Value error = {VALUE_NIL};
        return error;
    }
    // Add the string as a file named "data.txt"
    bool ok = zip_add_data(archive, "data.txt", input, input_length, COMPRESS_LEVEL_DEFAULT);
    if (!ok) {
        zip_close_archive(archive);
        Value error = {VALUE_NIL};
        return error;
    }
    // Extract the zip archive as a string
    size_t zip_length = 0;
    char* zip_data = zip_extract_data(archive, NULL, &zip_length); // NULL for whole archive
    zip_close_archive(archive);
    if (!zip_data) {
        Value error = {VALUE_NIL};
        return error;
    }
    Value result = {VALUE_STRING};
    result.as.string = zip_data;
    return result;
}

// Decompress zip data and return the first file's contents as string
Value decompress_zip(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    const char* zip_data = args[0].as.string;
    size_t zip_length = strlen(zip_data);
    // Open the zip archive from memory
    ZipArchive* archive = zip_open_archive(zip_data);
    if (!archive) {
        Value error = {VALUE_NIL};
        return error;
    }
    // Get the first entry name
    int entry_count = zip_get_entry_count(archive);
    if (entry_count < 1) {
        zip_close_archive(archive);
        Value error = {VALUE_NIL};
        return error;
    }
    char** entries = zip_list_entries(archive, NULL);
    if (!entries || !entries[0]) {
        zip_close_archive(archive);
        Value error = {VALUE_NIL};
        return error;
    }
    // Extract the first file
    size_t out_len = 0;
    char* file_data = zip_extract_data(archive, entries[0], &out_len);
    zip_close_archive(archive);
    if (!file_data) {
        Value error = {VALUE_NIL};
        return error;
    }
    Value result = {VALUE_STRING};
    result.as.string = file_data;
    return result;
}

// ZIP: extract all files to directory, return newline-separated list of files
// Unzip a zip file to a directory, return true if successful
Value transcode_unzip_to_directory(int arg_count, Value* args) {
    Value vfalse = {VALUE_BOOL};
    vfalse.as.boolean = false;
    if (arg_count < 2 || args[0].type != VALUE_STRING || args[1].type != VALUE_STRING) {
        return vfalse;
    }
    const char* zip_path = args[0].as.string;
    const char* dest_dir = args[1].as.string;
    ZipArchive* archive = zip_open_archive(zip_path);
    if (!archive) {
        return vfalse;
    }
    int entry_count = zip_get_entry_count(archive);
    if (entry_count < 1) {
        zip_close_archive(archive);
        return vfalse;
    }
    char** entries = zip_list_entries(archive, NULL);
    if (!entries) {
        zip_close_archive(archive);
        return vfalse;
    }
    bool all_ok = true;
    for (int i = 0; i < entry_count; i++) {
        if (!zip_extract_file(archive, entries[i], dest_dir)) {
            all_ok = false;
            break;
        }
    }
    zip_close_archive(archive);
    Value vret = {VALUE_BOOL};
    vret.as.boolean = all_ok;
    return vret;
}
