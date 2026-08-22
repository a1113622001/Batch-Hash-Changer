#include "../src/hash_changer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wincrypt.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "kernel32.lib")

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        printf("  [FAIL] Line %d: %s\n", __LINE__, msg); \
        g_tests_failed++; \
        return false; \
    } \
} while(0)

#define RUN_TEST(test_func, test_name) do { \
    printf("Running [%s] ...\n", test_name); \
    if (test_func()) { \
        printf("  [PASS] %s\n\n", test_name); \
        g_tests_passed++; \
    } else { \
        printf("  [FAILED] %s\n\n", test_name); \
    } \
} while(0)

// Helper: Calculate MD5 hex string of a file
static bool CalculateFileMD5(const wchar_t *filePath, char *outHex, size_t outHexSize)
{
    if (!filePath || !outHex || outHexSize < 33) return false;

    HANDLE hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    bool success = false;

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            BYTE buffer[8192];
            DWORD bytesRead = 0;
            BOOL readOk = TRUE;

            while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
            {
                if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
                    readOk = FALSE;
                    break;
                }
            }

            if (readOk)
            {
                BYTE hashBytes[16];
                DWORD hashLen = sizeof(hashBytes);
                if (CryptGetHashParam(hHash, HP_HASHVAL, hashBytes, &hashLen, 0))
                {
                    for (DWORD i = 0; i < hashLen; i++) {
                        snprintf(outHex + (i * 2), outHexSize - (i * 2), "%02x", hashBytes[i]);
                    }
                    outHex[32] = '\0';
                    success = true;
                }
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    CloseHandle(hFile);
    return success;
}

// Helper: Create a temporary test file with specific content
static bool CreateTestFile(const wchar_t *path, const char *content, size_t len)
{
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written = 0;
    if (len > 0 && content) {
        WriteFile(hFile, content, (DWORD)len, &written, NULL);
    }
    CloseHandle(hFile);
    return true;
}

// Helper: Create nested directory tree
static bool CreateDirTree(const wchar_t *dir)
{
    wchar_t temp[MAX_PATH * 2];
    wcsncpy(temp, dir, sizeof(temp) / sizeof(wchar_t) - 1);
    for (wchar_t *p = temp + 1; *p; p++)
    {
        if (*p == L'\\' || *p == L'/')
        {
            wchar_t slash = *p;
            *p = L'\0';
            CreateDirectoryW(temp, NULL);
            *p = slash;
        }
    }
    return CreateDirectoryW(temp, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

// Test 1: Single file hash modification & byte preservation
static bool TestSingleFileHashModification(void)
{
    const wchar_t *testFile = L"test_single_hash.dat";
    const char initialData[] = "Antigravity Batch Hash Modifier Test Data 1234567890";
    size_t initialLen = strlen(initialData);

    TEST_ASSERT(CreateTestFile(testFile, initialData, initialLen), "Create test file failed");

    char md5Before[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5Before, sizeof(md5Before)), "Calculate MD5 before failed");

    // Perform AppendDataWithFreeze
    TEST_ASSERT(AppendDataWithFreeze(testFile), "AppendDataWithFreeze failed");

    char md5After[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5After, sizeof(md5After)), "Calculate MD5 after failed");

    // Check MD5 changed
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MD5 must change after appending random noise");

    // Check size changed by 1~4 bytes
    HANDLE h = CreateFileW(testFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER newSize;
    GetFileSizeEx(h, &newSize);
    CloseHandle(h);

    int64_t diff = newSize.QuadPart - (int64_t)initialLen;
    TEST_ASSERT(diff >= 1 && diff <= 4, "Appended bytes count must be in range [1, 4]");

    // Verify original content prefix is intact
    FILE *f = _wfopen(testFile, L"rb");
    TEST_ASSERT(f != NULL, "Re-open test file failed");
    char *buf = (char*)malloc(initialLen);
    fread(buf, 1, initialLen, f);
    fclose(f);
    TEST_ASSERT(memcmp(buf, initialData, initialLen) == 0, "Original file content must remain unchanged");
    free(buf);

    DeleteFileW(testFile);
    return true;
}

// Test 2: Timestamp preservation test
static bool TestTimestampPreservation(void)
{
    const wchar_t *testFile = L"test_timestamp.dat";
    const char data[] = "Testing FILETIME preservation";
    TEST_ASSERT(CreateTestFile(testFile, data, strlen(data)), "Create timestamp test file failed");

    HANDLE h = CreateFileW(testFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    TEST_ASSERT(h != INVALID_HANDLE_VALUE, "Open file failed");
    FILETIME ftCreateBefore, ftAccessBefore, ftWriteBefore;
    TEST_ASSERT(GetFileTime(h, &ftCreateBefore, &ftAccessBefore, &ftWriteBefore), "GetFileTime before failed");
    CloseHandle(h);

    // Sleep 100ms so system time moves
    Sleep(100);

    TEST_ASSERT(AppendDataWithFreeze(testFile), "AppendDataWithFreeze failed");

    h = CreateFileW(testFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    TEST_ASSERT(h != INVALID_HANDLE_VALUE, "Open file after modify failed");
    FILETIME ftCreateAfter, ftAccessAfter, ftWriteAfter;
    TEST_ASSERT(GetFileTime(h, &ftCreateAfter, &ftAccessAfter, &ftWriteAfter), "GetFileTime after failed");
    CloseHandle(h);

    TEST_ASSERT(ftCreateBefore.dwLowDateTime == ftCreateAfter.dwLowDateTime &&
                ftCreateBefore.dwHighDateTime == ftCreateAfter.dwHighDateTime,
                "CreationTime must match exactly 100%");

    TEST_ASSERT(ftWriteBefore.dwLowDateTime == ftWriteAfter.dwLowDateTime &&
                ftWriteBefore.dwHighDateTime == ftWriteAfter.dwHighDateTime,
                "LastWriteTime must match exactly 100%");

    TEST_ASSERT(ftAccessBefore.dwLowDateTime == ftAccessAfter.dwLowDateTime &&
                ftAccessBefore.dwHighDateTime == ftAccessAfter.dwHighDateTime,
                "LastAccessTime must match exactly 100%");

    DeleteFileW(testFile);
    return true;
}

// Test 3: 0-byte Empty File handling
static bool TestEmptyFile(void)
{
    const wchar_t *testFile = L"test_empty.dat";
    TEST_ASSERT(CreateTestFile(testFile, "", 0), "Create 0-byte test file failed");

    char md5Before[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5Before, sizeof(md5Before)), "MD5 empty file failed");
    TEST_ASSERT(strcmp(md5Before, "d41d8cd98f00b204e9800998ecf8427e") == 0, "Empty file MD5 mismatch");

    TEST_ASSERT(AppendDataWithFreeze(testFile), "AppendDataWithFreeze on empty file failed");

    char md5After[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5After, sizeof(md5After)), "MD5 after modify failed");
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MD5 of empty file must change");

    HANDLE h = CreateFileW(testFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    LARGE_INTEGER newSize;
    GetFileSizeEx(h, &newSize);
    CloseHandle(h);

    TEST_ASSERT(newSize.QuadPart >= 1 && newSize.QuadPart <= 4, "Empty file size must become 1~4 bytes");

    DeleteFileW(testFile);
    return true;
}

// Test 4: Unicode, Chinese characters and nested folders
static bool TestUnicodeAndChinesePaths(void)
{
    const wchar_t *testDir = L"测试_中文_目录 🚀\\子目录 A\\深层 [2026]";
    CreateDirTree(testDir);

    const wchar_t *testFile = L"测试_中文_目录 🚀\\子目录 A\\深层 [2026]\\文件 (带空格).mp4";
    const char data[] = "Unicode path test content for MP4 dummy";
    TEST_ASSERT(CreateTestFile(testFile, data, strlen(data)), "Create unicode test file failed");

    char md5Before[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5Before, sizeof(md5Before)), "MD5 before failed");

    TEST_ASSERT(AppendDataWithFreeze(testFile), "AppendDataWithFreeze failed on unicode path");

    char md5After[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5After, sizeof(md5After)), "MD5 after failed");
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MD5 must change for unicode file");

    DeleteFileW(testFile);
    RemoveDirectoryW(L"测试_中文_目录 🚀\\子目录 A\\深层 [2026]");
    RemoveDirectoryW(L"测试_中文_目录 🚀\\子目录 A");
    RemoveDirectoryW(L"测试_中文_目录 🚀");
    return true;
}

// Test 5: Recursive Scanner and Self-Exclusion
static bool TestScannerAndSelfExclusion(void)
{
    const wchar_t *testBaseDir = L"test_scan_tree";
    CreateDirTree(L"test_scan_tree\\sub1");
    CreateDirTree(L"test_scan_tree\\sub2\\nested");

    CreateTestFile(L"test_scan_tree\\file1.txt", "1", 1);
    CreateTestFile(L"test_scan_tree\\sub1\\file2.txt", "2", 1);
    CreateTestFile(L"test_scan_tree\\sub2\\nested\\file3.txt", "3", 1);
    CreateTestFile(L"test_scan_tree\\self_exclude_tool.exe", "exe", 3);

    wchar_t selfExeNorm[MAX_PATH * 2];
    GetFullPathNameW(L"test_scan_tree\\self_exclude_tool.exe", sizeof(selfExeNorm)/sizeof(wchar_t), selfExeNorm, NULL);

    FileItem *items = NULL;
    size_t count = 0;
    TEST_ASSERT(ScanDirectoryFiles(testBaseDir, selfExeNorm, &items, &count), "ScanDirectoryFiles failed");

    TEST_ASSERT(count == 3, "ScanDirectoryFiles must find exactly 3 files, excluding self exe");

    FreeFileItems(items, count);

    DeleteFileW(L"test_scan_tree\\file1.txt");
    DeleteFileW(L"test_scan_tree\\sub1\\file2.txt");
    DeleteFileW(L"test_scan_tree\\sub2\\nested\\file3.txt");
    DeleteFileW(L"test_scan_tree\\self_exclude_tool.exe");
    RemoveDirectoryW(L"test_scan_tree\\sub2\\nested");
    RemoveDirectoryW(L"test_scan_tree\\sub2");
    RemoveDirectoryW(L"test_scan_tree\\sub1");
    RemoveDirectoryW(L"test_scan_tree");
    return true;
}

// Test 6: End-to-End Multi-threaded Batch Run
static bool TestEndToEndBatchRun(void)
{
    const wchar_t *testDir = L"test_batch_e2e";
    CreateDirTree(L"test_batch_e2e\\dir1");
    CreateDirTree(L"test_batch_e2e\\dir2");

    const int totalFiles = 60;
    char md5ListBefore[60][64];

    for (int i = 0; i < totalFiles; i++)
    {
        wchar_t path[MAX_PATH];
        if (i % 2 == 0) {
            swprintf(path, MAX_PATH, L"test_batch_e2e\\dir1\\item_%03d.bin", i);
        } else {
            swprintf(path, MAX_PATH, L"test_batch_e2e\\dir2\\item_%03d.bin", i);
        }
        char content[64];
        snprintf(content, sizeof(content), "Batch test content file index %d\n", i);
        CreateTestFile(path, content, strlen(content));
        CalculateFileMD5(path, md5ListBefore[i], sizeof(md5ListBefore[i]));
    }

    HashChangerOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.target_directory = (wchar_t*)testDir;
    opts.threads = 4;
    opts.batch_size = 25;
    opts.interactive = false;

    HashChangerStats stats;
    TEST_ASSERT(RunBatchHashChanger(&opts, &stats), "RunBatchHashChanger failed");

    TEST_ASSERT(stats.total_files == totalFiles, "Total files processed mismatch");
    TEST_ASSERT(stats.ok_count == totalFiles, "All files must be processed successfully");
    TEST_ASSERT(stats.fail_count == 0, "Fail count must be 0");

    // Verify all MD5s changed
    for (int i = 0; i < totalFiles; i++)
    {
        wchar_t path[MAX_PATH];
        if (i % 2 == 0) {
            swprintf(path, MAX_PATH, L"test_batch_e2e\\dir1\\item_%03d.bin", i);
        } else {
            swprintf(path, MAX_PATH, L"test_batch_e2e\\dir2\\item_%03d.bin", i);
        }
        char md5After[64];
        CalculateFileMD5(path, md5After, sizeof(md5After));
        TEST_ASSERT(strcmp(md5ListBefore[i], md5After) != 0, "MD5 must change for batch file");
        DeleteFileW(path);
    }

    RemoveDirectoryW(L"test_batch_e2e\\dir1");
    RemoveDirectoryW(L"test_batch_e2e\\dir2");
    RemoveDirectoryW(L"test_batch_e2e");
    return true;
}

int main(void)
{
    ConsoleInit();
    printf("==================================================\n");
    printf("   Batch-Hash-Changer C语言自动化测试套件 (TDD)   \n");
    printf("==================================================\n\n");

    RUN_TEST(TestSingleFileHashModification, "1. 单文件哈希微修改与前缀无损测试");
    RUN_TEST(TestTimestampPreservation,       "2. FILETIME 原生高精度时间戳冻结测试");
    RUN_TEST(TestEmptyFile,                   "3. 0字节空文件修改测试");
    RUN_TEST(TestUnicodeAndChinesePaths,      "4. 中文/特殊符号/多级嵌套路径测试");
    RUN_TEST(TestScannerAndSelfExclusion,     "5. 递归扫描器与运行自身排除测试");
    RUN_TEST(TestEndToEndBatchRun,            "6. 多线程并发流水线端到端批量测试");

    printf("==================================================\n");
    printf("测试结果: 通过 %d 个, 失败 %d 个\n", g_tests_passed, g_tests_failed);
    printf("==================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
