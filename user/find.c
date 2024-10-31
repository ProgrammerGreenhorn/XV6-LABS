#include "../kernel/types.h"
#include "../kernel/stat.h"
#include "../user/user.h"
#include "../kernel/fs.h"

void find(char *base_dir, char *to_find_file)
{
    int fd;
    struct stat st;
    struct dirent de;
    
    // 打开文件
    if ((fd = open(base_dir, 0)) < 0)
    {
        fprintf(2, "find: cannot access path %s\n", base_dir);
        return;
    }
    // fstat通过fd获取文件信息
    if ((fstat(fd, &st) < 0))
    {
        fprintf(2, "find: cannot stat path %s", base_dir);
        return;
    }
    // 不是目录
    if(st.type != T_DIR){
        fprintf(2,"find: %s it not a dir\n",base_dir);
    }
    // buf存储完整路径 eg: ./b ./a/b 以及用于递归
    char buf[512], *buf_ptr;
    if (strlen(base_dir) + 1 + DIRSIZ + 1 > sizeof(buf))
    {
        printf("find: path too long\n");
        return;
    }
    strcpy(buf, base_dir);
    buf_ptr = buf + strlen(buf);
    // buf ptr 指向当前 buf 字符串的尾端,添加 /
    *buf_ptr = '/';
    ++buf_ptr;
    //遍历目录中的所有目录项
    while (read(fd, &de, sizeof(de)) == sizeof(de))
    {
        if (de.inum == 0){
            continue;
        }
            
        //避免递归进入 . 和 ..
        if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
        {
            continue;
        }

        // 拼接当前文件名到buf
        memcpy(buf_ptr, de.name, DIRSIZ);
        // 添加空字符
        *(buf_ptr + DIRSIZ) = 0;

        // 当前文件是文件，匹配名字
        // 当前文件是文件夹，递归查找
        if (stat(buf, &st) < 0)
        {
            printf("find: connot stat file %s\n", buf);
            continue;
        }

        if(st.type == T_FILE){
            if(strcmp(de.name,to_find_file) == 0){
                printf("%s\n", buf);
            }
        }else if(st.type == T_DIR){
            find(buf,to_find_file);
        }

    }
    return;
}

int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        fprintf(2, "Usage: find dir filename\n");
        exit(0);
    }

    find(argv[1], argv[2]);

    exit(0);
}