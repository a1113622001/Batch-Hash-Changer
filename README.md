<div align="center">

# ⚡ Batch File Hash Modifier (批量文件哈希修改工具)
### 🚀 基于 Win32 P/Invoke 与物理簇排序的毫秒级文件指纹批量修改利器

[![Release](https://img.shields.io/badge/Release-v11.1-blue.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011%20%7C%20Server-0078D6.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![PowerShell](https://img.shields.io/badge/PowerShell-5.1%2B%20%7C%207%2B-5391FE.svg?style=flat-square&logo=powershell)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Throughput](https://img.shields.io/badge/Throughput-~2600%20files%2Fsec-success.svg?style=flat-square&logo=speedtest)](https://github.com/a1113622001/Batch-Hash-Changer)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

</div>

---

## 📖 项目简介

**Batch-Hash-Changer** 是一款面向海量文件的高性能文件哈希值批量修改工具。通过在文件末尾**零分配追加 1~4 个随机字节**，毫秒级改变文件的 `MD5` / `SHA1` / `SHA256` 等特征指纹，专为规避云盘（如百度网盘、阿里云盘、115网盘等）的**“秒传/特征码去重与拦截”**机制而设计。

结合 **C# 动态内存编译**、**Win32 原生 P/Invoke**、**HDD 物理簇号（LCN）电梯排序** 与 **元数据时间戳冻结** 技术，在保证文件内容完整可用与文件属性完全不变的前提下，实现了万级文件的秒级处理。

---

## ⚡ 性能实测 (Benchmark)

在真实测试环境（Windows 11 / NVMe & HDD 混合场景）下的实测吞吐数据：

| 处理规模 | v10.6 (单线程/小缓冲) | v11.0 (LCN优化) | v11.1 (多线程并行加速) 🚀 | 吞吐提升比 |
| :--- | :--- | :--- | :--- | :--- |
| **10,000 文件** | ~14.2 秒 (~700/s) | ~7.0 秒 (~1,430/s) | **~3.9 秒 (~2,540/s)** | **🔥 3.6x** |
| **20,000 文件** | ~28.5 秒 (~701/s) | ~13.9 秒 (~1,438/s) | **~7.7 秒 (~2,600/s)** | **🔥 3.7x** |

---

## ✨ 核心特性

- 🎯 **文件指纹毫秒级唯一化**：尾部微注入 1~4 字节随机噪点，不破坏多媒体（MP4/MKV/MP3/PNG）及压缩包（ZIP/7z/RAR）的结构完整性与可读性。
- 🧊 **MFT 时间戳完美冻结**：修改前后通过 Win32 `FILETIME` 原生 API 精确锁定创建时间、最后访问时间与修改时间，杜绝留下文件被修改的时间痕迹。
- 🛗 **HDD 物理簇（LCN）电梯调度**：利用 `FSCTL_GET_RETRIEVAL_POINTERS` 获取扇区物理簇号并升序排队，极大减少机械硬盘磁头往返寻道延迟。
- ⚡ **多线程自适应并发**：自动识别 CPU 核心数进行多线程并发流水线追加（支持手动指定 `-Threads`）。
- 🔒 **防锁死共享流打开**：以 `ReadWrite | Delete` 共享模式打开文件句柄，大幅降低因后台杀毒软件或索引扫描导致的占用失败率。
- 🖱️ **一键双击即用**：提供开箱即用的 `.bat` 引导器，双击即可进入交互界面。

---

## 🏗️ 工作流程

```mermaid
flowchart TD
    A[选择目标文件夹] --> B[流式枚举文件列表]
    B --> C[DeviceIoControl 获取物理簇号 LCN]
    C --> D[按 LCN 物理扇区升序排队 (电梯调度)]
    D --> E[分配多线程并行写入任务 (Thread Pool)]
    
    subgraph Worker [单个文件原子处理流程]
        E1[GetFileTime 锁定原始时间戳] --> E2[CreateFile 追加 1~4 字节随机噪点]
        E2 --> E3[SetFileTime 还原原始时间戳]
        E3 --> E4[释放句柄完成]
    end
    
    E --> Worker
    Worker --> F[输出处理速率与成功统计]
```

---

## 🚀 快速上手

### 1. 双击运行（推荐）
直接双击运行目录下的 **`批量文件哈希值修改工具 v11.1.bat`**，按照交互提示输入或拖拽文件夹路径即可。

### 2. 命令行调用 (CLI)
```powershell
# 绕过执行策略直接运行
powershell -NoProfile -ExecutionPolicy Bypass -File "批量文件哈希值修改工具 v11.1.ps1" "D:\Downloads\TargetFolder"

# 指定并发线程数（默认自适应，上限 8）
powershell -NoProfile -ExecutionPolicy Bypass -File "批量文件哈希值修改工具 v11.1.ps1" "D:\Downloads\TargetFolder" -Threads 16
```

> 💡 **提示**：读取物理簇号（LCN）属于底层存储操作，建议以**管理员身份运行**以激活全部磁盘寻道优化。

---

## ⚠️ 安全须知 (Disclaimer)

1. 该工具会对文件末尾追加微量字节，直接导致文件的校验和（CRC32 / MD5 / SHA1）改变；
2. 对于具备严格签名校验（如可执行程序 `.exe`、`.dll` 驱动等）的文件，修改后可能导致签名失效，请谨慎处理；
3. 大规模批量处理前，建议做好数据备份。

---

## 📄 开源许可证

本项目采用 [MIT License](LICENSE) 授权开源。
