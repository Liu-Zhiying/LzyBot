#include <iostream>
#include <unistd.h>
#include <cstring>
#include <string>
#include <cstdlib>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <stdio.h>
#include <time.h>

#define MAX_PATH 260

#define MCL_WORKING_DIR "/MCL"
#define JAVA_RUNNER_PATH "/JDK17X64/bin/java"
#define BOT_WORKING_DIR "/linux-x64"
#define BOT_PATH "/linux-x64/LzyBot"
#define JAVA_RUNNER_ARGU1 "-jar"
#define JAVA_RUNNER_ARGU2 "./mcl.jar"
#define LOCK_FILE_PATH "/tmp/start_lzy_bot.st"
#define SERVICE_FILE_PATH "/etc/systemd/system/start_lzy_bot.service"

//获取程序的绝对路径（不包括程序名）
std::string get_bin_path_withou_name()
{
    char bin_path[MAX_PATH] = { 0 };
    readlink("/proc/self/exe",bin_path,MAX_PATH);
    std::string result(bin_path);
    result = result.erase(result.find_last_of('/'),result.size());
    return result;
}
//启动服务
void start_service()
{
    //创建进程之间同步的文件，存在则退出
    int fd = open(LOCK_FILE_PATH,O_CREAT | O_EXCL | O_RDWR,0777);
    int lock_file_value = 0;
    pid_t pid1 = -1,pid2 = -1;
    int stat = 0;
    if(fd < 0)
    {
        std::cout << "Lock file exists." << std::endl;
        std::cout << "If you are sure that no service instance of this program is running." << std::endl;
        std::cout << "Delete " <<  LOCK_FILE_PATH << std::endl;
        return;
    }
    //建议性写入锁加锁，写入初始值
    flock(fd,LOCK_EX);
    lseek(fd,0,SEEK_SET);
    write(fd,&lock_file_value,sizeof lock_file_value);
    sync();
    flock(fd,LOCK_UN);
    while(!lock_file_value)
    {
        //判断机器人进程和java进程是否正常运行，如果有不正常，杀死已有进程，然后重新开始
        if(pid1 < 0 || pid2 < 0 || waitpid(pid1,&stat,WNOHANG) < 0 || waitpid(pid2,&stat,WNOHANG) < 0)
        {
            //杀死还在运行的进程
            if(pid1 >= 0)
            {
                kill(pid1,SIGKILL);
                //这步是为了给子进程收尸(下同)
                waitpid(pid1,&stat,WNOHANG);
            }
            if(pid2 >= 0)
            {
                kill(pid2,SIGKILL);
                
                waitpid(pid2,&stat,WNOHANG);
            }
            //创建新进程（和main的测试启动没啥区别）
            pid1 = fork();
            if(pid1 < 0)
            {
                continue;
            }
            if(!pid1)
            {
                std::string working_dir = get_bin_path_withou_name()+ MCL_WORKING_DIR;
                std::string java_bin_path = get_bin_path_withou_name() + JAVA_RUNNER_PATH;
                char buffer1[MAX_PATH];
                memcpy(buffer1,java_bin_path.c_str(),java_bin_path.size() + 1);
                char buffer2[MAX_PATH];
                memcpy(buffer2,JAVA_RUNNER_ARGU1,strlen(JAVA_RUNNER_ARGU1) + 1);
                char buffer3[MAX_PATH];
                memcpy(buffer3,JAVA_RUNNER_ARGU2,strlen(JAVA_RUNNER_ARGU2) + 1);
                char * temp[4] = {buffer1,buffer2,buffer3,NULL};
                if(chdir(working_dir.c_str()) < 0)
                {
                    continue;
                }
                if(execv(java_bin_path.c_str(),temp) < 0)
                {
                    continue;
                }
            }
            sleep(10);
            pid2 = fork();
            if(pid2 < 0)
            {
                continue;
            }
            if(!pid2)
            {
                std::string working_dir = get_bin_path_withou_name() + BOT_WORKING_DIR;
                std::string bot_bin_path = get_bin_path_withou_name() + BOT_PATH;
                if(chdir(working_dir.c_str()) < 0)
                {
                    continue;
                }
                char buffer[MAX_PATH];\
                memcpy(buffer,bot_bin_path.c_str(),bot_bin_path.size() + 1);
                char *temp[2] = {buffer,NULL};
                if(execv(bot_bin_path.c_str(),temp) < 0)
                {
                    continue;
                }
            }
        }
        if(flock(fd,LOCK_SH | LOCK_NB))
        {
            continue;
        }
        //读取文件头部的int值
        lseek(fd,0,SEEK_SET);
        sync();
        read(fd,&lock_file_value,sizeof lock_file_value);
        flock(fd,LOCK_UN);
    }
    //服务停止代码
    //清理子进程（见上）
    if(pid1 >= 0)
    {
        kill(pid1,SIGKILL);
        waitpid(pid1,&stat,WNOHANG);
    }
    if(pid2 >= 0)
    {
        kill(pid2,SIGKILL);
        waitpid(pid2,&stat,WNOHANG);
    }
    //向文件写入新值，代表清理完成
    time_t wait_start_time = time(NULL);
    while(flock(fd,LOCK_EX|LOCK_NB) < 0 && time(NULL) - wait_start_time < 5);
    if(time(NULL) - wait_start_time >= 5)
    {
        flock(fd,LOCK_UN);
        std::cout << "Can not report service stoped!" << std::endl;
        return;
    }
    lock_file_value = 2;
    lseek(fd,0,SEEK_SET);
    write(fd,&lock_file_value,sizeof lock_file_value);
    sync();
    flock(fd,LOCK_UN);
    close(fd);
}
//注册服务
void register_service()
{
    //确定service文件是否存在，存在则返回
    if(!access(LOCK_FILE_PATH,F_OK))
    {
        std::cout << "Service registered!" << std::endl;
        return;
    }
    //打开新service文件
    FILE *f = fopen(SERVICE_FILE_PATH,"w");
    if(!f)
    {
        std::cout << "register service failed, can not create file." << std::endl;
        return;
    }
    //写入service文件内容（部分内容需要字符串拼接）
    fprintf(f,"[Unit]\nDescription=LzyBotService\n");
    fprintf(f,"[Service]\nType=simple\n");

    char buffer[MAX_PATH] = { 0 };
    readlink("/proc/self/exe",buffer,MAX_PATH);
    std::string cmd_line = buffer;
    cmd_line += " sta";
    fprintf(f,"ExecStart=%s\n",cmd_line.c_str());
    cmd_line.clear();
    cmd_line = buffer;
    cmd_line += " sto";
    fprintf(f,"ExecStop=%s\n",cmd_line.c_str());
    fprintf(f,"[Install]\nWantedBy=multi-user.target\n");
    fclose(f);
    //调用systemctl设置服务为自启动
    std::system("systemctl enable start_lzy_bot");
    //立即启动服务
    std::system("systemctl start start_lzy_bot");
}
//取消注册服务
void unregister_serivce()
{
    //立即关闭服务
    std::system("systemctl stop start_lzy_bot");
    //取消服务自启动
    std::system("systemctl disable start_lzy_bot");
    //删除service文件
    if(remove(SERVICE_FILE_PATH) < 0)
    {
        std::cout << "Unregister service failed!" << std::endl;
        std::cout << "Can not remove file" << std::endl;
        std::cout << "Please run thid command as administrator again." << std::endl;
    }
}
//停止服务
void stop_service()
{
    //确定同步文件存在
    if(access(LOCK_FILE_PATH,F_OK) < 0)
    {
        std::cout << "Service is not running." << std::endl;
        return;
    }
    //打开同步文件
    int fd = open(LOCK_FILE_PATH,O_CREAT|O_RDWR,0777);
    if(fd < 0)
    {
        std::cout << "Stop service error!" << std::endl;
        return;
    }
    //写入值，通知服务进程停止
    time_t wait_start_time = time(NULL);
    while(flock(fd,LOCK_EX|LOCK_NB) < 0 && time(NULL) - wait_start_time);
    if(time(NULL) - wait_start_time >= 15)
    {
        flock(fd,LOCK_UN);
        std::cout << "Stop service timeout!" << std::endl;
        return;
    }
    int lock_file_value = 1;
    write(fd,&lock_file_value,sizeof lock_file_value);
    sync();
    flock(fd,LOCK_UN);
    //等待服务进程通知清理完毕
    do
    {
        if(flock(fd,LOCK_SH|LOCK_NB) < 0)
        {
            continue;
        }
        lseek(fd,0,SEEK_SET);
        sync();
        read(fd,&lock_file_value,sizeof lock_file_value);
        flock(fd,LOCK_UN);
    }
    while(lock_file_value < 2 && time(NULL) - wait_start_time < 15);
    close(fd);
    //删除同步文件
    remove(LOCK_FILE_PATH);
    if(lock_file_value < 2)
    {
        std::cout << "Service may not start!" << std::endl;
    }
}

int main(int argc,char **argv)
{
    if(argc < 2)
    {
        //测试启动模式
        std::cout << "Test Starting Mode..." << std::endl;
        std::cout << "$exe r: register as a service" << std::endl;
        std::cout << "$exe u: unregister as a service" << std::endl;
        std::cout << "$exe sta: start service (do not use manually)" << std::endl;
        std::cout << "$exe sto: stop service (do not use manually)" << std::endl;
        //复制进程（linux启动新进程需要先复制进程）（下同）
        pid_t pid1 = fork();
        //如果复制失败（下同）
        if(pid1 < 0)
        {
            std::cout << "Fork failed!" << std::endl;
            return 0;
        }
        //复制后子进程执行代码
        //进行字符串拼接出工作目录和目标模块的路径
        //然后进行镜像替换（下同）
        if(!pid1)
        {
            std::string working_dir = get_bin_path_withou_name()+ MCL_WORKING_DIR;
            std::string java_bin_path = get_bin_path_withou_name() + JAVA_RUNNER_PATH;
            char buffer1[MAX_PATH];
            memcpy(buffer1,java_bin_path.c_str(),java_bin_path.size() + 1);
            char buffer2[MAX_PATH];
            memcpy(buffer2,JAVA_RUNNER_ARGU1,strlen(JAVA_RUNNER_ARGU1) + 1);
            char buffer3[MAX_PATH];
            memcpy(buffer3,JAVA_RUNNER_ARGU2,strlen(JAVA_RUNNER_ARGU2) + 1);
            char * temp[4] = {buffer1,buffer2,buffer3,NULL};
            if(chdir(working_dir.c_str()) < 0)
            {
                std::cout << "Switching working directory failed!" << std::endl;
                return 0;
            }
            if(execv(java_bin_path.c_str(),temp) < 0)
            {
                std::cout << "Replacing execute iamge failed2!" << std::endl;
                return 0;
            }
        }
        sleep(10);
        pid_t pid2 = fork();
        if(pid2 < 0)
        {
            std::cout << "Fork failed!" << std::endl;
            return 0;
        }
        if(!pid2)
        {
            std::string working_dir = get_bin_path_withou_name() + BOT_WORKING_DIR;
            std::string bot_bin_path = get_bin_path_withou_name() + BOT_PATH;

            if(chdir(working_dir.c_str()) < 0)
            {
                kill(pid1,SIGKILL);
                std::cout << "Switching working directory failed!" << std::endl;
                return 0;
            }
            char buffer[MAX_PATH];\
            memcpy(buffer,bot_bin_path.c_str(),bot_bin_path.size() + 1);
            char *temp[2] = {buffer,NULL};
            if(execv(bot_bin_path.c_str(),temp) < 0)
            {
                kill(pid1,SIGKILL);
                std::cout << "Replacing execute iamge failed1!" << std::endl;
                return 0;
            }
        }
    }
    else
    {
        if(!strcmp(argv[1],"r"))
        {
            register_service();
        }
        else if(!strcmp(argv[1],"u"))
        {
            unregister_serivce();
        }
        else if(!strcmp(argv[1],"sta"))
        {
            start_service();
        }
        else if(!strcmp(argv[1],"sto"))
        {
            stop_service();
        }
        else
        {
            //提示用户命令输入错误
            std::cout << "Cmommand Line Wrong..." << std::endl;
        }
    }
    return 0;
}
