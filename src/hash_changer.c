#include "hash_changer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

// Global console handle and attributes
static HANDLE s_hConsole = INVALID_HANDLE_VALUE;
static WORD   s_origAttributes = 0;
static bool   s_supportsAnsi = false;

// Initialize console for UTF-8 and Colors
void ConsoleInit(void)
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    s_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (s_hConsole != INVALID_HANDLE_VALUE && s_hConsole != NULL)
    {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(s_hConsole, &csbi))
        {
            s_origAttributes = csbi.wAttributes;
        }

        DWORD mode = 0;
        if (GetConsoleMode(s_hConsole, &mode))
        {
            if (SetConsoleMode(s_hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING))
            {
                s_supportsAnsi = true;
            }
        }
    }
}

void SetColorCyan(void)
{
    if (s_supportsAnsi) {
        fputs("\033[96m", stdout);
    } else if (s_hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(s_hConsole, FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
    }
}

void SetColorGreen(void)
{
    if (s_supportsAnsi) {
        fputs("\033[92m", stdout);
    } else if (s_hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(s_hConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
}

void SetColorYellow(void)
{
    if (s_supportsAnsi) {
        fputs("\033[93m", stdout);
    } else if (s_hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(s_hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
}

void SetColorRed(void)
{
    if (s_supportsAnsi) {
        fputs("\033[91m", stdout);
    } else if (s_hConsole != INVALID_HANDLE_VALUE) {
        SetConsoleTextAttribute(s_hConsole, FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
}

void ResetColor(void)
{
    if (s_supportsAnsi) {
        fputs("\033[0m", stdout);
    } else if (s_hConsole != INVALID_HANDLE_VALUE && s_origAttributes != 0) {
        SetConsoleTextAttribute(s_hConsole, s_origAttributes);
    }
}

// Check administrator permissions
bool IsRunningAsAdmin(void)
{
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
    {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin ? true : false;
}

// Check if file extension is an executable / driver / signature-sensitive binary
bool IsExecutableFile(const wchar_t *file_path)
{
    if (!file_path) return false;
    const wchar_t *dot = wcsrchr(file_path, L'.');
    if (!dot) return false;

    static const wchar_t *kExecExts[] = {
        L".exe", L".dll", L".sys", L".msi", L".ocx",
        L".cpl", L".scr", L".efi", L".drv", L".mui",
        L".ax",  L".com"
    };

    for (size_t i = 0; i < sizeof(kExecExts) / sizeof(kExecExts[0]); i++)
    {
        if (_wcsicmp(dot, kExecExts[i]) == 0) {
            return true;
        }
    }
    return false;
}

// Helper to convert any path to an absolute "\\?\" long path
static wchar_t* CreatePrefixedPath(const wchar_t *src)
{
    if (!src || *src == L'\0') return NULL;

    if (wcsncmp(src, L"\\\\?\\", 4) == 0)
    {
        size_t len = wcslen(src);
        wchar_t *dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, src);
        return dup;
    }

    // P0 FIX: Dynamically allocate path buffer to prevent Stack Overflow in recursive calls
    DWORD requiredLen = GetFullPathNameW(src, 0, NULL, NULL);
    if (requiredLen == 0)
    {
        size_t len = wcslen(src);
        wchar_t *dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, src);
        return dup;
    }

    wchar_t *fullPath = (wchar_t*)malloc((requiredLen + 1) * sizeof(wchar_t));
    if (!fullPath) return NULL;

    DWORD fullLen = GetFullPathNameW(src, requiredLen + 1, fullPath, NULL);
    if (fullLen == 0)
    {
        free(fullPath);
        return NULL;
    }

    if (wcsncmp(fullPath, L"\\\\?\\", 4) == 0)
    {
        return fullPath;
    }

    wchar_t *prefixed = (wchar_t*)malloc((fullLen + 16) * sizeof(wchar_t));
    if (!prefixed)
    {
        free(fullPath);
        return NULL;
    }

    if (fullPath[0] == L'\\' && fullPath[1] == L'\\')
    {
        swprintf(prefixed, fullLen + 16, L"\\\\?\\UNC\\%ls", fullPath + 2);
    }
    else
    {
        swprintf(prefixed, fullLen + 16, L"\\\\?\\%ls", fullPath);
    }
    free(fullPath);
    return prefixed;
}

// Fast thread-safe Random generator
static uint32_t FastRand(void)
{
    static _Thread_local uint64_t s_rng_state = 0;
    if (s_rng_state == 0)
    {
        LARGE_INTEGER qpc;
        QueryPerformanceCounter(&qpc);
        s_rng_state = ((uint64_t)GetCurrentThreadId() << 32) ^ (uint64_t)qpc.QuadPart ^ (uint64_t)time(NULL);
        if (s_rng_state == 0) s_rng_state = 0x853c49e6748fea9bULL;
    }
    s_rng_state ^= s_rng_state >> 12;
    s_rng_state ^= s_rng_state << 25;
    s_rng_state ^= s_rng_state >> 27;
    return (uint32_t)((s_rng_state * 0x2545F4914F6CDD1DULL) >> 32);
}

// Standard IEEE 802.3 CRC32 Calculation
static uint32_t CalculateCRC32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
            else crc >>= 1;
        }
    }
    return ~crc;
}

// Query physical cluster (LCN) of a file; returns -1 on failure
int64_t GetPhysicalClusterLCN(const wchar_t *file_path)
{
    if (!file_path) return -1;
    wchar_t *prefixed = CreatePrefixedPath(file_path);
    if (!prefixed) return -1;

    HANDLE h = CreateFileW(prefixed,
                           FILE_READ_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           NULL);
    free(prefixed);

    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }

    int64_t lcn = -1;
    STARTING_VCN_INPUT_BUFFER startingVcn;
    startingVcn.StartingVcn.QuadPart = 0;

    BYTE outBuf[4096];
    DWORD bytesReturned = 0;

    BOOL ok = DeviceIoControl(h,
                              FSCTL_GET_RETRIEVAL_POINTERS,
                              &startingVcn,
                              sizeof(startingVcn),
                              outBuf,
                              sizeof(outBuf),
                              &bytesReturned,
                              NULL);

    if (ok || GetLastError() == ERROR_MORE_DATA)
    {
        RETRIEVAL_POINTERS_BUFFER *retrieval = (RETRIEVAL_POINTERS_BUFFER*)outBuf;
        if (retrieval->ExtentCount > 0)
        {
            lcn = retrieval->Extents[0].Lcn.QuadPart;
        }
    }

    CloseHandle(h);
    return lcn;
}

// Build format-aware injection payload (MP4 free box, PNG tEXt chunk, or generic random noise)
static size_t BuildFormatPayload(HANDLE hFile, bool format_aware, uint8_t *outPayload, size_t maxPayloadSize)
{
    if (!format_aware || maxPayloadSize < 64)
    {
        int numBytes = 1 + (FastRand() % 4);
        for (int i = 0; i < numBytes; i++) {
            outPayload[i] = (uint8_t)(FastRand() & 0xFF);
        }
        return (size_t)numBytes;
    }

    // Probe file header (first 16 bytes)
    LARGE_INTEGER liZero = { 0 };
    LARGE_INTEGER origPos = { 0 };
    SetFilePointerEx(hFile, liZero, &origPos, FILE_CURRENT);

    SetFilePointerEx(hFile, liZero, NULL, FILE_BEGIN);
    uint8_t header[16] = { 0 };
    DWORD readBytes = 0;
    ReadFile(hFile, header, sizeof(header), &readBytes, NULL);
    SetFilePointerEx(hFile, origPos, NULL, FILE_BEGIN);

    // 1. MP4 / MOV Container detection (ftyp box at offset 4..7)
    if (readBytes >= 8 && memcmp(header + 4, "ftyp", 4) == 0)
    {
        int noiseBytes = 1 + (FastRand() % 4);
        uint32_t boxSize = 8 + (uint32_t)noiseBytes;

        outPayload[0] = (uint8_t)((boxSize >> 24) & 0xFF);
        outPayload[1] = (uint8_t)((boxSize >> 16) & 0xFF);
        outPayload[2] = (uint8_t)((boxSize >> 8) & 0xFF);
        outPayload[3] = (uint8_t)(boxSize & 0xFF);
        outPayload[4] = 'f';
        outPayload[5] = 'r';
        outPayload[6] = 'e';
        outPayload[7] = 'e';

        for (int i = 0; i < noiseBytes; i++) {
            outPayload[8 + i] = (uint8_t)(FastRand() & 0xFF);
        }
        return (size_t)boxSize;
    }

    // 2. PNG Image detection (\x89PNG\r\n\x1a\n)
    if (readBytes >= 8 && memcmp(header, "\x89PNG\r\n\x1a\n", 8) == 0)
    {
        // Construct PNG tEXt Chunk: Length(4B) + 'tEXt'(4B) + 'Comment\0' + noise + CRC32(4B)
        static const char kKeyword[] = "Comment";
        size_t kwLen = 7; // "Comment"
        int noiseBytes = 1 + (FastRand() % 4);
        uint32_t dataLen = (uint32_t)(kwLen + 1 + noiseBytes);

        // Length
        outPayload[0] = (uint8_t)((dataLen >> 24) & 0xFF);
        outPayload[1] = (uint8_t)((dataLen >> 16) & 0xFF);
        outPayload[2] = (uint8_t)((dataLen >> 8) & 0xFF);
        outPayload[3] = (uint8_t)(dataLen & 0xFF);

        // Chunk Type
        outPayload[4] = 't';
        outPayload[5] = 'E';
        outPayload[6] = 'X';
        outPayload[7] = 't';

        // Data: keyword + null + noise
        memcpy(outPayload + 8, kKeyword, kwLen);
        outPayload[8 + kwLen] = '\0';
        for (int i = 0; i < noiseBytes; i++) {
            outPayload[8 + kwLen + 1 + i] = (uint8_t)(FastRand() & 0xFF);
        }

        // Calculate CRC32 over Chunk Type + Data
        uint32_t crc = CalculateCRC32(outPayload + 4, 4 + dataLen);

        // CRC32 (Big Endian)
        size_t crcOffset = 8 + dataLen;
        outPayload[crcOffset + 0] = (uint8_t)((crc >> 24) & 0xFF);
        outPayload[crcOffset + 1] = (uint8_t)((crc >> 16) & 0xFF);
        outPayload[crcOffset + 2] = (uint8_t)((crc >> 8) & 0xFF);
        outPayload[crcOffset + 3] = (uint8_t)(crc & 0xFF);

        return 12 + dataLen;
    }

    // 3. Generic fallback noise (1~4 bytes)
    int numBytes = 1 + (FastRand() % 4);
    for (int i = 0; i < numBytes; i++) {
        outPayload[i] = (uint8_t)(FastRand() & 0xFF);
    }
    return (size_t)numBytes;
}

// Atomically append noise with signature protection, read-only handling, and timestamp freezing
bool AppendDataWithFreezeEx(const wchar_t *file_path, bool force_all, bool format_aware, bool *out_skipped)
{
    if (out_skipped) *out_skipped = false;
    if (!file_path) return false;

    // 1. Signature protection for Executable files
    if (!force_all && IsExecutableFile(file_path))
    {
        if (out_skipped) *out_skipped = true;
        return true; // Skipped for safety
    }

    wchar_t *prefixed = CreatePrefixedPath(file_path);
    if (!prefixed) return false;

    // 2. Read-Only Attribute auto-compatibility
    DWORD origAttrs = GetFileAttributesW(prefixed);
    if (origAttrs != INVALID_FILE_ATTRIBUTES && (origAttrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        free(prefixed);
        return false; // Reject directories
    }

    bool isReadOnly = (origAttrs != INVALID_FILE_ATTRIBUTES && (origAttrs & FILE_ATTRIBUTE_READONLY));
    if (isReadOnly)
    {
        SetFileAttributesW(prefixed, origAttrs & ~FILE_ATTRIBUTE_READONLY);
    }

    DWORD accessMode = GENERIC_READ | GENERIC_WRITE;
    HANDLE h = CreateFileW(prefixed,
                           accessMode,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           NULL);

    if (h == INVALID_HANDLE_VALUE)
    {
        h = CreateFileW(prefixed,
                        FILE_APPEND_DATA | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_BACKUP_SEMANTICS,
                        NULL);
    }

    if (h == INVALID_HANDLE_VALUE)
    {
        if (isReadOnly) SetFileAttributesW(prefixed, origAttrs);
        free(prefixed);
        return false;
    }

    FILETIME ftCreation, ftLastAccess, ftLastWrite;
    BOOL timesOk = GetFileTime(h, &ftCreation, &ftLastAccess, &ftLastWrite);

    // Build Payload
    uint8_t payload[128];
    size_t payloadLen = BuildFormatPayload(h, format_aware, payload, sizeof(payload));

    // Seek to end of file
    LARGE_INTEGER liDistance;
    liDistance.QuadPart = 0;
    SetFilePointerEx(h, liDistance, NULL, FILE_END);

    DWORD written = 0;
    BOOL writeOk = WriteFile(h, payload, (DWORD)payloadLen, &written, NULL);

    if (writeOk && written == (DWORD)payloadLen)
    {
        if (timesOk)
        {
            SetFileTime(h, &ftCreation, &ftLastAccess, &ftLastWrite);
        }
        CloseHandle(h);
        if (isReadOnly) SetFileAttributesW(prefixed, origAttrs);
        free(prefixed);
        return true;
    }

    CloseHandle(h);
    if (isReadOnly) SetFileAttributesW(prefixed, origAttrs);
    free(prefixed);
    return false;
}

// Basic wrapper for backward compatibility
bool AppendDataWithFreeze(const wchar_t *file_path)
{
    bool skipped = false;
    return AppendDataWithFreezeEx(file_path, true, true, &skipped);
}

// --------------------------------------------------------------------------
// Streaming Producer-Consumer Concurrent Queue & Pipeline
// --------------------------------------------------------------------------

#define QUEUE_CAPACITY 8192

typedef struct {
    wchar_t         *items[QUEUE_CAPACITY];
    size_t           head;
    size_t           tail;
    size_t           count;
    bool             producer_finished;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv_not_empty;
    CONDITION_VARIABLE cv_not_full;
} ConcurrentQueue;

static void QueueInit(ConcurrentQueue *q)
{
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->producer_finished = false;
    InitializeCriticalSection(&q->cs);
    InitializeConditionVariable(&q->cv_not_empty);
    InitializeConditionVariable(&q->cv_not_full);
}

static void QueuePush(ConcurrentQueue *q, wchar_t *path)
{
    EnterCriticalSection(&q->cs);
    while (q->count == QUEUE_CAPACITY)
    {
        SleepConditionVariableCS(&q->cv_not_full, &q->cs, INFINITE);
    }
    q->items[q->tail] = path;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    WakeConditionVariable(&q->cv_not_empty);
    LeaveCriticalSection(&q->cs);
}

static wchar_t* QueuePop(ConcurrentQueue *q)
{
    EnterCriticalSection(&q->cs);
    while (q->count == 0 && !q->producer_finished)
    {
        SleepConditionVariableCS(&q->cv_not_empty, &q->cs, INFINITE);
    }

    if (q->count == 0 && q->producer_finished)
    {
        LeaveCriticalSection(&q->cs);
        return NULL; // Queue is empty and producer is done
    }

    wchar_t *item = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;
    WakeConditionVariable(&q->cv_not_full);
    LeaveCriticalSection(&q->cs);
    return item;
}

static void QueueSetFinished(ConcurrentQueue *q)
{
    EnterCriticalSection(&q->cs);
    q->producer_finished = true;
    WakeAllConditionVariable(&q->cv_not_empty);
    LeaveCriticalSection(&q->cs);
}

static void QueueDestroy(ConcurrentQueue *q)
{
    // P1 FIX: Drain and free any remaining items in queue to prevent memory leak
    while (q->count > 0)
    {
        if (q->items[q->head]) {
            free(q->items[q->head]);
        }
        q->head = (q->head + 1) % QUEUE_CAPACITY;
        q->count--;
    }
    DeleteCriticalSection(&q->cs);
}

// Producer-Consumer Shared Context
typedef struct {
    ConcurrentQueue  queue;
    volatile LONG64  scanned_files;
    volatile LONG64  processed_files;
    volatile LONG64  ok_count;
    volatile LONG64  fail_count;
    volatile LONG64  skipped_count;
    volatile LONG64  last_reported;
    bool             force_all;
    bool             format_aware;
    LARGE_INTEGER    perf_freq;
    LARGE_INTEGER    start_time;
} PipelineContext;

static void ReportStreamingProgress(PipelineContext *ctx)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - ctx->start_time.QuadPart) / (double)ctx->perf_freq.QuadPart;

    int64_t done = ctx->processed_files;
    int64_t scanned = ctx->scanned_files;
    double rate = elapsed > 0.001 ? (double)done / elapsed : 0.0;

    printf("[2/3] 流式处理中: 已完成 %lld (扫描中: %lld)  成功 %lld  跳过 %lld  失败 %lld  速率 %.0f 个/秒\n",
           (long long)done, (long long)scanned, (long long)ctx->ok_count,
           (long long)ctx->skipped_count, (long long)ctx->fail_count, rate);
    fflush(stdout);
}

static DWORD WINAPI StreamingWorkerThreadProc(LPVOID lpParam)
{
    PipelineContext *ctx = (PipelineContext*)lpParam;

    while (1)
    {
        wchar_t *filePath = QueuePop(&ctx->queue);
        if (!filePath) break;

        bool skipped = false;
        bool ok = AppendDataWithFreezeEx(filePath, ctx->force_all, ctx->format_aware, &skipped);
        free(filePath);

        if (skipped) {
            InterlockedIncrement64(&ctx->skipped_count);
        } else if (ok) {
            InterlockedIncrement64(&ctx->ok_count);
        } else {
            InterlockedIncrement64(&ctx->fail_count);
        }

        LONG64 done = InterlockedIncrement64(&ctx->processed_files);
        LONG64 last = ctx->last_reported;
        if (done - last >= 1000)
        {
            if (InterlockedCompareExchange64(&ctx->last_reported, done, last) == last)
            {
                ReportStreamingProgress(ctx);
            }
        }
    }
    return 0;
}

// Producer recursive directory scanner
static void StreamScanDir(const wchar_t *currentDir, const wchar_t *excludePathNorm, PipelineContext *ctx)
{
    wchar_t *prefixedDir = CreatePrefixedPath(currentDir);
    if (!prefixedDir) return;

    size_t prefLen = wcslen(prefixedDir);
    wchar_t *searchPattern = (wchar_t*)malloc((prefLen + 16) * sizeof(wchar_t));
    if (!searchPattern) {
        free(prefixedDir);
        return;
    }
    swprintf(searchPattern, prefLen + 16, L"%ls\\*", prefixedDir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern, &fd);
    free(searchPattern);
    free(prefixedDir);

    if (hFind == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        size_t curDirLen = wcslen(currentDir);
        size_t fileNameLen = wcslen(fd.cFileName);
        wchar_t *itemPath = (wchar_t*)malloc((curDirLen + fileNameLen + 8) * sizeof(wchar_t));
        if (!itemPath) continue;

        swprintf(itemPath, curDirLen + fileNameLen + 8, L"%ls\\%ls", currentDir, fd.cFileName);

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            StreamScanDir(itemPath, excludePathNorm, ctx);
            free(itemPath);
        }
        else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            // Check against exclude path
            if (excludePathNorm && _wcsicmp(itemPath, excludePathNorm) == 0)
            {
                free(itemPath);
                continue; // Skip excluded file
            }

            InterlockedIncrement64(&ctx->scanned_files);
            QueuePush(&ctx->queue, itemPath);
        }
        else
        {
            free(itemPath);
        }
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
}

// Public API for Directory Scanning (Legacy / Direct)
typedef struct {
    FileItem *items;
    size_t    count;
    size_t    capacity;
} FileItemList;

static bool FileItemListInit(FileItemList *list, size_t initialCapacity)
{
    list->count = 0;
    list->capacity = initialCapacity > 0 ? initialCapacity : 1024;
    list->items = (FileItem*)malloc(list->capacity * sizeof(FileItem));
    return (list->items != NULL);
}

static bool FileItemListAdd(FileItemList *list, const wchar_t *path)
{
    if (list->count >= list->capacity)
    {
        size_t newCap = list->capacity * 2;
        FileItem *newItems = (FileItem*)realloc(list->items, newCap * sizeof(FileItem));
        if (!newItems) return false;
        list->items = newItems;
        list->capacity = newCap;
    }

    size_t len = wcslen(path);
    wchar_t *pCopy = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!pCopy) return false;
    wcscpy(pCopy, path);

    list->items[list->count].path = pCopy;
    list->items[list->count].lcn  = -1;
    list->count++;
    return true;
}

static bool ScanDirRecursive(const wchar_t *currentDir, const wchar_t *excludePathNorm, FileItemList *list)
{
    wchar_t *prefixedDir = CreatePrefixedPath(currentDir);
    if (!prefixedDir) return false;

    size_t prefLen = wcslen(prefixedDir);
    wchar_t *searchPattern = (wchar_t*)malloc((prefLen + 16) * sizeof(wchar_t));
    if (!searchPattern) {
        free(prefixedDir);
        return false;
    }
    swprintf(searchPattern, prefLen + 16, L"%ls\\*", prefixedDir);

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPattern, &fd);
    free(searchPattern);
    free(prefixedDir);

    if (hFind == INVALID_HANDLE_VALUE) {
        return true;
    }

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) {
            continue;
        }

        size_t curDirLen = wcslen(currentDir);
        size_t fileNameLen = wcslen(fd.cFileName);
        wchar_t *itemPath = (wchar_t*)malloc((curDirLen + fileNameLen + 8) * sizeof(wchar_t));
        if (!itemPath) continue;

        swprintf(itemPath, curDirLen + fileNameLen + 8, L"%ls\\%ls", currentDir, fd.cFileName);

        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
        {
            ScanDirRecursive(itemPath, excludePathNorm, list);
        }
        else if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (excludePathNorm && _wcsicmp(itemPath, excludePathNorm) == 0)
            {
                free(itemPath);
                continue;
            }
            FileItemListAdd(list, itemPath);
        }
        free(itemPath);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return true;
}

bool ScanDirectoryFiles(const wchar_t *dir_path, const wchar_t *exclude_path, FileItem **out_items, size_t *out_count)
{
    if (!dir_path || !out_items || !out_count) return false;

    FileItemList list;
    if (!FileItemListInit(&list, 2048)) return false;

    wchar_t excludeNorm[MAX_PATH * 4];
    bool hasExclude = false;
    if (exclude_path)
    {
        DWORD flen = GetFullPathNameW(exclude_path, sizeof(excludeNorm)/sizeof(wchar_t), excludeNorm, NULL);
        if (flen > 0) hasExclude = true;
    }

    bool ok = ScanDirRecursive(dir_path, hasExclude ? excludeNorm : NULL, &list);
    if (!ok)
    {
        FreeFileItems(list.items, list.count);
        *out_items = NULL;
        *out_count = 0;
        return false;
    }

    *out_items = list.items;
    *out_count = list.count;
    return true;
}

void FreeFileItems(FileItem *items, size_t count)
{
    if (!items) return;
    for (size_t i = 0; i < count; i++) {
        if (items[i].path) free(items[i].path);
    }
    free(items);
}

// --------------------------------------------------------------------------
// Full Batch Streaming Runner
// --------------------------------------------------------------------------

bool RunBatchHashChanger(const HashChangerOptions *options, HashChangerStats *stats)
{
    if (!options || !options->target_directory) return false;

    int threads = options->threads;
    if (threads < 1)
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        threads = (int)sysInfo.dwNumberOfProcessors;
        if (threads > 8) threads = 8;
        if (threads < 1) threads = 1;
    }
    // P1 FIX: Clamp threads to MAXIMUM_WAIT_OBJECTS (64)
    if (threads > MAXIMUM_WAIT_OBJECTS)
    {
        threads = MAXIMUM_WAIT_OBJECTS;
    }

    LARGE_INTEGER perfFreq, startTime, endTime;
    QueryPerformanceFrequency(&perfFreq);
    QueryPerformanceCounter(&startTime);

    PipelineContext ctx;
    memset(&ctx, 0, sizeof(PipelineContext));
    QueueInit(&ctx.queue);
    ctx.force_all    = options->force_all;
    ctx.format_aware = options->format_aware;
    ctx.perf_freq    = perfFreq;
    ctx.start_time   = startTime;

    printf("[1/3] 启动流式流水线：工作线程 %d，签名保护: %s，格式感知: %s\n",
           threads,
           options->force_all ? "已禁用(--force-all)" : "已启用(默认保护.exe/.dll)",
           options->format_aware ? "已启用(MP4/PNG结构感知)" : "通用追加");

    HANDLE *workerHandles = (HANDLE*)malloc(threads * sizeof(HANDLE));
    if (!workerHandles)
    {
        QueueDestroy(&ctx.queue);
        return false;
    }

    // Launch worker threads with robust error handling
    int activeWorkers = 0;
    for (int t = 0; t < threads; t++) {
        HANDLE hTh = CreateThread(NULL, 0, StreamingWorkerThreadProc, &ctx, 0, NULL);
        if (hTh != NULL) {
            workerHandles[activeWorkers++] = hTh;
        }
    }

    // P0 FIX: If no worker threads could be created, abort immediately to avoid deadlock
    if (activeWorkers == 0)
    {
        free(workerHandles);
        QueueDestroy(&ctx.queue);
        return false;
    }

    // Producer scans directory on current thread
    wchar_t excludeNorm[MAX_PATH * 4];
    bool hasExclude = false;
    if (options->self_exe_path)
    {
        DWORD flen = GetFullPathNameW(options->self_exe_path, sizeof(excludeNorm)/sizeof(wchar_t), excludeNorm, NULL);
        if (flen > 0) hasExclude = true;
    }

    StreamScanDir(options->target_directory, hasExclude ? excludeNorm : NULL, &ctx);

    // Producer finished, signal all workers
    QueueSetFinished(&ctx.queue);

    // Wait for active workers to drain the queue and complete
    WaitForMultipleObjects(activeWorkers, workerHandles, TRUE, INFINITE);
    for (int t = 0; t < activeWorkers; t++) {
        CloseHandle(workerHandles[t]);
    }
    free(workerHandles);
    QueueDestroy(&ctx.queue);

    QueryPerformanceCounter(&endTime);
    double elapsed = (double)(endTime.QuadPart - startTime.QuadPart) / (double)perfFreq.QuadPart;
    int64_t total = ctx.processed_files;
    double rate = elapsed > 0.0001 ? (double)total / elapsed : 0.0;

    if (stats)
    {
        stats->total_files        = total;
        stats->ok_count           = ctx.ok_count;
        stats->fail_count         = ctx.fail_count;
        stats->skipped_count      = ctx.skipped_count;
        stats->elapsed_seconds    = elapsed;
        stats->rate_files_per_sec = rate;
    }

    printf("\n");
    printf("[3/3] 处理完成：总计 %lld 个，成功 %lld 个，跳过(签名保护) %lld 个，失败 %lld 个，耗时 %.2f 秒，速率 %.0f 个/秒\n",
           (long long)total, (long long)ctx.ok_count, (long long)ctx.skipped_count, (long long)ctx.fail_count, elapsed, rate);

    return true;
}
