# =================================================================
# HDD God-Mode 优化脚本 (PowerShell 动态调用底层 API 版)
# =================================================================

$code = @"
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace HddGodMode
{
    public class Optimizer
    {
        public static void Run(string targetDirectory)
        {
            Console.WriteLine("[1/3] 正在扫描文件...");
            var allFiles = Directory.EnumerateFiles(targetDirectory, "*.*", SearchOption.AllDirectories);
            
            Console.WriteLine("[2/3] 开始执行电梯算法批处理...");
            System.Diagnostics.Stopwatch sw = System.Diagnostics.Stopwatch.StartNew();
            
            int batchSize = 5000;
            List<string> currentBatch = new List<string>(batchSize);
            int totalProcessed = 0;

            foreach (var path in allFiles)
            {
                currentBatch.Add(path);
                if (currentBatch.Count >= batchSize)
                {
                    ProcessBatchElevator(currentBatch);
                    totalProcessed += currentBatch.Count;
                    Console.WriteLine($"已顺滑写入: {totalProcessed} 个文件...");
                    currentBatch.Clear();
                }
            }

            if (currentBatch.Count > 0)
            {
                ProcessBatchElevator(currentBatch);
                totalProcessed += currentBatch.Count;
                Console.WriteLine($"已顺滑写入: {totalProcessed} 个文件...");
            }

            sw.Stop();
            Console.WriteLine($"\n[3/3] 处理完成！总耗时: {sw.Elapsed.TotalSeconds:F2} 秒");
        }

        static void ProcessBatchElevator(List<string> batchPaths)
        {
            var fileWithLcn = batchPaths.AsParallel()
                .Select(path => new { Path = path, Lcn = GetPhysicalCluster(path) })
                .Where(x => x.Lcn != -1)
                .ToList();

            // 电梯算法：按硬盘物理簇号排序
            fileWithLcn.Sort((a, b) => a.Lcn.CompareTo(b.Lcn));

            Random rnd = new Random();
            foreach (var item in fileWithLcn)
            {
                AppendDataWithFreeze(item.Path, rnd);
            }
        }

        static long GetPhysicalCluster(string filePath)
        {
            string longPath = @"\\?\" + filePath;
            IntPtr hFile = CreateFileW(longPath, 0x80, FileShare.ReadWrite | FileShare.Delete, IntPtr.Zero, 3, 0x00000080, IntPtr.Zero);
            if (hFile == IntPtr.Zero || hFile == (IntPtr)(-1)) return -1;

            try
            {
                long startingVcn = 0;
                RETRIEVAL_POINTERS_BUFFER outBuffer = new RETRIEVAL_POINTERS_BUFFER();
                bool success = DeviceIoControl(hFile, 589939, ref startingVcn, sizeof(long), ref outBuffer, Marshal.SizeOf(typeof(RETRIEVAL_POINTERS_BUFFER)), out _, IntPtr.Zero);
                return (success && outBuffer.ExtentCount > 0) ? outBuffer.Lcn : 0;
            }
            finally { CloseHandle(hFile); }
        }

        static void AppendDataWithFreeze(string filePath, Random rnd)
        {
            string longPath = @"\\?\" + filePath;
            IntPtr hFile = CreateFileW(longPath, 0x40000000 | 0x00000100, 0, IntPtr.Zero, 3, 0x00000080, IntPtr.Zero);
            if (hFile != IntPtr.Zero && hFile != (IntPtr)(-1))
            {
                try
                {
                    // 冻结时间戳
                    FILETIME freezeTime = new FILETIME { dwLowDateTime = 0xFFFFFFFF, dwHighDateTime = 0xFFFFFFFF };
                    SetFileTime(hFile, IntPtr.Zero, ref freezeTime, ref freezeTime);

                    // 移动到末尾并写入随机字节
                    SetFilePointerEx(hFile, 0, out _, 2);
                    byte[] data = new byte[rnd.Next(1, 5)];
                    rnd.NextBytes(data);
                    WriteFile(hFile, data, (uint)data.Length, out _, IntPtr.Zero);
                }
                finally { CloseHandle(hFile); }
            }
        }

        [StructLayout(LayoutKind.Sequential)] public struct FILETIME { public uint dwLowDateTime; public uint dwHighDateTime; }
        [StructLayout(LayoutKind.Sequential)] public struct RETRIEVAL_POINTERS_BUFFER { public int ExtentCount; public int Unused; public long StartingVcn; public long NextVcn; public long Lcn; }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess, FileShare dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);
        [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr hObject);
        [DllImport("kernel32.dll")] public static extern bool WriteFile(IntPtr hFile, byte[] lpBuffer, uint nNumberOfBytesToWrite, out uint lpNumberOfBytesWritten, IntPtr lpOverlapped);
        [DllImport("kernel32.dll")] public static extern bool SetFilePointerEx(IntPtr hFile, long liDistanceToMove, out long lpNewFilePointer, uint dwMoveMethod);
        [DllImport("kernel32.dll")] public static extern bool DeviceIoControl(IntPtr hDevice, uint dwIoControlCode, ref long lpInBuffer, int nInBufferSize, ref RETRIEVAL_POINTERS_BUFFER lpOutBuffer, int nOutBufferSize, out int lpBytesReturned, IntPtr lpOverlapped);
        [DllImport("kernel32.dll")] public static extern bool SetFileTime(IntPtr hFile, IntPtr lpCreationTime, ref FILETIME lpLastAccessTime, ref FILETIME lpLastWriteTime);
    }
}
"@

# 编译 C# 代码
Add-Type -TypeDefinition $code -Language CSharp

# 交互式获取路径并运行
Clear-Host
Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "  HDD God-Mode 优化脚本 (物理簇排序 + MFT冻结)" -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

$targetDir = Read-Host "请输入要处理的文件夹路径 (例如 D:\TestFolder)"
$targetDir = $targetDir.Trim('"').Trim("'")

if (Test-Path $targetDir) {
    [HddGodMode.Optimizer]::Run($targetDir)
    Write-Host "`n处理完毕！请按回车键退出..." -ForegroundColor Green
} else {
    Write-Host "路径不存在！" -ForegroundColor Red
}
Read-Host
