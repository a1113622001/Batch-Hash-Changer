# ⚡ 批量文件哈希值修改工具 (C语言高性能独立版)
### 🚀 Batch File Hash Modifier (Native Win32 C Edition)

[![Release](https://img.shields.io/badge/Release-v11.1-blue.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Platform](https://img.shields.io/badge/Platform-Windows%207%20%7C%208%20%7C%2010%20%7C%2011%20%7C%20Server-0078D6.svg?style=flat-square&logo=windows)](https://github.com/a1113622001/Batch-Hash-Changer)
[![Binary Size](https://img.shields.io/badge/Size-~71%20KB-success.svg?style=flat-square)](bin/BatchHashChanger.exe)
[![Throughput](https://img.shields.io/badge/Throughput-~4000%2B%20files%2Fsec-brightgreen.svg?style=flat-square)](bin/test_suite.exe)
[![License](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

---

## 📖 项目简介

本仓库是 [Batch-Hash-Changer](https://github.com/a1113622001/Batch-Hash-Changer) 的 **纯 C 语言 (Win32 原生 API) 极致重构版**。

原版基于 PowerShell + C# 动态内存编译，需要启动 PowerShell 进程及 .NET Roslyn 编译环境。本 C 语言重构版秉持 **“小而美” (Small & Lean)** 的设计哲学：
- 📦 **单个完全独立的打包可执行文件**：最终仅需一个 `BatchHashChanger.exe`（体积仅 **~71 KB**）；
- ⚡ **零外部依赖与毫秒级秒开**：无需安装 .NET Framework、PowerShell 或任何第三方 DLL，仅依赖 Windows 自带的系统动态库；
- 🎯 **功能与原版 100% 严格一致**：完美复刻所有核心特性、交互体验与色彩看板。

---

## ✨ 核心特性

1. 🎯 **文件指纹毫秒级唯一化**：尾部零分配追加 1~4 字节随机噪点，毫秒级改变 `MD5` / `SHA1` / `SHA256` 等特征指纹，不破坏视频、音频、图片及压缩包的完整性。
2. 🧊 **Win32 `FILETIME` 原生高精度时间戳冻结**：修改前后锁定创建时间、最后访问时间与修改时间，毫秒不差原样还原。
3. 🛗 **HDD 物理簇（LCN）电梯调度**：通过 `FSCTL_GET_RETRIEVAL_POINTERS` 获取扇区物理簇号并升序排队，最大化降低机械硬盘磁头寻道延迟。
4. ⚡ **多线程自适应并行流水线**：自动识别 CPU 核心数进行多线程并发（上限 8，支持命令行 `-Threads` 自定义）。
5. 🔒 **防锁死共享流模式**：以 `ReadWrite | Delete` 共享模式打开，极大降低被杀毒软件或索引进程占用冲突的失败率。
6. 🖱️ **一键双击即用与拖拽支持**：双击即可进入交互界面，支持直接拖入文件夹并自动去除首尾引号与空格。
7. 🛡️ **运行自身安全排除**：自动获取并排除当前程序自身，防止误操作修改工具自身。
8. 🌐 **原生长路径与 UTF-8 支持**：采用 Windows 原生清单元数据，完整支持深层嵌套长路径（> 260 字符）与中文字符。

---

## 🚀 使用指南

### 1. 交互模式（推荐）
直接双击运行 **`BatchHashChanger.exe`**（或 **`批量文件哈希值修改工具.exe`**）：
```text
==================================================
  批量文件哈希值修改工具 v11.1（C语言高性能独立版）
==================================================
请输入要处理的文件夹路径 (例如 D:\TestFolder): 
```
直接将目标文件夹拖入窗口后按回车即可。

### 2. 命令行模式 (CLI)
```powershell
# 基础运行（自动根据 CPU 分配线程）
BatchHashChanger.exe "D:\Downloads\TargetFolder"

# 手动指定 16 线程并行加速
BatchHashChanger.exe "D:\Downloads\TargetFolder" -Threads 16
```

---

## 🛠️ 构建与测试

### 1. 一键编译
本项目提供一键构建脚本 `build.bat`：
```cmd
build.bat
```
编译产物位于 `bin/` 目录下：
- `bin/BatchHashChanger.exe`：主程序 (~71 KB)
- `bin/批量文件哈希值修改工具.exe`：中文命名副本 (~71 KB)
- `bin/test_suite.exe`：自动化测试套件

### 2. 运行自动化测试套件
```cmd
bin\test_suite.exe
```
测试套件包含 6 大自动化测试场景：
- [x] 单文件哈希微修改与前缀无损测试
- [x] Win32 FILETIME 原生高精度时间戳冻结测试
- [x] 0 字节空文件修改测试
- [x] 中文/特殊符号/多级嵌套路径测试
- [x] 递归扫描器与运行自身排除测试
- [x] 多线程并发流水线端到端批量测试

---

## 📄 开源许可证

本项目基于 [MIT License](LICENSE) 开源。
