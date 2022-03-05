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

std::string get_bin_path_withou_name()
{
    char bin_path[MAX_PATH] = { 0 };
    readlink("/proc/self/exe",bin_path,MAX_PATH);
    std::string result(bin_path);
    result = result.erase(result.find_last_of('/'),result.size());
    return result;
}

void start_service()
{
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
    flock(fd,LOCK_EX);
    lseek(fd,0,SEEK_SET);
    write(fd,&lock_file_value,sizeof lock_file_value);
    sync();
    flock(fd,LOCK_UN);
    while(!lock_file_value)
    {
        if(pid1 < 0 || pid2 < 0 || waitpid(pid1,&stat,WNOHANG) < 0 || waitpid(pid2,&stat,WNOHANG) < 0)
        {
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
        lseek(fd,0,SEEK_SET);
        sync();
        read(fd,&lock_file_value,sizeof lock_file_value);
        flock(fd,LOCK_UN);
    }
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

void register_service()
{
    if(!access(LOCK_FILE_PATH,F_OK))
    {
        std::cout << "Service registered!" << std::endl;
        return;
    }
    FILE *f = fopen(SERVICE_FILE_PATH,"w");
    if(!f)
    {
        std::cout << "register service failed, can not create file." << std::endl;
        return;
    }
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
    std::system("systemctl enable start_lzy_bot");
    std::system("systemctl start start_lzy_bot");
}

void unregister_serivce()
{
    std::system("systemctl stop start_lzy_bot");
    std::system("systemctl disable start_lzy_bot");
    if(remove(SERVICE_FILE_PATH) < 0)
    {
        std::cout << "Unregister service failed!" << std::endl;
        std::cout << "Can not remove file" << std::endl;
        std::cout << "Please run thid command as administrator again." << std::endl;
    }
}

void stop_service()
{
    if(access(LOCK_FILE_PATH,F_OK) < 0)
    {
        std::cout << "Service is not running." << std::endl;
        return;
    }
    int fd = open(LOCK_FILE_PATH,O_CREAT|O_RDWR,0777);
    if(fd < 0)
    {
        std::cout << "Stop service error!" << std::endl;
        return;
    }
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
        std::cout << "Test Starting Mode..." << std::endl;
        std::cout << "$exe r: register as a service" << std::endl;
        std::cout << "$exe u: unregister as a service" << std::endl;
        std::cout << "$exe sta: start service (do not use manually)" << std::endl;
        std::cout << "$exe sto: stop service (do not use manually)" << std::endl;

        pid_t pid1 = fork();
        if(pid1 < 0)
        {
            std::cout << "Fork failed!" << std::endl;
            return 0;
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
            std::cout << "Cmommand Line Wrong..." << std::endl;
        }
    }
    return 0;
}
