#ifndef HASH_CHANGER_H
#define HASH_CHANGER_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Options for batch processing
typedef struct {
    wchar_t *target_directory;   // Target directory path
    wchar_t *self_exe_path;      // Executable's own path to exclude
    int      threads;             // Number of threads (0 for auto: min(CPU, 8))
    int      batch_size;          // Batch size (default: 5000)
    bool     interactive;         // Whether running in interactive console mode
} HashChangerOptions;

// Statistics of processing
typedef struct {
    int64_t total_files;
    int64_t ok_count;
    int64_t fail_count;
    double  elapsed_seconds;
    double  rate_files_per_sec;
} HashChangerStats;

// File item for LCN sorting and processing
typedef struct {
    wchar_t *path;
    int64_t  lcn;
} FileItem;

// Check if current process has Administrator privileges
bool IsRunningAsAdmin(void);

// Console color helpers (supports ANSI with fallback to Win32 console attributes)
void ConsoleInit(void);
void SetColorCyan(void);
void SetColorGreen(void);
void SetColorYellow(void);
void SetColorRed(void);
void ResetColor(void);

// Core APIs
// 1. Scan directory recursively for all files (excluding self)
bool ScanDirectoryFiles(const wchar_t *dir_path, const wchar_t *exclude_path, FileItem **out_items, size_t *out_count);
void FreeFileItems(FileItem *items, size_t count);

// 2. Query physical cluster (LCN) of a file; returns -1 on failure
int64_t GetPhysicalClusterLCN(const wchar_t *file_path);

// 3. Atomically append 1~4 random bytes and freeze/restore FILETIME timestamps
bool AppendDataWithFreeze(const wchar_t *file_path);

// 4. Run full batch processing
bool RunBatchHashChanger(const HashChangerOptions *options, HashChangerStats *stats);

#ifdef __cplusplus
}
#endif

#endif // HASH_CHANGER_H
