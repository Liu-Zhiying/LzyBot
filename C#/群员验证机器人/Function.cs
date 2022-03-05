using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using Mirai.Net.Sessions.Http.Managers;
using Mirai.Net.Sessions;
using Mirai.Net.Data.Events;
using Mirai.Net.Data.Events.Concretes.Group;
using Mirai.Net.Data.Messages.Receivers;
using Mirai.Net.Data.Messages.Concretes;
using Mirai.Net.Data.Messages;
using System.Reactive.Linq;
using Mirai.Net.Data.Shared;
using Mirai.Net.Utils.Scaffolds;
using System.Threading;

namespace Bot
{
    internal class NewMemberWaitObj
    {
        //记录群员信息
        public Member Member { get; private set; }
        //记录加入时间
        public DateTime DateTime { get; private set; }
        //记录验证码
        public string VarifyCode { get; private set; }
        //生成对象，时间使用当前时间，验证码随机生成
        public NewMemberWaitObj(Member member)
        {
            Member = member;
            DateTime = DateTime.Now;
            VarifyCode = new Random().Next(1000, 9999).ToString();
        }
    }
    internal class Function
    {
        //设置信息
        private Settings settings;
        //记录待验证成员的信息
        private List<NewMemberWaitObj> newMembers;
        //踢人任务的计时器
        private System.Timers.Timer timer;
        //读写锁，用于保护newMember
        private ReaderWriterLock locker;
        //读写锁超时
        private int lockTimeout;
        //用于防止线程重入的对象
        private object o;
        //初始化
        public Function(int lockTimeout, Settings settings)
        {
            o = new();
            this.settings = settings;
            foreach (string id in settings.QQGroupIDs)
            {
                Console.WriteLine("监视的群:" + id);
            }
            newMembers = new();
            locker = new();
            this.lockTimeout = lockTimeout;
            timer = new();
            timer.Interval = 100;
            timer.Elapsed += Kick;
            timer.Start();
        }
        //释放资源
        ~Function()
        {
            if (timer != null)
            {
                timer.Dispose();
            }
        }
        //注册事件监听器
        public void Load(MiraiBot bot)
        {
            //成员加入监听，符合条件加入待验证人员并提示
            bot.EventReceived.OfType<MemberJoinedEvent>().Where(
                e =>
                {
                    foreach (string id in settings.QQGroupIDs)
                    {
                        if (e.Member.Group.Id == id)
                        {
                            return true;
                        }
                    }
                    return false;
                }
            ).Subscribe(
                e =>
                {
                    locker.AcquireWriterLock(lockTimeout);
                    Console.WriteLine(e.Member.Name + "加入了群" + e.Member.Group.Name);
                    NewMemberWaitObj newMemberWaitObj = new(e.Member);
                    newMembers.Add(newMemberWaitObj);
                    MessageManager.SendGroupMessageAsync(e.Member.Group.Id, new MessageChainBuilder()
                        .At(newMemberWaitObj.Member.Id)
                        .Plain($"你加入了{newMemberWaitObj.Member.Group.Name}，" +
                            $"请于{settings.Time.TotalMinutes}分钟内回复{newMemberWaitObj.VarifyCode}，否则将被踢出群聊!")
                        .Build()
                    );
                    locker.ReleaseWriterLock();
                }
            );
            //消息监听，符合条件从待验证人员中删除（不踢人）并提示
            bot.MessageReceived.OfType<GroupMessageReceiver>().Where(
                m =>
                {
                    foreach (string id in settings.QQGroupIDs)
                    {
                        if (m.Id == id)
                        {
                            return true;
                        }
                    }
                    return false;
                }
            ).Subscribe(
                m =>
                {
                    locker.AcquireReaderLock(lockTimeout);
                    NewMemberWaitObj? obj = newMembers.Find(
                        n =>
                        {
                            if (n.Member.Id == m.Sender.Id
                                && n.Member.Group.Id == m.Sender.Group.Id)
                                return true;
                            else
                                return false;
                        }
                    );
                    if (obj != null
                        && m.MessageChain.GetPlainMessage() == obj.VarifyCode)
                    {
                        Console.WriteLine(m.Sender.Name + "通过了验证！");
                        LockCookie lockCookie = locker.UpgradeToWriterLock(lockTimeout);
                        newMembers.Remove(obj);
                        locker.DowngradeFromWriterLock(ref lockCookie);
                        MessageManager.SendGroupMessageAsync(obj.Member.Group.Id,
                            new MessageChainBuilder().At(obj.Member.Id).Plain("你已经通过验证!")
                            .Build()
                        );
                    }
                    locker.ReleaseReaderLock();
                }
            );
        }
        void Kick(object? sender, System.Timers.ElapsedEventArgs e)
        {
            if(!Monitor.TryEnter(o))
            {
                return;
            }
            List<NewMemberWaitObj> removedObjs = new();
            locker.AcquireReaderLock(lockTimeout);
            foreach (NewMemberWaitObj obj in newMembers)
            {
                if (DateTime.Now - obj.DateTime >= settings.Time)
                {
                    GroupManager.KickAsync(obj.Member);
                    Console.WriteLine(obj.Member.Name + "被踢出群" + obj.Member.Group.Name);
                    removedObjs.Add(obj);
                }
            }
            LockCookie lockCookie = locker.UpgradeToWriterLock(lockTimeout);
            foreach (NewMemberWaitObj obj in removedObjs)
            {
                newMembers.Remove(obj);
            }
            removedObjs.Clear();
            locker.DowngradeFromWriterLock(ref lockCookie);
            locker.ReleaseReaderLock();
            Monitor.Exit(o);
        }
    }
}
