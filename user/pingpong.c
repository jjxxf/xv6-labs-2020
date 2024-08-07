#include "kernel/types.h"
#include "user/user.h"

int main(int arc, char** argv){
    char buf = 'a' ;
    int p_to_c[2]; // 父->子
    int c_to_p[2]; // 子->父
    pipe(p_to_c);
    pipe(c_to_p);

    int pid = fork();
    int exit_status = 0;

    if(pid < 0){ // 创建子进程失败
        printf("fork() error!\n");
        close(c_to_p[0]);
        close(c_to_p[1]);
        close(p_to_c[0]);
        close(p_to_c[1]);
        exit(1);
    }else if(pid == 0){ // 子进程
        close(p_to_c[1]);// 关闭写端
        close(c_to_p[0]);// 关闭读端

        // 子进程先读再写

        // 从管道读数据
        if (read(p_to_c[0], &buf, sizeof(char)) != sizeof(char)) {
            printf("child read() error!\n");
            exit_status = 1; //标记出错
        } else {
            printf("%d: received ping\n", getpid());
        }

        // 向管道写数据
        if (write(c_to_p[1], &buf, sizeof(char)) != sizeof(char)) {
            printf("child write() error!\n");
            exit_status = 1;
        }

        close(p_to_c[0]);
        close(c_to_p[1]);

        exit(exit_status);
    }else{
        close(c_to_p[1]);// 关闭写端
        close(p_to_c[0]);// 关闭读端

        // 父进程先写再读

        if(write(p_to_c[1], &buf, sizeof(char)) != sizeof(char)) {
            printf("parent write() error!\n");
            exit_status = 1;
        }

        if(read(c_to_p[0], &buf, sizeof(char)) != sizeof(char)) {
            printf("parent read() error!\n");
            exit_status = 1; //标记出错
        } else {
            printf("%d: received pong\n", getpid());
        }

        close(p_to_c[1]);
        close(c_to_p[0]);
        
        exit(exit_status);
    }
}