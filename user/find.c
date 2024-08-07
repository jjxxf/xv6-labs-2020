#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

// 定义 find 函数，用于在指定路径下查找指定名称的文件
void find(char *path, char *name)
{
  char file_name_buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

 // 打开指定路径的文件或目录
  if((fd = open(path, 0)) < 0){
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

// 获取文件或目录的状态信息
  if(fstat(fd, &st) < 0){
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

// 根据文件类型进行处理
  switch(st.type){
  case T_FILE: // 第一个参数是文件，不符合要求
    printf("usage: find <directory> <file>\n");
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof file_name_buf){
      printf("find: path too long\n");
      break;
    }
    strcpy(file_name_buf, path); // 复制路径到缓冲区
    p = file_name_buf+strlen(file_name_buf);
    *p++ = '/'; // 在路径末尾添加斜杠
    while(read(fd, &de, sizeof(de)) == sizeof(de)){  // 指针 p 指向缓冲区末尾
      if(de.inum == 0) 
        continue;     
      memmove(p, de.name, DIRSIZ);  // 将目录项名称复制到缓冲区
      p[DIRSIZ] = 0; // 添加字符串结束符
      if(stat(file_name_buf, &st) < 0){
        printf("find: cannot stat %s\n", file_name_buf);
        continue;
      }

      // 如果目录项名称与指定名称匹配，输出完整路径
      if(strcmp(de.name, name) == 0){
        printf("%s\n", file_name_buf);
      }

      // 如果目录项是目录，递归查找
      if(st.type == T_DIR && de.name[0] != '.'){
        find(file_name_buf, name);
      }
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[])
{

  // 参数检查
  if(argc <= 2){
    printf("usage: find <directory> <file>\n");
    exit(0);
  }

  find(argv[1], argv[2]);
  exit(0);
}
