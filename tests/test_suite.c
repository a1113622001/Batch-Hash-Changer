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
static bool CreateTestFile(const wchar_t *path, const void *content, size_t len)
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

// Test 1: Single file hash modification & prefix non-destructive
static bool TestSingleFileHashModification(void)
{
    const wchar_t *testFile = L"test_single_hash.dat";
    const char initialData[] = "Antigravity Batch Hash Modifier Test Data 1234567890";
    size_t initialLen = strlen(initialData);

    TEST_ASSERT(CreateTestFile(testFile, initialData, initialLen), "Create test file failed");

    char md5Before[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5Before, sizeof(md5Before)), "Calculate MD5 before failed");

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testFile, true, false, &skipped), "AppendDataWithFreezeEx failed");
    TEST_ASSERT(!skipped, "Regular dat file should not be skipped");

    char md5After[64] = { 0 };
    TEST_ASSERT(CalculateFileMD5(testFile, md5After, sizeof(md5After)), "Calculate MD5 after failed");
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MD5 must change after appending random noise");

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

    Sleep(100);

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testFile, true, false, &skipped), "AppendDataWithFreezeEx failed");

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

    DeleteFileW(testFile);
    return true;
}

// Test 3: Read-Only Attribute auto-compatibility
static bool TestReadOnlyAttributeCompatibility(void)
{
    const wchar_t *testFile = L"test_readonly.dat";
    const char data[] = "Read-Only file test content";
    TEST_ASSERT(CreateTestFile(testFile, data, strlen(data)), "Create readonly test file failed");

    // Set file as READ-ONLY
    SetFileAttributesW(testFile, FILE_ATTRIBUTE_READONLY);
    DWORD attrsBefore = GetFileAttributesW(testFile);
    TEST_ASSERT((attrsBefore & FILE_ATTRIBUTE_READONLY) != 0, "File should have READONLY attribute");

    char md5Before[64] = { 0 };
    CalculateFileMD5(testFile, md5Before, sizeof(md5Before));

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testFile, true, false, &skipped), "AppendDataWithFreezeEx on readonly file failed");
    TEST_ASSERT(!skipped, "Should not skip readonly file");

    char md5After[64] = { 0 };
    CalculateFileMD5(testFile, md5After, sizeof(md5After));
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MD5 must change for readonly file");

    // Assert READONLY attribute is restored
    DWORD attrsAfter = GetFileAttributesW(testFile);
    TEST_ASSERT((attrsAfter & FILE_ATTRIBUTE_READONLY) != 0, "READONLY attribute must be restored after modification");

    SetFileAttributesW(testFile, FILE_ATTRIBUTE_NORMAL);
    DeleteFileW(testFile);
    return true;
}

// Test 4: Executable protection (.exe/.dll default skip vs force-all)
static bool TestExecutableProtection(void)
{
    const wchar_t *testExe = L"test_dummy_app.exe";
    const wchar_t *testDll = L"test_dummy_lib.dll";
    const char data[] = "MZ Dummy PE Header";

    CreateTestFile(testExe, data, strlen(data));
    CreateTestFile(testDll, data, strlen(data));

    char exeMd5Before[64], dllMd5Before[64];
    CalculateFileMD5(testExe, exeMd5Before, sizeof(exeMd5Before));
    CalculateFileMD5(testDll, dllMd5Before, sizeof(dllMd5Before));

    // 1. Without force_all (Default): Should be SKIPPED
    bool skipped = false;
    bool res = AppendDataWithFreezeEx(testExe, false, false, &skipped);
    TEST_ASSERT(res && skipped, ".exe must be skipped by default");

    res = AppendDataWithFreezeEx(testDll, false, false, &skipped);
    TEST_ASSERT(res && skipped, ".dll must be skipped by default");

    char exeMd5After1[64], dllMd5After1[64];
    CalculateFileMD5(testExe, exeMd5After1, sizeof(exeMd5After1));
    CalculateFileMD5(testDll, dllMd5After1, sizeof(dllMd5After1));
    TEST_ASSERT(strcmp(exeMd5Before, exeMd5After1) == 0, ".exe MD5 must not change when skipped");
    TEST_ASSERT(strcmp(dllMd5Before, dllMd5After1) == 0, ".dll MD5 must not change when skipped");

    // 2. With force_all = true: Should be MODIFIED
    res = AppendDataWithFreezeEx(testExe, true, false, &skipped);
    TEST_ASSERT(res && !skipped, ".exe should be modified when force_all=true");

    char exeMd5After2[64];
    CalculateFileMD5(testExe, exeMd5After2, sizeof(exeMd5After2));
    TEST_ASSERT(strcmp(exeMd5Before, exeMd5After2) != 0, ".exe MD5 must change when force_all=true");

    DeleteFileW(testExe);
    DeleteFileW(testDll);
    return true;
}

// Test 5: Format-aware MP4 free box injection
static bool TestMp4FormatAwareInjection(void)
{
    const wchar_t *testMp4 = L"test_sample.mp4";
    // Mock MP4 header (4 bytes size + 'ftyp' + brand)
    uint8_t mp4Data[32] = {
        0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm', 0x00, 0x00, 0x02, 0x00,
        0x00, 0x00, 0x00, 0x08, 'm', 'd', 'a', 't', 'D', 'A', 'T', 'A', '1', '2', '3', '4'
    };
    TEST_ASSERT(CreateTestFile(testMp4, mp4Data, sizeof(mp4Data)), "Create mock MP4 file failed");

    char md5Before[64];
    CalculateFileMD5(testMp4, md5Before, sizeof(md5Before));

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testMp4, false, true, &skipped), "Format-aware MP4 modify failed");

    char md5After[64];
    CalculateFileMD5(testMp4, md5After, sizeof(md5After));
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "MP4 MD5 must change");

    // Inspect tail: should have 'free' box (size 9~12 bytes, 'free' tag)
    FILE *f = _wfopen(testMp4, L"rb");
    TEST_ASSERT(f != NULL, "Open MP4 failed");
    fseek(f, 0, SEEK_END);
    long totalSize = ftell(f);
    TEST_ASSERT(totalSize > (long)sizeof(mp4Data), "File size must increase");

    // Read injected box header at original EOF
    fseek(f, sizeof(mp4Data), SEEK_SET);
    uint8_t boxHeader[8];
    fread(boxHeader, 1, 8, f);
    fclose(f);

    uint32_t boxSize = (boxHeader[0] << 24) | (boxHeader[1] << 16) | (boxHeader[2] << 8) | boxHeader[3];
    TEST_ASSERT(boxSize >= 9 && boxSize <= 12, "MP4 free box size must be 9..12 bytes");
    TEST_ASSERT(memcmp(boxHeader + 4, "free", 4) == 0, "Injected box type must be 'free'");

    DeleteFileW(testMp4);
    return true;
}

// Test 6: Format-aware PNG tEXt chunk injection
static bool TestPngFormatAwareInjection(void)
{
    const wchar_t *testPng = L"test_sample.png";
    // Mock PNG header
    uint8_t pngData[16] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A,
        0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R'
    };
    TEST_ASSERT(CreateTestFile(testPng, pngData, sizeof(pngData)), "Create mock PNG file failed");

    char md5Before[64];
    CalculateFileMD5(testPng, md5Before, sizeof(md5Before));

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testPng, false, true, &skipped), "Format-aware PNG modify failed");

    char md5After[64];
    CalculateFileMD5(testPng, md5After, sizeof(md5After));
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "PNG MD5 must change");

    FILE *f = _wfopen(testPng, L"rb");
    TEST_ASSERT(f != NULL, "Open PNG failed");
    fseek(f, sizeof(pngData), SEEK_SET);
    uint8_t chunkHeader[8];
    fread(chunkHeader, 1, 8, f);
    fclose(f);

    TEST_ASSERT(memcmp(chunkHeader + 4, "tEXt", 4) == 0, "Injected chunk type must be 'tEXt'");

    DeleteFileW(testPng);
    return true;
}

// Test 7: Producer-Consumer streaming pipeline end-to-end
static bool TestStreamingPipelineEndToEnd(void)
{
    const wchar_t *testDir = L"test_stream_e2e";
    CreateDirTree(L"test_stream_e2e\\vids");
    CreateDirTree(L"test_stream_e2e\\pics");
    CreateDirTree(L"test_stream_e2e\\binaries");

    // Create 30 normal files, 10 mp4 files, 10 png files, and 10 exe/dll files
    for (int i = 0; i < 30; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\doc_%d.txt", i);
        CreateTestFile(p, "Text Content", 12);
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\vids\\clip_%d.mp4", i);
        uint8_t mp4[16] = { 0, 0, 0, 8, 'f', 't', 'y', 'p', 'm', 'p', '4', '2', 0, 0, 0, 0 };
        CreateTestFile(p, mp4, sizeof(mp4));
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\pics\\img_%d.png", i);
        uint8_t png[16] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 0 };
        CreateTestFile(p, png, sizeof(png));
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\binaries\\tool_%d.exe", i);
        CreateTestFile(p, "MZ Executable", 13);
    }

    // Run Streaming Batch Hash Changer (Default: protects .exe, format_aware enabled)
    HashChangerOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.target_directory = (wchar_t*)testDir;
    opts.threads = 4;
    opts.force_all = false;
    opts.format_aware = true;
    opts.interactive = false;

    HashChangerStats stats;
    TEST_ASSERT(RunBatchHashChanger(&opts, &stats), "RunBatchHashChanger failed");

    TEST_ASSERT(stats.total_files == 60, "Total files should be 60");
    TEST_ASSERT(stats.ok_count == 50, "50 non-executable files should succeed");
    TEST_ASSERT(stats.skipped_count == 10, "10 .exe files should be safely skipped");
    TEST_ASSERT(stats.fail_count == 0, "Fail count should be 0");

    // Clean up
    for (int i = 0; i < 30; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\doc_%d.txt", i);
        DeleteFileW(p);
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\vids\\clip_%d.mp4", i);
        DeleteFileW(p);
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\pics\\img_%d.png", i);
        DeleteFileW(p);
    }
    for (int i = 0; i < 10; i++) {
        wchar_t p[MAX_PATH];
        swprintf(p, MAX_PATH, L"test_stream_e2e\\binaries\\tool_%d.exe", i);
        DeleteFileW(p);
    }
    RemoveDirectoryW(L"test_stream_e2e\\vids");
    RemoveDirectoryW(L"test_stream_e2e\\pics");
    RemoveDirectoryW(L"test_stream_e2e\\binaries");
    RemoveDirectoryW(L"test_stream_e2e");
    return true;
}

// Test 8: Empty (0-byte) file modification test
static bool TestEmptyFileModification(void)
{
    const wchar_t *testEmpty = L"test_empty_file.dat";
    TEST_ASSERT(CreateTestFile(testEmpty, NULL, 0), "Create 0-byte file failed");

    char md5Before[64];
    TEST_ASSERT(CalculateFileMD5(testEmpty, md5Before, sizeof(md5Before)), "Calculate MD5 of empty file failed");

    bool skipped = false;
    TEST_ASSERT(AppendDataWithFreezeEx(testEmpty, true, true, &skipped), "Append to empty file failed");
    TEST_ASSERT(!skipped, "Empty file should not be skipped");

    char md5After[64];
    TEST_ASSERT(CalculateFileMD5(testEmpty, md5After, sizeof(md5After)), "Calculate MD5 after append failed");
    TEST_ASSERT(strcmp(md5Before, md5After) != 0, "Empty file MD5 must change after noise injection");

    DeleteFileW(testEmpty);
    return true;
}

// Test 9: Deep nested directory recursion stack safety test (P0 BUG-02 regression)
static bool TestDeepDirectoryStackSafety(void)
{
    wchar_t deepPath[MAX_PATH * 4] = L"test_deep_dir";
    CreateDirectoryW(deepPath, NULL);

    wchar_t current[MAX_PATH * 4];
    wcscpy(current, deepPath);

    // Create 20 levels of nested subdirectories
    for (int lvl = 0; lvl < 20; lvl++)
    {
        swprintf(current + wcslen(current), MAX_PATH * 4 - wcslen(current), L"\\lvl_%d", lvl);
        CreateDirectoryW(current, NULL);
    }

    wchar_t deepFile[MAX_PATH * 4];
    swprintf(deepFile, sizeof(deepFile)/sizeof(wchar_t), L"%ls\\deep_leaf.txt", current);
    CreateTestFile(deepFile, "Deep Payload", 12);

    HashChangerOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.target_directory = deepPath;
    opts.threads = 2;
    opts.force_all = true;
    opts.format_aware = false;

    HashChangerStats stats;
    TEST_ASSERT(RunBatchHashChanger(&opts, &stats), "Deep recursive scan must succeed without Stack Overflow");
    TEST_ASSERT(stats.ok_count == 1, "Deep nested file should be processed");

    // Clean up
    DeleteFileW(deepFile);
    for (int lvl = 19; lvl >= 0; lvl--)
    {
        RemoveDirectoryW(current);
        wchar_t *lastSlash = wcsrchr(current, L'\\');
        if (lastSlash) *lastSlash = L'\0';
    }
    RemoveDirectoryW(deepPath);
    return true;
}

// Test 10: Directory path rejection in AppendDataWithFreezeEx
static bool TestDirectoryPathRejection(void)
{
    const wchar_t *testDir = L"test_dir_reject";
    CreateDirectoryW(testDir, NULL);

    bool skipped = false;
    bool res = AppendDataWithFreezeEx(testDir, true, true, &skipped);
    TEST_ASSERT(!res, "AppendDataWithFreezeEx must reject directory path");

    RemoveDirectoryW(testDir);
    return true;
}

int main(void)
{
    ConsoleInit();
    printf("===================================================================\n");
    printf("   Batch-Hash-Changer v13.0 C语言全量自动化测试套件 (TDD)   \n");
    printf("===================================================================\n\n");

    RUN_TEST(TestSingleFileHashModification,     "1. 单文件哈希微修改与前缀无损测试");
    RUN_TEST(TestTimestampPreservation,           "2. FILETIME 原生高精度时间戳冻结测试");
    RUN_TEST(TestReadOnlyAttributeCompatibility,  "3. 只读文件属性自动兼容与恢复测试");
    RUN_TEST(TestExecutableProtection,            "4. 可执行文件签名保护(默认跳过与--force-all)测试");
    RUN_TEST(TestMp4FormatAwareInjection,         "5. MP4 容器 ISO-BMFF free Box 格式感知注入测试");
    RUN_TEST(TestPngFormatAwareInjection,         "6. PNG 图像 tEXt 辅助 Chunk 格式感知注入测试");
    RUN_TEST(TestStreamingPipelineEndToEnd,       "7. 生产者-消费者流式流水线端到端测试");
    RUN_TEST(TestEmptyFileModification,           "8. 0 字节空文件哈希修改与注入测试");
    RUN_TEST(TestDeepDirectoryStackSafety,        "9. 20 层深层嵌套目录递归栈安全测试 (防栈溢出)");
    RUN_TEST(TestDirectoryPathRejection,          "10. 目录路径非法操作前置拦截测试");

    printf("===================================================================\n");
    printf("测试结果: 通过 %d 个, 失败 %d 个\n", g_tests_passed, g_tests_failed);
    printf("===================================================================\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
