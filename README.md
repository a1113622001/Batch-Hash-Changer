# ⚡ 批量文件哈希值修改工具 v13.0 (C语言高性能流式独立版)
### 🚀 Batch File Hash Modifier (Native Win32 C Edition)

[![Release](https://img.shields.io/badge/Release-v13.0-blue.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Platform](https://img.shields.io/badge/Platform-Windows%207%20%7C%208%20%7C%2010%20%7C%2011%20%7C%20Server-0078D6.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Binary Size](https://img.shields.io/badge/Size-~77%20KB-success.svg?style=flat-square)](bin/BatchHashChanger.exe)
[![Throughput](https://img.shields.io/badge/Throughput-~4000%2B%20files%2Fsec-brightgreen.svg?style=flat-square)](bin/test_suite.exe)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

---

## 📖 项目简介

**Batch-Hash-Changer** 是一款面向海量文件的高性能文件哈希值批量修改工具。通过在文件末尾**零分配追加随机噪点或格式感知合法元数据块**，毫秒级改变文件的 `MD5` / `SHA1` / `SHA256` 等特征指纹，专为规避云盘（如百度网盘、阿里云盘、115网盘等）的**“秒传/特征码去重与拦截”**机制而设计。

v13.0 版本基于 **纯 C 语言 (Win32 原生 API)** 深度重构，极致贯彻 **“小而美” (Small & Lean)** 的设计哲学：
- 📦 **单文件完全独立运行**：输出单个可执行文件（体积仅 **~77 KB**），零外部依赖，免装 .NET Framework / PowerShell，双击秒开；
- 🛡️ **智能格式感知与签名保护**：默认安全跳过带有 Authenticode 数字签名的文件（`.exe` / `.dll` 等），支持 MP4 / PNG 格式原生合法元数据块注入；
- 🔄 **只读属性自动兼容**：自动识别只读文件并在处理后无损还原只读标记；
- ⚡ **生产者-消费者流式流水线**：目录递归扫描与多线程处理 100% 并发重叠，消除扫描等待。

---

## ✨ 核心特性

1. 🎯 **文件指纹毫秒级唯一化**：
   - **MP4 / MOV 容器**：注入标准的 ISO-BMFF `free` box（带合法长度标头），完全符合多媒体规范，各大播放器无感解析；
   - **PNG 图像**：注入标准的 `tEXt` 辅助块（带正确 CRC32 校验），各类看图工具与浏览器原生兼容；
   - **通用格式**：尾部追加 1~4 字节随机噪点，不破坏压缩包与普通文件结构。
2. 🛡️ **可执行文件签名保护**：默认跳过 `.exe`, `.dll`, `.sys`, `.msi` 等文件以防破坏数字签名，支持 `--force-all` 参数强制修改。
3. 🔒 **只读属性自动兼容**：遭遇 `FILE_ATTRIBUTE_READONLY` 只读文件时自动临时剥离属性，处理完成后毫秒级原样恢复。
4. 🧊 **Win32 `FILETIME` 原生高精度时间戳冻结**：修改前后锁定创建时间、最后访问时间与修改时间，毫秒不差原样还原。
5. 🛗 **HDD 物理簇（LCN）电梯调度**：通过 `FSCTL_GET_RETRIEVAL_POINTERS` 获取扇区物理簇号并升序排队，极大减少机械硬盘磁头寻道延迟。
6. ⚡ **生产者-消费者并发流水线**：流式无锁环形队列，边扫描边并发处理，在百万级小文件下彻底消除扫描等待。
7. 🖱️ **图标直接拖拽与防闪退**：直接将文件夹拖拽到 `BatchHashChanger.exe` 文件图标上即可自动运行并停留显示统计看板。

---

## ⚡ 性能实测 (Benchmark)

在真实测试环境（Windows 11 / 10,000 个测试文件）下的实测吞吐数据：

| 处理版本 | 10,000 文件耗时 | 处理吞吐 (速率) | 成功率 | 提升倍数 |
| :--- | :--- | :--- | :--- | :--- |
| 原版 v10.6 (单线程) | ~14.2 秒 | ~700 个/秒 | 100% | 1.0x |
| 原版 v11.0 (LCN优化) | ~7.0 秒 | ~1,430 个/秒 | 100% | 2.0x |
| 原版 v11.1 (C# 并行) | ~3.9 秒 | ~2,540 个/秒 | 100% | 3.6x |
| **🚀 C 语言流式重构版 (v13.0)** | **🔥 2.38 ~ 3.4 秒** | **🔥 3,000 ~ 4,200+ 个/秒** | **100% 全成** | **🔥 比 C# 快 1.7x** |

---

## 🚀 使用指南

### 1. 图标拖拽启动（推荐）
直接将需要处理的文件夹 **拖拽到 `批量文件哈希值修改工具.exe` 图标上释放**，程序将自动启动并在处理完成后停留显示统计面板。

### 2. 交互模式（双击运行）
直接双击运行 **`批量文件哈希值修改工具.exe`**：
```text
===================================================================
  批量文件哈希值修改工具 v13.0（C语言流式高性能版）
===================================================================
请输入要处理的文件夹路径 (例如 D:\TestFolder): 
```
按提示输入或拖入文件夹路径后按回车。

### 3. 命令行模式 (CLI)
```powershell
# 1. 基础调用（自动多线程并发，默认跳过 .exe/.dll 等签名文件）
BatchHashChanger.exe "D:\Downloads\TargetFolder"

# 2. 强制修改所有文件（包含可执行与驱动文件）
BatchHashChanger.exe "D:\Downloads\TargetFolder" --force-all

# 3. 指定 16 线程加速且运行完毕不暂停
BatchHashChanger.exe "D:\Downloads\TargetFolder" -Threads 16 --no-pause
```

---

## 🛠️ 构建与测试

### 1. 一键编译
```cmd
build.bat
```
编译产物位于 `bin/` 目录下：
- `bin/BatchHashChanger.exe`：主程序 (~77 KB)
- `bin/批量文件哈希值修改工具.exe`：中文命名副本 (~77 KB)
- `bin/test_suite.exe`：全量自动化测试套件

### 2. 运行自动化测试套件
```cmd
bin\test_suite.exe
```
测试套件包含 7 大自动化测试场景：
- [x] 单文件哈希微修改与前缀无损测试
- [x] Win32 FILETIME 原生高精度时间戳冻结测试
- [x] 只读文件属性自动兼容与恢复测试
- [x] 可执行文件签名保护（默认跳过与 `--force-all`）测试
- [x] MP4 容器 ISO-BMFF `free` Box 格式感知注入测试
- [x] PNG 图像 `tEXt` 辅助 Chunk 格式感知注入测试
- [x] 生产者-消费者流式流水线端到端测试

---

## 📄 开源许可证

本项目采用 [MIT License](LICENSE) 授权开源。
