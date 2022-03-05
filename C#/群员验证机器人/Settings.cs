using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.IO;
using System.Xml;
using Mirai.Net.Sessions.Http.Managers;
using Mirai.Net.Data.Shared;

namespace Bot
{
    public class Settings
    {
        //保存监视的群信息
        public List<string> QQGroupIDs { get; private set; }
        public TimeSpan Time { get; private set; }
        public string BotQQ { get; private set; }
        public string Address { get; private set; }
        public string VerifyCode { get; private set; }
        //初始化
        public Settings()
        {
            QQGroupIDs = new();
            Time = TimeSpan.FromSeconds(300);
            BotQQ = "";
            Address = "";
            VerifyCode = "";
        }
        /// <summary>
        /// 载入设置（查看要监视的群）
        /// </summary>
        public void Load()
        {
            XmlReader xmlReader = XmlReader.Create("config.xml");
            while(xmlReader.Read())
            {
                if(xmlReader.NodeType == XmlNodeType.Element)
                {
                    switch(xmlReader.Name)
                    {
                        //遇上根元素不处理
                        case "BotSettings":
                            break;
                        //机器人的QQ号
                        case "BotQQ":
                            BotQQ = xmlReader.ReadString();
                            break;
                        //核心服务器地址
                        case "Address":
                            Address = xmlReader.ReadString();
                            break;
                        //核心服务器校验码
                        case "VerifyCode":
                            VerifyCode = xmlReader.ReadString();
                            break;
                        //处理设定的群号
                        case "GroupIDs":
                            string[] ids = xmlReader.ReadString().Split(',');
                            foreach(string id in ids)
                            {
                                if(!QQGroupIDs.Any(addedId => addedId == id ))
                                {
                                    QQGroupIDs.Add(id);
                                }
                            }
                            break;
                        //处理等待时间，按秒计算
                        case "WaitTime":
                            Time = TimeSpan.FromSeconds(
                                Convert.ToInt32(
                                    xmlReader.ReadString()
                                )
                            );
                            break;
                        //出现未知元素，抛出异常
                        default:
                            throw new ArgumentException();
                    }
                }
            }
            xmlReader.Dispose();
        }
    }
}
