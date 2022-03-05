# LzyBot
自己写的基于Mirai框架，使用Mirai.NET库的机器人程序
## 使用方法：
1.到 [release](https://github.com/Liu-Zhiying/LzyBot/releases/tag/release) 下载 Windows x64 或者 Linux x64 包

2.找到MCL文件夹，找到MCL/config/Console/AutoLogin.yml打开，account填你想使用的qq账号，

password下的value填密码保存

3.找到win-x64或者linux-x64下的config.xml，修改WaitTime为超时秒数，修改BotQQ为之前account的qq号，

GroupID为目标群号，可以是多个，用英文逗号隔开，保存

4.桌面模式双击StartBot文件测试启动，第一次登陆可能需要额外的验证

5.确定配置好后，以管理员模式运行StartBot r，这将机器人程序注册为服务

## 以下设置是可选的:
MCL/config/net.mamoe.mirai-api-http/settings.yml

port改端口

verifyKey改Mirai-api-http的密码

对应的，请在win-x64或者linux-x64下的config.xml修改

Address中的端口

VerifyCode中的密码
