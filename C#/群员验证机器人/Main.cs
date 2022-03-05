using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Mirai.Net.Sessions.Http.Managers;
using Mirai.Net.Sessions;
using System.Threading;

MiraiBot bot = new();
Bot.Settings settings = new();
settings.Load();
bot.QQ = settings.BotQQ;
bot.Address = settings.Address;
bot.VerifyKey = settings.VerifyCode;
Bot.Function f = new(10000, settings);
f.Load(bot);
await bot.LaunchAsync();
while (true)
{
    Thread.Sleep(100);
}
