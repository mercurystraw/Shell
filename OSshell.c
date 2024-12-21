#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include<stdbool.h>

#define LINE_SIZE 1024  // 命令行最大长度
#define ARGC_SIZE 32    // 命令行参数个数最大个数
#define EXIT_CODE 55    // 设定的退出代码

int lastcode = 0; // 最后执行命令的返回值
char pwd[LINE_SIZE];          // 当前工作目录
char hostname[64];            // 主机名
char commandline[LINE_SIZE];  // 存放命令行
char *argv[ARGC_SIZE];        // 存放命令行参数,字符指针数组
char *internalCommands[] = {"exit", "cd", "help", "echo", NULL}; // 内部命令列表


// 函数声明，避免后面函数相互调用受到定义的先后顺序 影响
const char* getusername();                               // 获取当前用户名
void get_host_name();                                    // 获取主机名   
void getpwd();                                           // 获取当前工作目录
void print_prompt();                                     // 打印命令行提示符
void get_command(char *cline, int size);                 // 从标准输入获取命令行
int splitCline_getArgcv(char cline[], char *_argv[]);    // 分割命令行，存储命令参数并且返回参数个数
void runExternalCommands(char *_argv[]);                 // 执行外部命令
bool isInternalCommands(char *_argv[]);                  // 判断是否为内部命令
void runInternalCommands(char *_argv[], int argc);       // 执行内部命令
void handle_basic_commands(char *_argv[], int argc);     // 处理基本命令（外部和内部命令）
void handle_mayRedir_commands(char commands[]);          // 处理可能带重定向的命令（即基本命令+重定向）
void handle_single_pipe(char commands[]);                // 处理带单个管道命令
void handle_multiple_pipe(char commands[]);              // 处理带多个管道命令
int count_pipe(const char *commands);                    // 统计获取命令行中管道符的个数
bool isRedir(char *commands);                            //判断传入的命令是否含有重定向符号
void handle_redirection(char *commands);                 // 处理带有重定向符号的命令


// 获取当前用户名
const char* getusername() {
    return getenv("USER");
}
// 获取主机名
void get_host_name() {
    gethostname(hostname, sizeof(hostname));
}

// 获取当前工作目录
void getpwd() {
    getcwd(pwd, sizeof(pwd));
}

// 打印命令行提示符
void print_prompt() {
    get_host_name();
    getpwd();
    printf("\033[31m%s@%s %s$ \033[0m", getusername(), hostname, pwd);
}

// 从标准输入获取命令行
void get_command(char *cline, int size) {
    if (fgets(cline, size, stdin) != NULL) { //使用fgets函数从标准输入获取命令行输入
        cline[strcspn(cline, "\n")] = '\0'; // 使用strcspn函数找到换行符并且移除
    } else {
        if (feof(stdin)) {
            printf("EOF reached\n");
        } else {
            perror("fgets error");
        }
        exit(EXIT_CODE);
    }
}

// 分割命令行，存储命令参数并且返回参数个数
int splitCline_getArgcv(char cline[], char *_argv[]) {
    int i = 0;
    char *token = strtok(cline, " \t"); // 命令名称
    while (token != NULL) {
        _argv[i++] = token;            // 保存命令或者其参数
        token = strtok(NULL, " \t");   // 获取下一个 token
    }
    _argv[i] = NULL; // 确保以 NULL 结尾，满足 execvp 要求
    return i;        // 返回参数个数
}

// 利用子进程去执行外部命令
void runExternalCommands(char *_argv[]) {
    pid_t pid = fork();
    
    if (pid < 0) { // fork 失败
        perror("Fork failed");
        return;
    }
    if (pid == 0) { // 子进程执行命令
        //printf("内部指令运行，Child process executing command %s\n", _argv[0]);
        execvp(_argv[0], _argv); //execvp 会从系统的 PATH 环境变量指定的目录中查找 _argv[0] 命令并执行
        fprintf(stderr, "execvp error: Command not found or failed to execute: %s\n", _argv[0]);
        exit(EXIT_CODE); // 退出子进程
    } else { // 父进程等待子进程结束
        int status;
        if (waitpid(pid, &status, 0) == -1) { // 等待子进程结束
            perror("Waitpid failed");
        } else if (WIFEXITED(status)) { // 子进程正常退出
            lastcode = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) { // 子进程因信号终止
            fprintf(stderr, "Process terminated by signal %d\n", WTERMSIG(status));
            lastcode = EXIT_CODE; // 设为错误代码
        }
    }
}

bool isInternalCommands(char *_argv[]) {
    for(int i=0; internalCommands[i]!= NULL; i++){
        if(strcmp(_argv[0], internalCommands[i]) == 0)return true;
    }
    return false;
}
// 内部命令处理
void runInternalCommands(char *_argv[], int argc){
    if (argc == 0) {
        fprintf(stderr, "No Internal commands specified\n");
        return;
    }

    else if (strcmp(_argv[0], "exit") == 0) { // exit命令
        exit(0);
    }

    else if(strcmp(_argv[0], "cd") == 0) { // cd命令
        if (argc == 1) { // 只有一个cd命令，未指定目录，切换到用户主目录
            chdir(getenv("HOME"));
        } else {
            if(chdir(_argv[1]) != 0) { // 切换到指定目录 argv[1]
                perror("chdir error");
            }
        }
        getpwd(); // 更新当前目录
        return;
    }

    else if(strcmp(_argv[0], "echo") == 0){ // echo命令
        if (argc == 1) { // 只有一个echo命令，打印换行符
            printf("\n");
        } else{ // 打印跟在后面的string，即使中间带了空格，会被存在argv中。
            for (int i = 1; _argv[i]!=NULL && i < argc; i++) {
                printf("%s ", _argv[i]); // 中间加空格
            }
            printf("\n");
        }
        return;
    }

    else if(strcmp(_argv[0], "help")== 0){ // help命令，打印内部命令列表
        printf("Internal commands:\n");
        printf("  exit\n");
        printf("  cd [directory]\n");
        printf("  help\n");
        printf("  echo [string]\n");
        return;
    }
}
void handle_basic_commands(char *_argv[], int argc){  //处理不带管道和重定向的基本命令
    if(isInternalCommands(_argv)){ // 内部命令执行
        runInternalCommands(_argv, argc);
    }
    else{
        runExternalCommands(_argv);       // 外部命令执行
    }
}

void handle_mayRedir_commands(char commands[]){ //处理可能有重定向符号命令的函数，handle_basic_commands的进阶版。
    if(isRedir(commands)){ // 重定向符号存在
        handle_redirection(commands); //处理重定向
    }
    else{
        char *argv[ARGC_SIZE];
        int argc = splitCline_getArgcv(commands, argv); // 分割命令行，存储参数
        handle_basic_commands(argv, argc); // 处理基本命令
    }
}

void handle_single_pipe(char commands[]){
    //strtok 函数在处理字符串时，会用 \0 替换分隔符（在您代码中的 " \t" 和 |），
    //这意味着当您查找 | 符号时，strtok 会在遇到第一个分隔符（如下空格）时将其替换为 \0，从而结束处理的字符串。
    //so commandline became ls instead of ls | wc -l

    //printf("handle_single_pipe\n");
    //printf("commands: %s\n", commands);

    int pipefd[2]; // 管道文件描述符
    pid_t pid;
    char *pipe_pos = strchr(commands, '|');
    //printf("pipe_pos: %d\n", *pipe_pos);

    if(pipe_pos){
        //printf("pipe found\n");
        *pipe_pos = '\0'; // 将管道符号替换为字符串结束符，变成左右两个字符串
        char * left_command = commands;
        char * right_command = pipe_pos + 1;

        //printf("left_command: %s\n", left_command);
        //printf("right_command: %s\n", right_command);
    
        if(pipe(pipefd) < 0){
            perror("pipr error");
            exit(EXIT_CODE);
        }

        // 创建第一个子进程，执行左边命令
        if ((pid = fork()) == 0) {
            // 子进程执行左边命令
            //printf("child process execute left command\n");
            close(pipefd[0]); // 关闭读端
            dup2(pipefd[1], STDOUT_FILENO); // 将管道写端文件描述符复制给标准输出
            close(pipefd[1]);

            handle_mayRedir_commands(left_command); // 处理左边命令
            exit(EXIT_SUCCESS); // 退出子进程 ！！！非常重要，补充到报告中!
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_CODE);
        }

        // 创建第二个子进程，执行右边命令
        if ((pid = fork()) == 0) {
            // 子进程执行右边命令
            //printf("child process execute right command\n");
            close(pipefd[1]); // 关闭写端
            dup2(pipefd[0], STDIN_FILENO); // 将管道读端文件描述符复制给标准输入
            close(pipefd[0]);

            handle_mayRedir_commands(right_command); // 处理右边命令
            exit(EXIT_SUCCESS); // 退出子进程
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_CODE);
        }

        close(pipefd[0]);
        close(pipefd[1]);
         // 等待所有子进程结束
        while (wait(NULL) > 0);
    }
}

void handle_multiple_pipe(char commands[]){ //多重管道情况
    //printf("Handle_multiple_pipe\n");
    //printf("commands: %s\n", commands);

    int pipefd[2]; // 管道文件描述符
    pid_t pid;

    char *pipe_pos = strchr(commands, '|');
    //printf("pipe_pos: %d\n", *pipe_pos);

    if(pipe_pos){ // 管道符号存在
        //printf("pipe found\n");
        *pipe_pos = '\0'; // 将管道符号替换为字符串结束符，变成左右两个字符串
        char * left_command = commands;
        char * right_command = pipe_pos + 1;

        //printf("left_command: %s\n", left_command);
        //printf("right_command: %s\n", right_command);
    
        if(pipe(pipefd) < 0){
            perror("pipr error");
            exit(EXIT_CODE);
        }

        // 创建第一个子进程，执行左边命令
        if ((pid = fork()) == 0) {
            // 子进程执行左边命令
            //printf("child process execute left command\n");
            close(pipefd[0]); // 关闭读端
            dup2(pipefd[1], STDOUT_FILENO); // 将管道写端文件描述符复制给标准输出
            close(pipefd[1]);

            handle_mayRedir_commands(left_command); // 处理左边命令
            exit(EXIT_SUCCESS); // 退出子进程 ！！！非常重要，补充到报告中!
        } else if (pid < 0) {
            perror("fork");
            exit(EXIT_CODE);
        }

        if((pid =fork())==0){ // 创建第二个子进程，执行右边命令
            // 子进程执行右边命令
            //printf("child process execute right command\n");
            close(pipefd[1]); // 关闭写端
            dup2(pipefd[0], STDIN_FILENO); // 将管道读端文件描述符复制给标准输入
            close(pipefd[0]);

            
            // 递归处理右边命令
            if(strchr(right_command, '|')!= NULL){ //中止条件
                handle_multiple_pipe(right_command); 
            }
            else{ // 右边命令没有管道的情况

                handle_mayRedir_commands(right_command); // 处理右边命令
            }
            exit(EXIT_SUCCESS); // 退出子进程

        }
        else if (pid < 0) {
            perror("fork");
            exit(EXIT_CODE);
        }

        close(pipefd[0]);
        close(pipefd[1]);

        while(wait(NULL)>0); // 等待所有子进程结束
    }
}

int count_pipe(const char *commands){
    int count = 0;
    while(*commands != '\0'){ 
        if(*commands =='|' )count++;
        commands++;
    }
    return count;
}


bool isRedir(char *commands){
    for(int i =0;i < strlen(commands) ; i++){
        if(commands[i] =='<'|| commands[i] == '>')
            return true;
    }
    return false;
}
// 处理单命令（无管道的）重定向
//由于带重定向的指令就只有ppt上的四种形式即：<,>,<>,><；<和>之间不存在其他command，除了管道|;
//而管道的处理逻辑是会将多个命令分开为不带管道的单命令执行，所以重定向只需要考虑单条指令即可。
void handle_redirection(char *commands) {
    //printf("处理重定向：Handle_redirection\n");
    int inNum = 0, outNum = 0; // < 和 > 的个数
    pid_t pid;
    //保存原始标准输入和标准输出
    int saved_stdin = dup(STDIN_FILENO);
    int saved_stdout = dup(STDOUT_FILENO);

    char* commands_temp = malloc(sizeof(commands)); //避免设置重定向为'\0'时字符串遍历出错，采用temp保存原始命令行进行遍历
    strcpy(commands_temp, commands);
    
   for(int i =0;i<strlen(commands_temp);i++){

        if(commands_temp[i] == '>'){ //处理输出重定向
            outNum++;
            if(outNum > 1){
                perror("Too many output redirections");
                exit(EXIT_CODE);
            }
            char *outredir_pos = &commands[i];
            //printf("outredir_pos: %d\n", *outredir_pos);
            char *out_file = strtok(outredir_pos + 1," \t"); //输出文件，并且使用strtok去除前面的空格，否则路径不正确
            //printf("out_file: %s\n", out_file);

            int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0666); //写入方式（O_WRONLY），覆盖方式（O_TRUNC），创建方式（O_CREAT），权限设置为 0666。
            if(fd<0){
                perror("Output redirection error");
                exit(EXIT_CODE);
            }
            dup2(fd, STDOUT_FILENO); //复制fd到STDOUT_FILENO（标准输出），即将标准输出写入文件fd。
            close(fd);

            *outredir_pos = '\0'; //设置为字符串结束符号会影响到循环，所以循环用的是temp
        }
        else if(commands_temp[i]=='<'){ //处理输入重定向
            inNum++;
            if(inNum > 1){
                perror("Too many input redirections");
                exit(EXIT_CODE);
            }
            char *inredir_pos = &commands[i];
            //printf("inredir_pos: %d\n", *inredir_pos);
            char *in_file = strtok(inredir_pos + 1," \t"); //输入文件
            //printf("in_file: %s\n", in_file);

            int fd = open(in_file, O_RDONLY); //只读模式打开文件
            if(fd<0){
                perror("Input redirection error");
                exit(EXIT_CODE);
            }
            dup2(fd,STDIN_FILENO);
            close(fd);

            *inredir_pos = '\0';
        }
   }


   //printf("输入重定向数量：%d", inNum);
   //printf("输出重定向数量：%d", outNum);
   //处理最左边的命令
   char *left_command = commands;
   //printf("redir_left_command: %s\n", left_command);

   if(pid=fork() == 0){  
        char *argv_left[ARGC_SIZE]; // 左边命令
        int argc_left = splitCline_getArgcv(left_command, argv_left); // 分割左边命令，存储argv并返回argc。

        /*printf("Left command argc: %d\n", argc_left);
        printf("Left command: ");
        for (int i = 0; i < argc_left; i++) {
            printf("%s ", argv_left[i]); // 打印左边命令
        }
        printf("\n");*/

        handle_basic_commands(argv_left, argc_left); // 处理左边命令
        exit(EXIT_SUCCESS); // 退出子进程
   }
    else if(pid < 0){
        perror("fork");
        exit(EXIT_CODE);
    }
    else{
        wait(NULL); // 等待子进程结束
    } 
    dup2(saved_stdout,STDOUT_FILENO); //重新还原主进程的标准输入和输出，否则终端命令的标准输入输出还是定向在之前的重定向文件上。
    close(saved_stdout);
    dup2(saved_stdin,STDIN_FILENO);
    close(saved_stdin);
}

int main() {
    while (1) {
        print_prompt(); // 打印提示符
        get_command(commandline, sizeof(commandline)); // 获取用户输入

        char *commandline_copy = malloc(sizeof(commandline));
        strcpy(commandline_copy, commandline);

        int argc = splitCline_getArgcv(commandline, argv); // 分割命令，存储agrv并返回argc。
        if (argc == 0) continue;       // 空命令直接跳过该轮循环

        //printf("commandline: %s\n", commandline);

        int pipe_count = count_pipe(commandline_copy); // 管道的个数

        //printf("管道数量为：%d\n", pipe_count);
        if(pipe_count == 1){
            //printf("管道数量为1，进入单管道情况\n");
            handle_single_pipe(commandline_copy); // 单管道情况
        }
        else if(pipe_count > 1){
            //printf("管道数量大于1，进入多管道情况\n");
            handle_multiple_pipe(commandline_copy); // 多管道情况
        }
        else{
            //printf("没有管道；main function enter handle normal commands\n");
            handle_mayRedir_commands(commandline_copy); // 处理内可能带有重定向的命令
        }
    }
    return 0;
}

