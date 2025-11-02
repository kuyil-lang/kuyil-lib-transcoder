#define _XOPEN_SOURCE 600
#include "transcoder_utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
#include <archive.h>
#include <archive_entry.h>
#include <ctype.h>
#include <sys/stat.h>
#include <errno.h>

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

// Ensure directory exists (mkdir -p like) for a given path
static int ensure_directory_for_path(const char* fullpath) {
    if (!fullpath) return -1;
    char* path = strdup(fullpath);
    if (!path) return -1;
    // Turn trailing filename into directory path
    char* p = strrchr(path, '/');
    if (p) {
        *p = '\0';
    }
    // Create directories recursively
    char* iter = path;
    if (iter[0] == '\0') { free(path); return 0; }
    if (iter[0] == '/' ) iter++;
    for (; *iter; ++iter) {
        if (*iter == '/') {
            *iter = '\0';
            if (path[0] != '\0') {
                if (mkdir(path, 0755) != 0 && errno != EEXIST) { *iter = '/'; free(path); return -1; }
            }
            *iter = '/';
        }
    }
    if (mkdir(path, 0755) != 0 && errno != EEXIST) { free(path); return -1; }
    free(path);
    return 0;
}

// Copy data blocks from archive to disk writer
static int copy_data(struct archive* ar, struct archive* aw) {
    const void* buff;
    size_t size;
    la_int64_t offset;
    int r;
    for (;;) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if (r == ARCHIVE_EOF) return ARCHIVE_OK;
        if (r != ARCHIVE_OK) return r;
        r = archive_write_data_block(aw, buff, size, offset);
        if (r != ARCHIVE_OK) {
            return r;
        }
    }
}

// Join two path segments into a newly allocated string
static char* join_path(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    int need_slash = (la > 0 && a[la-1] != '/');
    char* out = (char*)malloc(la + need_slash + lb + 1);
    if (!out) return NULL;
    memcpy(out, a, la);
    size_t pos = la;
    if (need_slash) out[pos++] = '/';
    memcpy(out + pos, b, lb);
    out[pos + lb] = '\0';
    return out;
}

// Append a line to a dynamically growing buffer (newline separated)
static int append_line(char** buf, size_t* cap, size_t* len, const char* line) {
    size_t ll = strlen(line);
    size_t need = ll + 1; // plus newline
    if (*cap < *len + need + 1) { // +1 for terminator
        size_t ncap = (*cap == 0 ? 1024 : (*cap * 2));
        while (ncap < *len + need + 1) ncap *= 2;
        char* nbuf = (char*)realloc(*buf, ncap);
        if (!nbuf) return -1;
        *buf = nbuf;
        *cap = ncap;
    }
    memcpy(*buf + *len, line, ll);
    *len += ll;
    (*buf)[(*len)++] = '\n';
    (*buf)[*len] = '\0';
    return 0;
}

// Public API: unzip entire archive to directory, return list of extracted files
char* unzip_to_directory(const char* zip_path, const char* dest_dir) {
    transcoder_clear_error();
    // Debug: log input paths
    FILE* dbg = fopen("/tmp/unzip_c_debug.txt", "a");
    if (dbg) {
        fprintf(dbg, "unzip_to_directory called with zip_path='%s', dest_dir='%s'\n", zip_path ? zip_path : "(null)", dest_dir ? dest_dir : "(null)");
        fclose(dbg);
    }
    if (!zip_path || !dest_dir) {
        set_transcoder_error("unzip_to_directory: invalid arguments");
        return NULL;
    }

    struct archive* a = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    struct archive_entry* entry;
    char* out_list = NULL; size_t cap = 0, len = 0;

    if (!a || !ext) {
        set_transcoder_error("Failed to allocate archive structures");
        if (a) archive_read_free(a);
        if (ext) archive_write_free(ext);
        return NULL;
    }

    archive_read_support_format_zip(a);
    archive_read_support_filter_all(a);
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS);

    if (archive_read_open_filename(a, zip_path, 10240) != ARCHIVE_OK) {
        set_transcoder_error("Failed to open ZIP file");
        archive_read_free(a);
        archive_write_free(ext);
        return NULL;
    }

    // Ensure base dest dir exists
    char* keep_path = join_path(dest_dir, ".keep");
    if (!keep_path) {
        set_transcoder_error("Out of memory while joining path");
        archive_read_free(a);
        archive_write_free(ext);
        return NULL;
    }
    int dir_result = ensure_directory_for_path(keep_path);
    free(keep_path);
    if (dir_result != 0) {
        set_transcoder_error("Failed to create destination directory");
        archive_read_free(a);
        archive_write_free(ext);
        return NULL;
    }

    int r;
    while ((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        const char* current = archive_entry_pathname(entry);
        if (!current) { archive_read_data_skip(a); continue; }
        // Build full path under dest_dir
        char* fullpath = join_path(dest_dir, current);
        if (!fullpath) { set_transcoder_error("Out of memory"); archive_read_free(a); archive_write_free(ext); free(out_list); return NULL; }

        // Make sure the directory for this entry exists
        if (ensure_directory_for_path(fullpath) != 0) {
            free(fullpath);
            set_transcoder_error("Failed to create directory for entry");
            archive_read_free(a);
            archive_write_free(ext);
            free(out_list);
            return NULL;
        }

        // Override extraction path to our target
        archive_entry_set_pathname(entry, fullpath);
        r = archive_write_header(ext, entry);
        if (r == ARCHIVE_OK) {
            if (archive_entry_size(entry) > 0) {
                r = copy_data(a, ext);
                if (r != ARCHIVE_OK) {
                    free(fullpath);
                    set_transcoder_error("Failed to write entry data");
                    archive_read_free(a);
                    archive_write_free(ext);
                    free(out_list);
                    return NULL;
                }
            }
            archive_write_finish_entry(ext);
        } else {
            // Failed to write header; skip entry
            free(fullpath);
            continue;
        }

        // Record only regular files in output list
        mode_t ftype = archive_entry_filetype(entry);
        if (ftype == AE_IFREG) {
            // Convert fullpath to relative (dest_dir prefix removed) for listing readability
            const char* rel = fullpath;
            size_t ddlen = strlen(dest_dir);
            if (strncmp(fullpath, dest_dir, ddlen) == 0) {
                if (fullpath[ddlen] == '/') rel = fullpath + ddlen + 1; else if (fullpath[ddlen] == '\0') rel = fullpath + ddlen; else rel = fullpath;
            }
            // If rel is empty, still append fullpath
            if (rel == NULL || *rel == '\0') rel = fullpath;
            if (append_line(&out_list, &cap, &len, rel) != 0) {
                free(fullpath);
                set_transcoder_error("Out of memory while listing files");
                archive_read_free(a);
                archive_write_free(ext);
                free(out_list);
                return NULL;
            }
        }

        free(fullpath);
    }

    archive_read_free(a);
    archive_write_free(ext);

    // Debug: log output list
    dbg = fopen("/tmp/unzip_c_debug.txt", "a");
    if (dbg) {
        fprintf(dbg, "unzip_to_directory output list: '%s'\n", out_list ? out_list : "(null)");
        fclose(dbg);
    }
    if (!out_list) {
        // return empty string if nothing extracted
        out_list = strdup("");
    }
    return out_list;
}