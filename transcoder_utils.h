#ifndef TRANSCODER_UTILS_H
#define TRANSCODER_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Transcoder utilities library for Kuyil
// Version: 1.0.0
// Author: KuyilTeam
// Dependencies: zlib>=1.2.11, libarchive>=3.4.0

#ifdef __cplusplus
extern "C" {
#endif

// Compression levels
typedef enum {
    COMPRESS_LEVEL_FASTEST = 1,
    COMPRESS_LEVEL_FAST = 3,
    COMPRESS_LEVEL_DEFAULT = 6,
    COMPRESS_LEVEL_BEST = 9
} CompressionLevel;

// GZIP Compression/Decompression
char* gzip_compress(const char* data, size_t input_length, size_t* output_length, CompressionLevel level);
char* gzip_decompress(const char* compressed_data, size_t input_length, size_t* output_length);
bool gzip_compress_file(const char* input_file, const char* output_file, CompressionLevel level);
bool gzip_decompress_file(const char* input_file, const char* output_file);

// DEFLATE (raw compression without headers)
char* deflate_compress(const char* data, size_t input_length, size_t* output_length, CompressionLevel level);
char* deflate_decompress(const char* compressed_data, size_t input_length, size_t* output_length);

// ZIP Archive Management
typedef struct ZipArchive ZipArchive;

ZipArchive* zip_create_archive(const char* filename);
ZipArchive* zip_open_archive(const char* filename);
void zip_close_archive(ZipArchive* archive);

bool zip_add_file(ZipArchive* archive, const char* filename, const char* arcname, CompressionLevel level);
bool zip_add_data(ZipArchive* archive, const char* arcname, const char* data, size_t data_length, CompressionLevel level);
bool zip_extract_file(ZipArchive* archive, const char* arcname, const char* output_path);
char* zip_extract_data(ZipArchive* archive, const char* arcname, size_t* output_length);

// ZIP Archive Information
int zip_get_entry_count(ZipArchive* archive);
char** zip_list_entries(ZipArchive* archive, int* count);
bool zip_entry_exists(ZipArchive* archive, const char* arcname);
size_t zip_get_entry_size(ZipArchive* archive, const char* arcname);
size_t zip_get_entry_compressed_size(ZipArchive* archive, const char* arcname);

// TAR Archive Management  
typedef struct TarArchive TarArchive;

TarArchive* tar_create_archive(const char* filename);
TarArchive* tar_open_archive(const char* filename);
void tar_close_archive(TarArchive* archive);

bool tar_add_file(TarArchive* archive, const char* filename, const char* arcname);
bool tar_add_data(TarArchive* archive, const char* arcname, const char* data, size_t data_length);
bool tar_extract_file(TarArchive* archive, const char* arcname, const char* output_path);
char* tar_extract_data(TarArchive* archive, const char* arcname, size_t* output_length);
char** tar_list_entries(TarArchive* archive, int* count);

// Base Encoding (additional to crypto's base64)
char* base32_encode(const char* data, size_t length);
char* base32_decode(const char* encoded, size_t* output_length);
char* base85_encode(const char* data, size_t length);
char* base85_decode(const char* encoded, size_t* output_length);

// URL Encoding/Decoding
char* url_encode(const char* data);
char* url_decode(const char* encoded_data);

// HTML Entity Encoding/Decoding
char* html_encode(const char* data);
char* html_decode(const char* encoded_data);

// JSON String Escaping
char* json_escape(const char* data);
char* json_unescape(const char* escaped_data);

// Hex Encoding/Decoding
char* hex_encode(const char* data, size_t length);
char* hex_decode(const char* hex_data, size_t* output_length);

// Data Validation
bool is_valid_utf8(const char* data, size_t length);
bool is_valid_json(const char* json_data);
bool is_valid_xml(const char* xml_data);
bool is_valid_base64(const char* data);

// Checksum Functions
uint32_t crc32_checksum(const char* data, size_t length);
char* adler32_checksum(const char* data, size_t length);

// Utility Functions
size_t estimate_gzip_size(size_t input_size, CompressionLevel level);
size_t estimate_zip_size(size_t input_size, CompressionLevel level);
double get_compression_ratio(size_t original_size, size_t compressed_size);

// Streaming Compression (for large files)
typedef struct CompressStream CompressStream;

CompressStream* compress_stream_create(CompressionLevel level, bool gzip_format);
bool compress_stream_write(CompressStream* stream, const char* data, size_t length);
char* compress_stream_finish(CompressStream* stream, size_t* output_length);
void compress_stream_free(CompressStream* stream);

// Error Handling
const char* transcoder_get_last_error(void);
void transcoder_clear_error(void);

// Simple ZIP extraction helper
// Extracts all files from zip_path into dest_dir (creates dirs as needed).
// Returns a newly allocated string listing extracted file paths separated by newlines on success,
// or NULL on failure (check transcoder_get_last_error()).
char* unzip_to_directory(const char* zip_path, const char* dest_dir);

#ifdef __cplusplus
}
#endif

#endif // TRANSCODER_UTILS_H