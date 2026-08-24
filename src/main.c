#include "hash_changer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <shellapi.h>

// Helper to convert and print wide path as UTF-8
static void PrintWideAsUtf8(const wchar_t *wstr)
{
    if (!wstr) return;
    char utf8Buf[4096];
    int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8Buf, (int)sizeof(utf8Buf), NULL, NULL);
    if (len > 0) {
        fputs(utf8Buf, stdout);
        fflush(stdout);
    }
}

// Helper to trim quotes and whitespace from wide string
static wchar_t* TrimQuotesAndSpaces(wchar_t *str)
{
    if (!str) return NULL;
    while (*str && (*str == L' ' || *str == L'\t' || *str == L'\r' || *str == L'\n' ||
                    *str == L'"' || *str == L'\''))
    {
        str++;
    }

    if (*str == L'\0') return str;

    size_t len = wcslen(str);
    while (len > 0)
    {
        wchar_t ch = str[len - 1];
        if (ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\n' || ch == L'"' || ch == L'\'')
        {
            str[len - 1] = L'\0';
            len--;
        }
        else
        {
            break;
        }
    }
    return str;
}

// Check if directory exists
static bool DirectoryExists(const wchar_t *path)
{
    if (!path || *path == L'\0') return false;
    DWORD attrs = GetFileAttributesW(path);
    if (attrs == INVALID_FILE_ATTRIBUTES) return false;
    return (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

// Wait for user to press Enter in interactive mode
static void WaitForEnter(void)
{
    printf("\n按回车键退出程序...");
    fflush(stdout);
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn != INVALID_HANDLE_VALUE) {
        WCHAR buf[256];
        DWORD readCount = 0;
        ReadConsoleW(hIn, buf, 256, &readCount, NULL);
    }
}

int wmain(int argc, wchar_t *argv[])
{
    ConsoleInit();

    wchar_t targetDir[MAX_PATH * 4] = { 0 };
    int threads = 0;
    bool force_all = false;
    bool format_aware = true;
    bool interactive = false;
    bool pause_on_exit = true; // Default pause on exit to prevent console window from closing

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (_wcsicmp(argv[i], L"-h") == 0 || _wcsicmp(argv[i], L"--help") == 0 || _wcsicmp(argv[i], L"/?") == 0)
        {
            SetColorCyan();
            printf("===================================================================\n");
            printf("  批量文件哈希值修改工具 v13.0 (C语言高性能流式独立版)\n");
            printf("===================================================================\n");
            ResetColor();
            printf("用法:\n");
            printf("  命令行模式:    BatchHashChanger.exe <目标文件夹> [选项]\n");
            printf("  图标拖拽模式:  直接将文件夹拖拽到本程序 .exe 图标上释放\n");
            printf("  交互模式:      直接双击运行程序，按提示输入或拖入文件夹路径\n\n");
            printf("选项:\n");
            printf("  -Threads, -t   指定并行工作线程数 (默认自动根据 CPU 核心数分配，上限 8)\n");
            printf("  --force-all, -f 强制修改所有文件 (默认跳过 .exe / .dll 等带数字签名的二进制)\n");
            printf("  --raw-noise    禁用 MP4/PNG 格式感知注入，统一使用尾部随机噪点\n");
            printf("  --no-pause     运行完毕后不暂停等待回车 (适用于自动化调用)\n");
            printf("  -h, --help     显示此帮助信息\n");
            return 0;
        }
        else if (_wcsicmp(argv[i], L"-Threads") == 0 || _wcsicmp(argv[i], L"-t") == 0 || _wcsicmp(argv[i], L"/Threads") == 0)
        {
            if (i + 1 < argc) {
                threads = _wtoi(argv[++i]);
            }
        }
        else if (_wcsicmp(argv[i], L"--force-all") == 0 || _wcsicmp(argv[i], L"-f") == 0 || _wcsicmp(argv[i], L"/force-all") == 0)
        {
            force_all = true;
        }
        else if (_wcsicmp(argv[i], L"--raw-noise") == 0)
        {
            format_aware = false;
        }
        else if (_wcsicmp(argv[i], L"--no-pause") == 0)
        {
            pause_on_exit = false;
        }
        else if (targetDir[0] == L'\0')
        {
            wcsncpy(targetDir, argv[i], sizeof(targetDir) / sizeof(wchar_t) - 1);
        }
        else if (threads == 0 && argv[i][0] >= L'0' && argv[i][0] <= L'9')
        {
            threads = _wtoi(argv[i]);
        }
    }

    wchar_t *cleanedDir = TrimQuotesAndSpaces(targetDir);

    // Interactive mode if no target directory provided
    if (!cleanedDir || cleanedDir[0] == L'\0')
    {
        interactive = true;
        pause_on_exit = true;
        system("cls");

        SetColorCyan();
        printf("===================================================================\n");
        printf("  批量文件哈希值修改工具 v13.0（C语言流式高性能版）\n");
        printf("===================================================================\n");
        ResetColor();

        printf("请输入要处理的文件夹路径 (例如 D:\\TestFolder): ");
        fflush(stdout);

        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn != INVALID_HANDLE_VALUE)
        {
            DWORD charsRead = 0;
            DWORD fileType = GetFileType(hIn);
            if ((fileType & FILE_TYPE_CHAR) && ReadConsoleW(hIn, targetDir, (sizeof(targetDir) / sizeof(wchar_t)) - 1, &charsRead, NULL))
            {
                if (charsRead > 0) {
                    targetDir[charsRead] = L'\0';
                    cleanedDir = TrimQuotesAndSpaces(targetDir);
                }
            }
            else
            {
                // Fallback for piped input or redirected stdin (CI / Scripts / Automation)
                if (fgetws(targetDir, (int)(sizeof(targetDir) / sizeof(wchar_t)) - 1, stdin))
                {
                    cleanedDir = TrimQuotesAndSpaces(targetDir);
                }
            }
        }
    }

    if (!cleanedDir || cleanedDir[0] == L'\0' || !DirectoryExists(cleanedDir))
    {
        SetColorRed();
        printf("路径不存在：");
        PrintWideAsUtf8(cleanedDir ? cleanedDir : L"");
        printf("\n");
        ResetColor();
        if (interactive || pause_on_exit) {
            WaitForEnter();
        }
        return 1;
    }

    // Get normalized full path of target directory
    wchar_t fullTargetDir[MAX_PATH * 4];
    DWORD len = GetFullPathNameW(cleanedDir, sizeof(fullTargetDir) / sizeof(wchar_t), fullTargetDir, NULL);
    if (len == 0) {
        wcsncpy(fullTargetDir, cleanedDir, sizeof(fullTargetDir) / sizeof(wchar_t) - 1);
    }

    // Get own executable path to exclude from modification
    wchar_t selfExePath[MAX_PATH * 4] = { 0 };
    GetModuleFileNameW(NULL, selfExePath, sizeof(selfExePath) / sizeof(wchar_t));

    // Detect Administrator Privileges
    if (!IsRunningAsAdmin())
    {
        SetColorYellow();
        printf("提示：当前为普通用户权限（若需处理受限系统目录建议以管理员身份运行）。\n");
        ResetColor();
    }

    SetColorCyan();
    printf("开始处理目录：");
    PrintWideAsUtf8(fullTargetDir);
    printf("，线程数：%d%s\n", threads, force_all ? " (已启用--force-all)" : "");
    ResetColor();

    HashChangerOptions options;
    memset(&options, 0, sizeof(options));
    options.target_directory = fullTargetDir;
    options.self_exe_path    = selfExePath;
    options.threads          = threads;
    options.batch_size       = 5000;
    options.force_all        = force_all;
    options.format_aware     = format_aware;
    options.interactive      = interactive;
    options.pause_on_exit    = pause_on_exit;

    HashChangerStats stats;
    bool success = RunBatchHashChanger(&options, &stats);

    if (success)
    {
        SetColorGreen();
        printf("处理完毕！\n");
        ResetColor();
        if (stats.skipped_count > 0)
        {
            SetColorYellow();
            printf("提示：已安全跳过 %lld 个可执行/驱动文件，若需强制修改可附带 --force-all 参数。\n", (long long)stats.skipped_count);
            ResetColor();
        }
        SetColorYellow();
        printf("警告：修改哈希会破坏文件完整性校验，请谨慎使用。\n");
        ResetColor();
    }
    else
    {
        SetColorRed();
        printf("处理过程中遇到错误。\n");
        ResetColor();
    }

    if (pause_on_exit)
    {
        WaitForEnter();
    }

    return success ? 0 : 1;
}

// Fallback main for standard C runtime
int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    int wargc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) return 1;
    int ret = wmain(wargc, wargv);
    LocalFree(wargv);
    return ret;
}
