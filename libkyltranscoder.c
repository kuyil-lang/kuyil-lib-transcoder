// Kuyil Bridge for Transcoder Library
#include <stdio.h>
#include "transcoder_utils.h"
#include "../../src/ast.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
static char* json_get_field_impl(const char* json, const char* key);

// Kuyil interface signature metadata
__attribute__((visibility("default")))
const char* kyl_interface_signature_text = 
    "compress compressGzip(input: string) -> object\n"
    "compress compressDeflate(input: string) -> object\n"
    "compress compressZip(input: string) -> string\n"
    "decompress decompressGzip(input: object) -> string\n"
    "decompress decompressDeflate(input: object) -> string\n"
    "decompress decompressZip(input: string) -> string\n"
    "transcode urlEncode(input: string) -> string\n"
    "transcode urlDecode(input: string) -> string\n"
    "transcode htmlEncode(input: string) -> string\n"
    "transcode hexEncode(input: string) -> string\n"
    "transcode hexDecode(input: string) -> string\n"
    "transcode crc32(input: string) -> int32\n"
    "transcode compressionRatio(original: int32, compressed: int32) -> float64\n"
    "transcode jsonParse(json: string) -> object\n"
    "transcode jsonStringify(input: string) -> string\n"
    "transcode jsonGetField(json: string, key: string) -> string\n"
    "unzip toDirectory(zipPath: string) -> bool\n";

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
Value compress_compressGzip(int arg_count, Value* args) {
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
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_OBJECT;
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
Value decompress_decompressGzip(int arg_count, Value* args) {
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
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = decompressed;
    return result;
}

// DEFLATE compression - returns object with data (byte array) and length
Value compress_compressDeflate(int arg_count, Value* args) {
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
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_OBJECT;
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
Value decompress_decompressDeflate(int arg_count, Value* args) {
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
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = decompressed;
    return result;
}

// URL encoding
Value transcode_urlEncode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* encoded = url_encode(args[0].as.string);
    
    if (!encoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = encoded;
    return result;
}

// URL decoding
Value transcode_urlDecode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* decoded = url_decode(args[0].as.string);
    
    if (!decoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = decoded;
    return result;
}

// HTML encoding
Value transcode_htmlEncode(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* encoded = html_encode(args[0].as.string);
    
    if (!encoded) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = encoded;
    return result;
}

// Hex encoding
Value transcode_hexEncode(int arg_count, Value* args) {
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
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = encoded;
    return result;
}

// Hex decoding
Value transcode_hexDecode(int arg_count, Value* args) {
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
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = decoded;
    return result;
}

// CRC32 checksum
// NOTE: Avoid symbol collision with zlib's crc32 by exporting as kyl_crc32
Value transcode_crc32(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NUMBER};
        error.as.number = 0;
        return error;
    }
    
    const char* data = args[0].as.string;
    size_t length = strlen(data);
    
    uint32_t checksum = crc32_checksum(data, length);
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_NUMBER;
    result.as.number = (double)checksum;
    return result;
}

// Get compression ratio
Value transcode_compressionRatio(int arg_count, Value* args) {
    if (arg_count < 2 || args[0].type != VALUE_NUMBER || args[1].type != VALUE_NUMBER) {
        Value error = {VALUE_NUMBER};
        error.as.number = 0.0;
        return error;
    }
    
    size_t original_size = (size_t)args[0].as.number;
    size_t compressed_size = (size_t)args[1].as.number;
    
    double ratio = get_compression_ratio(original_size, compressed_size);
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_NUMBER;
    result.as.number = ratio;
    return result;
}

// JSON parser - converts JSON string to Value map/object
Value transcode_jsonParse(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value result;
        memset(&result, 0, sizeof(Value));
        result.type = VALUE_NIL;
        return result;
    }
    
    const char* json = args[0].as.string;
    
    // Simple JSON parser for flat objects: {"key":"value","key2":"value2"}
    // Allocate result as map/object
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_OBJECT;
    result.as.object.count = 0;
    result.as.object.keys = NULL;
    result.as.object.values = NULL;
    
    // Skip leading whitespace and opening brace
    const char* p = json;
    while (*p && isspace(*p)) p++;
    if (*p != '{') {
        result.type = VALUE_NIL;
        return result;
    }
    p++;
    
    // Count fields by parsing - actually walk through and count key:value pairs
    int field_count = 0;
    const char* scan = p;
    
    while (*scan && *scan != '}') {
        // Skip whitespace
        while (*scan && isspace(*scan)) scan++;
        if (*scan == '}' || *scan == '\0') break;
        if (*scan == ',') { scan++; continue; }
        
        // Must start with a quote for a key
        if (*scan != '"') break;
        
        // Found a key - skip the key string
        scan++;
        while (*scan && *scan != '"') {
            if (*scan == '\\' && *(scan+1)) scan += 2;
            else scan++;
        }
        if (*scan != '"') break;
        scan++;
        
        // Skip colon
        while (*scan && (isspace(*scan) || *scan == ':')) scan++;
        
        // Skip the value
        if (*scan == '"') {
            // String value
            scan++;
            while (*scan && *scan != '"') {
                if (*scan == '\\' && *(scan+1)) scan += 2;
                else scan++;
            }
            if (*scan == '"') scan++;
        } else if (*scan == '{' || *scan == '[') {
            // Nested object/array - skip to matching close
            char open = *scan;
            char close = (open == '{') ? '}' : ']';
            int depth = 1;
            scan++;
            while (*scan && depth > 0) {
                if (*scan == '"') {
                    scan++;
                    while (*scan && *scan != '"') {
                        if (*scan == '\\' && *(scan+1)) scan += 2;
                        else scan++;
                    }
                    if (*scan == '"') scan++;
                } else if (*scan == open) {
                    depth++;
                    scan++;
                } else if (*scan == close) {
                    depth--;
                    scan++;
                } else {
                    scan++;
                }
            }
        } else if (strncmp(scan, "true", 4) == 0) {
            scan += 4;
        } else if (strncmp(scan, "false", 5) == 0) {
            scan += 5;
        } else if (strncmp(scan, "null", 4) == 0) {
            scan += 4;
        } else if (isdigit(*scan) || *scan == '-') {
            // Number
            if (*scan == '-') scan++;
            while (*scan && isdigit(*scan)) scan++;
            if (*scan == '.') {
                scan++;
                while (*scan && isdigit(*scan)) scan++;
            }
        } else {
            // Unknown value, bail
            break;
        }
        
        field_count++;
        
        // Skip any whitespace and comma
        while (*scan && (isspace(*scan) || *scan == ',')) scan++;
    }

    
    if (field_count == 0) {
        return result;  // Empty object
    }
    
    // Allocate arrays and initialize to zero
    result.as.object.keys = malloc(sizeof(char*) * field_count);
    result.as.object.values = malloc(sizeof(Value) * field_count);
    memset(result.as.object.keys, 0, sizeof(char*) * field_count);
    memset(result.as.object.values, 0, sizeof(Value) * field_count);
    result.as.object.count = 0;
    
    // Parse fields
    while (*p && *p != '}') {
        // Skip whitespace
        while (*p && isspace(*p)) p++;
        if (*p == '}' || *p == '\0') break;
        if (*p == ',') { p++; continue; }
        
        // Parse key
        if (*p != '"') break;
        p++;
        const char* key_start = p;
        while (*p && *p != '"') p++;
        if (*p != '"') break;
        
        size_t key_len = p - key_start;
        char* key = malloc(key_len + 1);
        memcpy(key, key_start, key_len);
        key[key_len] = '\0';
        p++;
        
        // Skip colon
        while (*p && (isspace(*p) || *p == ':')) p++;
        
        // Parse value - handle strings, objects, arrays, numbers, booleans, null
        Value val_obj;
        memset(&val_obj, 0, sizeof(Value));
        
        if (*p == '"') {
            // String value - handle escaped quotes
            p++;
            const char* val_start = p;
            while (*p && !(*p == '"' && *(p-1) != '\\')) {
                if (*p == '\\' && *(p+1) == '"') p += 2;  // Skip escaped quote
                else p++;
            }
            if (*p != '"') {
                free(key);
                break;
            }
            
            size_t val_len = p - val_start;
            char* val = malloc(val_len + 1);
            memcpy(val, val_start, val_len);
            val[val_len] = '\0';
            p++;
            
            val_obj.type = VALUE_STRING;
            val_obj.as.string = val;
        } else if (*p == '{' || *p == '[') {
            // Nested object or array - capture as string
            char open = *p;
            char close = (open == '{') ? '}' : ']';
            const char* val_start = p;
            int depth = 0;
            while (*p) {
                if (*p == '"') {
                    p++;
                    while (*p && !(*p == '"' && *(p-1) != '\\')) p++;
                    if (*p == '"') p++;
                } else if (*p == open) {
                    depth++;
                    p++;
                } else if (*p == close) {
                    depth--;
                    p++;
                    if (depth == 0) break;
                } else {
                    p++;
                }
            }
            
            size_t val_len = p - val_start;
            char* val = malloc(val_len + 1);
            memcpy(val, val_start, val_len);
            val[val_len] = '\0';
            
            val_obj.type = VALUE_STRING;
            val_obj.as.string = val;
        } else if (strncmp(p, "true", 4) == 0) {
            val_obj.type = VALUE_BOOL;
            val_obj.as.boolean = true;
            p += 4;
        } else if (strncmp(p, "false", 5) == 0) {
            val_obj.type = VALUE_BOOL;
            val_obj.as.boolean = false;
            p += 5;
        } else if (strncmp(p, "null", 4) == 0) {
            val_obj.type = VALUE_NIL;
            p += 4;
        } else if (isdigit(*p) || *p == '-') {
            // Number
            const char* num_start = p;
            if (*p == '-') p++;
            while (*p && isdigit(*p)) p++;
            if (*p == '.') {
                p++;
                while (*p && isdigit(*p)) p++;
            }
            
            size_t num_len = p - num_start;
            char* num_str = malloc(num_len + 1);
            memcpy(num_str, num_start, num_len);
            num_str[num_len] = '\0';
            
            val_obj.type = VALUE_NUMBER;
            val_obj.as.number = atof(num_str);
            free(num_str);
        } else {
            // Unknown value type, skip it
            free(key);
            while (*p && *p != ',' && *p != '}') p++;
            continue;
        }
        
        // Add to result
        int idx = result.as.object.count++;
        result.as.object.keys[idx] = key;
        result.as.object.values[idx] = val_obj;
    }
    
    return result;
}

// JSON get field - extracts string value for a key from flat JSON
Value transcode_jsonGetField(int arg_count, Value* args) {
    printf("[DEBUG transcode_jsonGetField] arg_count=%d\n", arg_count);
    if (arg_count >= 1) printf("[DEBUG] args[0].type=%d\n", args[0].type);
    if (arg_count >= 2) printf("[DEBUG] args[1].type=%d\n", args[1].type);
    if (arg_count < 2 || args[0].type != VALUE_STRING || args[1].type != VALUE_STRING) {
        Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
        result.as.string = kstrdup("");
        return result;
    }
    
    const char* json = args[0].as.string;
    const char* key = args[1].as.string;
    
    char* value = json_get_field_impl(json, key);
    
    if (!value) {
        Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
        result.as.string = kstrdup("");
        return result;
    }
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = value;
    return result;
}

// Dummy functions for compatibility
// Compress a string to a zip archive (returns zip data as string)
Value compress_compressZip(int arg_count, Value* args) {
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
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = zip_data;
    return result;
}

// Decompress zip data and return the first file's contents as string
Value decompress_decompressZip(int arg_count, Value* args) {
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
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = file_data;
    return result;
}

// ZIP: extract all files to directory, return newline-separated list of files
// Unzip a zip file to a directory, return true if successful
Value unzip_toDirectory(int arg_count, Value* args) {
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

// JSON stringify - properly escapes string for JSON
Value transcode_jsonStringify(int arg_count, Value* args) {
    if (arg_count < 1 || args[0].type != VALUE_STRING) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    const char* input = args[0].as.string;
    size_t input_len = strlen(input);
    
    // Calculate output size (worst case: every char needs escaping + quotes)
    size_t output_size = input_len * 6 + 3; // \uXXXX for each char + quotes + null
    char* output = (char*)malloc(output_size);
    if (!output) {
        Value error = {VALUE_NIL};
        return error;
    }
    
    char* out = output;
    *out++ = '"'; // Opening quote
    
    for (size_t i = 0; i < input_len; i++) {
        unsigned char c = (unsigned char)input[i];
        
        switch (c) {
            case '"':  *out++ = '\\'; *out++ = '"'; break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\b': *out++ = '\\'; *out++ = 'b'; break;
            case '\f': *out++ = '\\'; *out++ = 'f'; break;
            case '\n': *out++ = '\\'; *out++ = 'n'; break;
            case '\r': *out++ = '\\'; *out++ = 'r'; break;
            case '\t': *out++ = '\\'; *out++ = 't'; break;
            default:
                if (c < 0x20) {
                    // Control characters - use \uXXXX
                    out += sprintf(out, "\\u%04x", c);
                } else if (c >= 0x80) {
                    // UTF-8 multibyte - pass through as-is
                    *out++ = c;
                } else {
                    // Regular ASCII
                    *out++ = c;
                }
                break;
        }
    }
    
    *out++ = '"'; // Closing quote
    *out = '\0';
    
    Value result;
    memset(&result, 0, sizeof(Value));
    result.type = VALUE_STRING;
    result.as.string = output;
    return result;
}

// Local JSON field extractor (flat JSON, no escaped quotes)
static char* json_get_field_impl(const char* json, const char* key) {
    if (!json || !key) {
        return NULL;
    }
    size_t key_len = strlen(key);
    size_t marker_len = key_len + 4; // "key":"
    char* marker = (char*)malloc(marker_len + 1);
    if (!marker) {
        return NULL;
    }
    snprintf(marker, marker_len + 1, "\"%s\":\"", key);
    const char* start = strstr(json, marker);
    free(marker);
    if (!start) {
        return kstrdup("");
    }
    start += marker_len;
    const char* end = start;
    while (*end && *end != '"') {
        end++;
    }
    size_t value_len = (size_t)(end - start);
    char* out = (char*)malloc(value_len + 1);
    if (!out) {
        return NULL;
    }
    memcpy(out, start, value_len);
    out[value_len] = '\0';
    return out;
}
