/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define PROMPT_LEN 16         // length of "> root@UCAS_OS: "
#define SHELL_BEGIN 20

#define MAX_ARG 16
#define INPUT_BUF_SIZE 128

#define BACKSPACE_FPGA 8
#define BACKSPACE_QEMU 127

#define ARR_SIZE(arr) sizeof(arr)/sizeof(arr[0])

// TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls
// TODO [P6-task2]: touch, cat, ln, ls -l, rm
// available commands
const char *commands[] = {
    "clear", "ps", "exec", "kill",
    "mkfs", "statfs", "mkdir", "ls",
    "cd", "rmdir", "touch", "cat",
    "ln", "rm"
};

enum cmds {
    CLEAR, PS, EXEC, KILL,
    MKFS, STATFS, MKDIR, LS,
    CD, RMDIR, TOUCH, CAT,
    LN, RM
} cmd_enum;

static inline void init_shell();
static inline void backspace();

static void read_keyboard(char *buf);
static void clear_buf(char *buf, int size);
static void parse_input(char *buf);

int main(void)
{
    init_shell();
    printf("> root@UCAS_OS: ");

    char buf[INPUT_BUF_SIZE];
    clear_buf(buf, INPUT_BUF_SIZE);

    while (1)
    {
        // call syscall to read UART port
        read_keyboard(buf);
        
        // parse input
        // note: backspace maybe 8('\b') or 127(delete)
        parse_input(buf);

        clear_buf(buf, INPUT_BUF_SIZE);
        printf("> root@UCAS_OS: ");
    }

    return 0;
}

static inline void init_shell() {
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
}

static inline void backspace() {
    sys_backspace(PROMPT_LEN);
}

static void read_keyboard(char *buf) {
    int ch = -1, i = 0;
    for (;;) {
        while ((ch = sys_getchar()) == -1);

        switch (ch) {
            case '\r':                            // Enter
                sys_write("\n");
                sys_reflush();
                buf[i] = 0;
                break;

            case BACKSPACE_QEMU:
            case BACKSPACE_FPGA:
                backspace();
                if (i > 0) buf[--i] = 0;
                break;

            default:
                buf[i++] = ch;
                sys_write((char *)&ch);
        }
        sys_reflush();
        if (ch == '\r') break;
    }
}

static void clear_buf(char *buf, int size) {
    for (int i = 0; i < size; i++) {
        buf[i] = 0;
    }
}

static void parse_input(char *buf) {
    int argc = 0;
    char *argv[MAX_ARG], *ptr = buf;
    int wait = 1;                       // whether to let shell wait until task finished
    pid_t pid;
    
    char *path = NULL, *option_str = NULL;
    int option = 0;

    for (int i = 0; i < MAX_ARG; i++) {
        argv[i] = NULL;
    }

    while (*ptr) {
        if (*ptr == ' ') {
            *ptr = 0;
            argv[argc++] = (char *)((uint64_t)ptr + 1);
        }

        ptr++;
    }

    if (*argv[argc - 1] == '&') {
        wait = 0;
        argv[--argc] = 0;
    }

    int index = 0;
    for (; index < ARR_SIZE(commands); index++) {
        if (!strcmp(buf, commands[index])) break;         // buf: command, like exec
    }

    
    switch(index) {
        case CLEAR:
            sys_clear();
            init_shell();
            break;
        case PS:
            sys_ps();
            break;
        case EXEC:
            pid = sys_exec(argv[0], argc, argv);
            if (pid > 0) {
                printf("execute %s successfully, pid = %d.\n", argv[0], pid);
            } else {
                printf("execute %s failed!\n", argv[0]);
            }

            if (wait && pid != 0) {
                sys_waitpid(pid);
            }
            break;
        case KILL:
            if (sys_kill(atoi(argv[0]))) {
                printf("kill pid %s successfully.\n", argv[0]);
            } else {
                printf("task with pid %s not found!\n", argv[0]);
            }
            break;
        case MKFS:
            sys_mkfs();
            break;
        case STATFS:
            sys_statfs();
            break;
        case MKDIR:
            sys_mkdir(argv[0]);
            break;
        case LS:
            /* 
              ls options:
              0 for default, 1 for "-a", 2 for "-l"
            */
            path = (argv[0] == NULL || argv[0][0] == '-') ? "." : argv[0];
            
            if (argv[0][0] == '-')      option_str = argv[0];
            else if (argv[1][0] == '-') option_str = argv[1];
            
            if (option_str[1] == 'a')      option = 1;
            else if (option_str[1] == 'l') option = 2;

            sys_ls(path, option);
            break;
        case CD:
            sys_cd(argv[0]);
            break;
        case RMDIR:
            sys_rmdir(argv[0]);
            break;
        case TOUCH:
            sys_touch(argv[0]);
            break;
        case CAT:
            sys_cat(argv[0]);
            break;
        case LN:
            sys_ln(argv[0], argv[1]);
            break;
        case RM:
            sys_rm(argv[0]);
            break;
        default:
            printf("Error: Unknown Command %s\n", buf);
    }
}
