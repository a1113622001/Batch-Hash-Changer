# 批量文件哈希值修改工具 (Batch-Hash-Changer)

批量修改文件哈希值的工具。通过在文件末尾追加微量数据改变文件的 MD5/SHA1/SHA256 特征指纹，用于规避网盘去重与秒传检测。

---

## 功能特性

- **指纹修改**：
  - MP4/MOV 文件：末尾注入标准 `free` box；
  - PNG 文件：末尾注入标准 `tEXt` 辅助块；
  - 其他文件：末尾追加 1~4 字节随机数据；
- **时间戳保留**：修改后使用 Win32 `FILETIME` 原生 API 还原文件的创建时间、修改时间和访问时间；
- **签名保护**：默认跳过 `.exe`、`.dll`、`.sys`、`.msi` 等文件，避免破坏数字签名；提供 `--force-all` 参数允许强制修改；
- **只读属性处理**：遇到只读文件时临时移除只读属性，写入并还原时间戳后恢复属性；
- **并发处理**：采用生产者-消费者队列，边遍历目录边并发写入；
- **独立运行**：纯 C 语言编写，编译为单个独立可执行文件（~77 KB），无需配置运行环境；
- **物理簇排序**：读取文件起始 LCN 簇号并升序排列，减少机械硬盘寻道时间。

---

## 使用说明

### 1. 拖拽运行
直接将文件夹拖拽到 `批量文件哈希值修改工具.exe` 图标上，处理完成后按回车键退出。

### 2. 交互模式
双击运行 `批量文件哈希值修改工具.exe`，按提示输入或拖入文件夹路径后按回车。

### 3. 命令行模式 (CLI)
```powershell
# 基础运行（默认跳过 .exe/.dll 等签名文件）
BatchHashChanger.exe "D:\TargetFolder"

# 强制修改所有文件（包含可执行文件）
BatchHashChanger.exe "D:\TargetFolder" --force-all

# 指定线程数并在执行完成后直接退出
BatchHashChanger.exe "D:\TargetFolder" -Threads 16 --no-pause
```

### 命令行参数

| 参数 | 说明 |
| :--- | :--- |
| `目标路径` | 需要处理的文件夹路径 |
| `-Threads, -t <数字>` | 指定工作线程数（默认自动按 CPU 核心数分配，上限 8） |
| `--force-all, -f` | 强制修改包含可执行/驱动在内的所有文件（默认跳过） |
| `--raw-noise` | 禁用 MP4/PNG 格式检测，统一在末尾追加 1~4 字节随机数据 |
| `--no-pause` | 处理完成后不等待回车，直接退出 |
| `-h, --help` | 查看帮助信息 |

---

## 编译与测试

### 1. 编译
执行目录下的构建脚本：
```cmd
build.bat
```
编译产物输出至 `bin/` 目录：
- `bin/BatchHashChanger.exe`
- `bin/批量文件哈希值修改工具.exe`
- `bin/test_suite.exe`

### 2. 运行测试
```cmd
bin\test_suite.exe
```

---

## 历史版本归档

历史基于 PowerShell 实现的脚本已归档至目录：
[`legacy/v11.1-powershell/`](legacy/v11.1-powershell/)

---

## 开源许可证

本项目基于 [MIT License](LICENSE) 授权开源。
