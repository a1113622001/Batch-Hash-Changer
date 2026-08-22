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

// Helper to convert any path to an absolute "\\?\" long path
static wchar_t* CreatePrefixedPath(const wchar_t *src)
{
    if (!src || *src == L'\0') return NULL;

    // Already prefixed
    if (wcsncmp(src, L"\\\\?\\", 4) == 0)
    {
        size_t len = wcslen(src);
        wchar_t *dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, src);
        return dup;
    }

    wchar_t fullPath[32768];
    DWORD fullLen = GetFullPathNameW(src, 32768, fullPath, NULL);
    if (fullLen == 0 || fullLen >= 32768)
    {
        size_t len = wcslen(src);
        wchar_t *dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, src);
        return dup;
    }

    if (wcsncmp(fullPath, L"\\\\?\\", 4) == 0)
    {
        wchar_t *dup = (wchar_t*)malloc((fullLen + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, fullPath);
        return dup;
    }

    wchar_t *prefixed = (wchar_t*)malloc((fullLen + 16) * sizeof(wchar_t));
    if (!prefixed) return NULL;

    if (fullPath[0] == L'\\' && fullPath[1] == L'\\')
    {
        // UNC path \\server\share -> \\?\UNC\server\share
        swprintf(prefixed, fullLen + 16, L"\\\\?\\UNC\\%ls", fullPath + 2);
    }
    else
    {
        swprintf(prefixed, fullLen + 16, L"\\\\?\\%ls", fullPath);
    }
    return prefixed;
}

// Fast thread-safe Random generator using thread ID + QPC seed
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

// Atomically append 1~4 random bytes and freeze/restore FILETIME timestamps
bool AppendDataWithFreeze(const wchar_t *file_path)
{
    if (!file_path) return false;
    wchar_t *prefixed = CreatePrefixedPath(file_path);
    if (!prefixed) return false;

    DWORD accessMode = GENERIC_READ | GENERIC_WRITE;
    HANDLE h = CreateFileW(prefixed,
                           accessMode,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_BACKUP_SEMANTICS,
                           NULL);

    // Fallback if full access denied: try write & attributes only
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
        free(prefixed);
        return false;
    }

    FILETIME ftCreation, ftLastAccess, ftLastWrite;
    BOOL timesOk = GetFileTime(h, &ftCreation, &ftLastAccess, &ftLastWrite);

    // Generate 1 to 4 random bytes
    int numBytes = 1 + (FastRand() % 4);
    BYTE randomData[4];
    for (int i = 0; i < numBytes; i++) {
        randomData[i] = (BYTE)(FastRand() & 0xFF);
    }

    // Seek to end of file
    LARGE_INTEGER liDistance;
    liDistance.QuadPart = 0;
    SetFilePointerEx(h, liDistance, NULL, FILE_END);

    DWORD written = 0;
    BOOL writeOk = WriteFile(h, randomData, (DWORD)numBytes, &written, NULL);

    if (writeOk && written == (DWORD)numBytes)
    {
        if (timesOk)
        {
            SetFileTime(h, &ftCreation, &ftLastAccess, &ftLastWrite);
        }
        CloseHandle(h);
        free(prefixed);
        return true;
    }

    CloseHandle(h);
    free(prefixed);
    return false;
}

// Dynamic array of FileItem
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

// Normalize path for comparison
static wchar_t* NormalizePathAlloc(const wchar_t *path)
{
    if (!path) return NULL;
    wchar_t fullPath[32768];
    DWORD len = GetFullPathNameW(path, 32768, fullPath, NULL);
    if (len == 0 || len >= 32768)
    {
        size_t slen = wcslen(path);
        wchar_t *dup = (wchar_t*)malloc((slen + 1) * sizeof(wchar_t));
        if (dup) wcscpy(dup, path);
        return dup;
    }
    wchar_t *dup = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (dup) wcscpy(dup, fullPath);
    return dup;
}

// Recursive directory scanner
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
        return true; // Directory empty or inaccessible, continue
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

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            ScanDirRecursive(itemPath, excludePathNorm, list);
        }
        else
        {
            // Check against exclude path
            if (excludePathNorm)
            {
                wchar_t *itemNorm = NormalizePathAlloc(itemPath);
                if (itemNorm)
                {
                    if (_wcsicmp(itemNorm, excludePathNorm) == 0)
                    {
                        free(itemNorm);
                        free(itemPath);
                        continue; // Skip excluded file
                    }
                    free(itemNorm);
                }
            }
            FileItemListAdd(list, itemPath);
        }
        free(itemPath);
    } while (FindNextFileW(hFind, &fd));

    FindClose(hFind);
    return true;
}

// Public directory scanning API
bool ScanDirectoryFiles(const wchar_t *dir_path, const wchar_t *exclude_path, FileItem **out_items, size_t *out_count)
{
    if (!dir_path || !out_items || !out_count) return false;

    FileItemList list;
    if (!FileItemListInit(&list, 2048)) return false;

    wchar_t *excludeNorm = exclude_path ? NormalizePathAlloc(exclude_path) : NULL;
    bool ok = ScanDirRecursive(dir_path, excludeNorm, &list);
    if (excludeNorm) free(excludeNorm);

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

// Comparator for LCN elevator sorting (Ascending order; items with LCN < 0 sorted last)
static int CompareFileItemsByLcn(const void *a, const void *b)
{
    const FileItem *itemA = (const FileItem*)a;
    const FileItem *itemB = (const FileItem*)b;

    int64_t lcnA = (itemA->lcn < 0) ? INT64_MAX : itemA->lcn;
    int64_t lcnB = (itemB->lcn < 0) ? INT64_MAX : itemB->lcn;

    if (lcnA < lcnB) return -1;
    if (lcnA > lcnB) return 1;
    return 0;
}

// Worker context for parallel processing
typedef struct {
    FileItem         *batch_items;
    size_t            batch_count;
    volatile LONG     queue_index;
    volatile LONG64   ok_count;
    volatile LONG64   fail_count;
    volatile LONG64   touched_count;
    volatile LONG64   last_reported;
    int64_t           total_files;
    LARGE_INTEGER     perf_freq;
    LARGE_INTEGER     start_time;
    bool              query_lcn_phase;
} BatchContext;

static void ReportProgress(BatchContext *ctx)
{
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double elapsed = (double)(now.QuadPart - ctx->start_time.QuadPart) / (double)ctx->perf_freq.QuadPart;

    int64_t done = ctx->touched_count;
    int64_t total = ctx->total_files;
    double pct  = total > 0 ? (double)done * 100.0 / (double)total : 0.0;
    double rate = elapsed > 0.001 ? (double)done / elapsed : 0.0;
    double eta  = rate > 0.001 ? (double)(total - done) / rate : 0.0;

    printf("[2/3] 进度 %lld/%lld (%.1f%%)  成功 %lld  失败 %lld  速率 %.0f 个/秒  预计剩余 %.0fs\n",
           (long long)done, (long long)total, pct, (long long)ctx->ok_count, (long long)ctx->fail_count, rate, eta);
    fflush(stdout);
}

static DWORD WINAPI WorkerThreadProc(LPVOID lpParam)
{
    BatchContext *ctx = (BatchContext*)lpParam;
    size_t batchCount = ctx->batch_count;

    if (ctx->query_lcn_phase)
    {
        while (1)
        {
            LONG idx = InterlockedIncrement(&ctx->queue_index) - 1;
            if (idx >= (LONG)batchCount) break;
            ctx->batch_items[idx].lcn = GetPhysicalClusterLCN(ctx->batch_items[idx].path);
        }
        return 0;
    }

    while (1)
    {
        LONG idx = InterlockedIncrement(&ctx->queue_index) - 1;
        if (idx >= (LONG)batchCount) break;

        bool ok = AppendDataWithFreeze(ctx->batch_items[idx].path);
        if (ok) {
            InterlockedIncrement64(&ctx->ok_count);
        } else {
            InterlockedIncrement64(&ctx->fail_count);
        }

        LONG64 done = InterlockedIncrement64(&ctx->touched_count);
        LONG64 last = ctx->last_reported;
        if (done - last >= 1000)
        {
            if (InterlockedCompareExchange64(&ctx->last_reported, done, last) == last)
            {
                ReportProgress(ctx);
            }
        }
    }
    return 0;
}

// Process a batch of files with LCN retrieval, sorting, and parallel execution
static void ProcessBatch(FileItem *batch_items, size_t batch_count, int threads,
                         BatchContext *shared_ctx, HANDLE *thread_handles)
{
    // Phase 1: Parallel LCN retrieval
    shared_ctx->batch_items     = batch_items;
    shared_ctx->batch_count     = batch_count;
    shared_ctx->queue_index     = 0;
    shared_ctx->query_lcn_phase = true;

    for (int t = 0; t < threads; t++) {
        thread_handles[t] = CreateThread(NULL, 0, WorkerThreadProc, shared_ctx, 0, NULL);
    }
    WaitForMultipleObjects(threads, thread_handles, TRUE, INFINITE);
    for (int t = 0; t < threads; t++) {
        CloseHandle(thread_handles[t]);
    }

    // Phase 2: Elevator Sort (Ascending by LCN)
    qsort(batch_items, batch_count, sizeof(FileItem), CompareFileItemsByLcn);

    // Phase 3: Parallel Append & Freeze
    shared_ctx->queue_index     = 0;
    shared_ctx->query_lcn_phase = false;

    for (int t = 0; t < threads; t++) {
        thread_handles[t] = CreateThread(NULL, 0, WorkerThreadProc, shared_ctx, 0, NULL);
    }
    WaitForMultipleObjects(threads, thread_handles, TRUE, INFINITE);
    for (int t = 0; t < threads; t++) {
        CloseHandle(thread_handles[t]);
    }
}

// Run full batch processing
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

    LARGE_INTEGER perfFreq, startTime, endTime;
    QueryPerformanceFrequency(&perfFreq);
    QueryPerformanceCounter(&startTime);

    FileItem *allItems = NULL;
    size_t totalCount = 0;

    if (!ScanDirectoryFiles(options->target_directory, options->self_exe_path, &allItems, &totalCount))
    {
        SetColorRed();
        printf("[错误] 扫描目录失败。\n");
        ResetColor();
        return false;
    }

    printf("[1/3] 扫描完成：共 %llu 个文件，并行线程 %d\n", (unsigned long long)totalCount, threads);
    if (totalCount == 0)
    {
        printf("没有可处理的文件。\n");
        FreeFileItems(allItems, totalCount);
        if (stats) {
            memset(stats, 0, sizeof(HashChangerStats));
        }
        return true;
    }

    size_t batchSize = options->batch_size > 0 ? (size_t)options->batch_size : 5000;

    BatchContext ctx;
    memset(&ctx, 0, sizeof(BatchContext));
    ctx.total_files   = (int64_t)totalCount;
    ctx.perf_freq     = perfFreq;
    ctx.start_time    = startTime;

    HANDLE *threadHandles = (HANDLE*)malloc(threads * sizeof(HANDLE));
    if (!threadHandles)
    {
        FreeFileItems(allItems, totalCount);
        return false;
    }

    for (size_t offset = 0; offset < totalCount; offset += batchSize)
    {
        size_t currentBatchCount = totalCount - offset;
        if (currentBatchCount > batchSize) {
            currentBatchCount = batchSize;
        }

        ProcessBatch(allItems + offset, currentBatchCount, threads, &ctx, threadHandles);
    }

    free(threadHandles);
    QueryPerformanceCounter(&endTime);

    double elapsed = (double)(endTime.QuadPart - startTime.QuadPart) / (double)perfFreq.QuadPart;
    double rate = elapsed > 0.0001 ? (double)totalCount / elapsed : 0.0;

    if (stats)
    {
        stats->total_files        = (int64_t)totalCount;
        stats->ok_count           = ctx.ok_count;
        stats->fail_count         = ctx.fail_count;
        stats->elapsed_seconds    = elapsed;
        stats->rate_files_per_sec = rate;
    }

    printf("\n");
    printf("[3/3] 处理完成：成功 %lld 个，失败 %lld 个，耗时 %.2f 秒，速率 %.0f 个/秒\n",
           (long long)ctx.ok_count, (long long)ctx.fail_count, elapsed, rate);

    FreeFileItems(allItems, totalCount);
    return true;
}
