<#
  ================================================================
  批量文件哈希值修改工具 v11.0（优化版）
  ----------------------------------
  原理：在每个文件末尾追加 1~4 个随机字节，从而改变文件的
  MD5/SHA1 等哈希值（文件指纹），用于规避云盘"秒传/去重"机制。

  优化内容（相对 v10.6）：
    1) 真正保留原始时间戳：写入前读取原始时间，写入后原样还原
       （v10.6 错误地把时间戳设成 0xFFFFFFFF 这个"垃圾未来日期"）。
    2) 磁盘簇号(LCN)读取改为可变大缓冲，分片文件也能正确排序，
       电梯算法对 HDD 的优化不再因 32 字节小缓冲而失效。
    3) 写入失败与跳过数量单独统计并汇总报告，不再静默吞错。
    4) 增加处理进度、速率与预计剩余时间显示。
    5) 文件共享方式改为读/写/删除共享，降低"被占用"导致失败。
    6) 附同级 .bat 一键启动器（v10.6 的 .bat 并非真正的批处理脚本，双击无法运行）。
    7) 增加管理员权限检测与友好提示。
  ================================================================
#>
param([string]$TargetDirectory = '')

[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$ErrorActionPreference = 'Stop'

$code = @"
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;

namespace HashMod
{
    public static class Processor
    {
        private const uint GenericWrite   = 0x40000000;
        private const uint FileAppendData = 0x00000100;
        private const uint FileReadAttr   = 0x00000080;
        private const uint BackupSemantics= 0x02000000;
        private const uint OpenExisting   = 3;
        private const uint FsctlGetRetrievalPointers = 0x00090073;
        private const int  ErrorMoreData  = 234;
        private static readonly IntPtr Invalid = new IntPtr(-1);

        public static void Run(string directory, int batchSize, string[] exclude)
        {
            var sw = Stopwatch.StartNew();
            var excluded = new HashSet<string>(exclude ?? new string[0], StringComparer.OrdinalIgnoreCase);

            string[] all;
            try
            {
                all = Directory.EnumerateFiles(directory, "*.*", SearchOption.AllDirectories)
                               .Where(f => !excluded.Contains(f))
                               .ToArray();
            }
            catch (Exception ex)
            {
                Console.WriteLine("[错误] 扫描目录失败: " + ex.Message);
                return;
            }

            Console.WriteLine("[1/3] 扫描完成：共 " + all.Length + " 个文件");
            if (all.Length == 0) { Console.WriteLine("没有可处理的文件。"); return; }

            int processed = 0, skipped = 0, lastReported = 0;
            var rnd = new Random();
            var batch = new List<string>(batchSize);

            foreach (var file in all)
            {
                batch.Add(file);
                if (batch.Count >= batchSize)
                {
                    ProcessBatch(batch, rnd, ref processed, ref skipped, all.Length, sw, ref lastReported);
                    batch.Clear();
                }
            }
            if (batch.Count > 0)
                ProcessBatch(batch, rnd, ref processed, ref skipped, all.Length, sw, ref lastReported);

            sw.Stop();
            Console.WriteLine();
            Console.WriteLine("[3/3] 处理完成：成功 " + processed + " 个，跳过 " + skipped + " 个，耗时 " + sw.Elapsed.TotalSeconds.ToString("F2") + " 秒");
        }

        private static void ProcessBatch(List<string> batch, Random rnd, ref int processed, ref int skipped, int total, Stopwatch sw, ref int lastReported)
        {
            var items = batch.AsParallel()
                .Select(p => new KeyValuePair<long, string>(GetPhysicalCluster(p), p))
                .ToList();

            // 无法获取簇号的(LCN<0)排到最后，仍正常处理；其余按物理簇号升序(电梯算法)
            for (int i = 0; i < items.Count; i++)
                if (items[i].Key < 0) items[i] = new KeyValuePair<long, string>(long.MaxValue, items[i].Value);
            items.Sort((a, b) => a.Key.CompareTo(b.Key));

            foreach (var item in items)
            {
                if (AppendDataWithFreeze(item.Value, rnd)) processed++; else skipped++;
                int cur = processed + skipped;
                if (cur - lastReported >= 1000)
                {
                    lastReported = cur;
                    Report(processed, skipped, total, sw);
                }
            }
        }

        private static void Report(int processed, int skipped, int total, Stopwatch sw)
        {
            double pct  = total > 0 ? (processed + skipped) * 100.0 / total : 0;
            double rate = sw.Elapsed.TotalSeconds > 0 ? (processed + skipped) / sw.Elapsed.TotalSeconds : 0;
            double eta  = rate > 0 ? (total - processed - skipped) / rate : 0;
            Console.WriteLine(string.Format("[2/3] 进度 {0}/{1} ({2:F1}%)  成功 {3}  跳过 {4}  速率 {5:F0} 个/秒  已用 {6:F1}s  预计剩余 {7:F0}s",
                processed + skipped, total, pct, processed, skipped, rate, sw.Elapsed.TotalSeconds, eta));
        }

        // 获取文件第一个数据区在磁盘上的物理簇号(LCN)；失败返回 -1
        private static long GetPhysicalCluster(string filePath)
        {
            IntPtr h = CreateFileW(@"\\?\" + filePath, FileReadAttr,
                                   FileShare.Read | FileShare.Write | FileShare.Delete,
                                   IntPtr.Zero, OpenExisting, BackupSemantics, IntPtr.Zero);
            if (h == Invalid) return -1;
            try
            {
                long startingVcn = 0;
                IntPtr buf = Marshal.AllocHGlobal(4096);   // 可变大缓冲，兼容分片文件
                try
                {
                    Marshal.WriteInt64(buf, 0, startingVcn);
                    uint bytesReturned;
                    bool ok = DeviceIoControl(h, FsctlGetRetrievalPointers, buf, 8, buf, 4096, out bytesReturned, IntPtr.Zero);
                    if (!ok && Marshal.GetLastWin32Error() != ErrorMoreData) return -1;
                    int extentCount = Marshal.ReadInt32(buf, 0);      // ExtentCount @0
                    if (extentCount <= 0) return -1;
                    return Marshal.ReadInt64(buf, 24);                 // 第一个 extent 的 Lcn @24
                }
                finally { Marshal.FreeHGlobal(buf); }
            }
            finally { CloseHandle(h); }
        }

        // 在末尾追加 1~4 个随机字节，并还原原始时间戳
        private static bool AppendDataWithFreeze(string filePath, Random rnd)
        {
            IntPtr h = CreateFileW(@"\\?\" + filePath, GenericWrite | FileAppendData,
                                   FileShare.Read | FileShare.Write | FileShare.Delete,
                                   IntPtr.Zero, OpenExisting, BackupSemantics, IntPtr.Zero);
            if (h == Invalid) return false;
            try
            {
                FILETIME c, a, w;
                bool timesOk = GetFileTime(h, out c, out a, out w);   // 记录原始时间戳

                byte[] data = new byte[rnd.Next(1, 5)];
                rnd.NextBytes(data);
                long newPtr;
                SetFilePointerEx(h, 0, out newPtr, 2);   // 定位到文件末尾，确保"追加"而非覆盖开头
                uint written;
                if (!WriteFile(h, data, (uint)data.Length, out written, IntPtr.Zero)) return false;

                if (timesOk) SetFileTime(h, ref c, ref a, ref w);     // 还原时间戳
                return true;
            }
            finally { CloseHandle(h); }
        }

        [StructLayout(LayoutKind.Sequential)]
        private struct FILETIME { public uint dwLowDateTime; public uint dwHighDateTime; }

        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern IntPtr CreateFileW(string lpFileName, uint dwDesiredAccess, FileShare dwShareMode, IntPtr lpSecurityAttributes, uint dwCreationDisposition, uint dwFlagsAndAttributes, IntPtr hTemplateFile);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool CloseHandle(IntPtr hObject);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool DeviceIoControl(IntPtr hDevice, uint dwIoControlCode, IntPtr lpInBuffer, int nInBufferSize, IntPtr lpOutBuffer, int nOutBufferSize, out uint lpBytesReturned, IntPtr lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool WriteFile(IntPtr hFile, byte[] lpBuffer, uint nNumberOfBytesToWrite, out uint lpNumberOfBytesWritten, IntPtr lpOverlapped);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetFilePointerEx(IntPtr hFile, long liDistanceToMove, out long lpNewFilePointer, uint dwMoveMethod);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool GetFileTime(IntPtr hFile, out FILETIME lpCreationTime, out FILETIME lpLastAccessTime, out FILETIME lpLastWriteTime);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern bool SetFileTime(IntPtr hFile, ref FILETIME lpCreationTime, ref FILETIME lpLastAccessTime, ref FILETIME lpLastWriteTime);
    }
}
"@

Add-Type -TypeDefinition $code -Language CSharp

# ---------- 交互输入 ----------
if ($TargetDirectory -eq '') {
    Clear-Host
    Write-Host "==================================================" -ForegroundColor Cyan
    Write-Host "  批量文件哈希值修改工具 v11.0（优化版）" -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Cyan
    $TargetDirectory = Read-Host "请输入要处理的文件夹路径 (例如 D:\TestFolder)"
}
$TargetDirectory = ($TargetDirectory.Trim().Trim('"').Trim("'"))
if (-not $TargetDirectory -or -not (Test-Path -LiteralPath $TargetDirectory)) {
    Write-Host ("路径不存在：" + $TargetDirectory) -ForegroundColor Red
    Read-Host "按回车退出"
    exit 1
}
$TargetDirectory = (Get-Item -LiteralPath $TargetDirectory).FullName

# 排除本脚本及其同级 .bat 启动器，避免修改到工具自身
$exclude = New-Object System.Collections.Generic.List[string]
[void]$exclude.Add($PSCommandPath)
try {
    $bat = [System.IO.Path]::ChangeExtension($PSCommandPath, '.bat')
    if (Test-Path -LiteralPath $bat) { [void]$exclude.Add($bat) }
} catch {}

# 检测管理员权限
try {
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (-not $isAdmin) { Write-Host "提示：建议以管理员身份运行（读取磁盘簇号需要相应权限）。" -ForegroundColor Yellow }
} catch {}

# ---------- 执行 ----------
Write-Host ("开始处理目录：" + $TargetDirectory) -ForegroundColor Cyan
[HashMod.Processor]::Run($TargetDirectory, 5000, [string[]]$exclude.ToArray())
Write-Host ""
Write-Host "处理完毕！" -ForegroundColor Green
Write-Host "警告：修改哈希会破坏文件完整性校验，请谨慎使用。" -ForegroundColor Yellow
Read-Host "按回车退出"