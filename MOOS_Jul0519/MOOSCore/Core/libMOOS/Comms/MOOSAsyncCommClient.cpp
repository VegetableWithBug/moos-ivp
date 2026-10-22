///////////////////////////////////////////////////////////////////////////
//
//   This file is part of the MOOS project
//
//   MOOS : Mission Oriented Operating Suite A suit of 
//   Applications and Libraries for Mobile Robotics Research 
//   Copyright (C) Paul Newman
//    
//   This software was written by Paul Newman at MIT 2001-2002 and 
//   the University of Oxford 2003-2013 
//   
//   email: pnewman@robots.ox.ac.uk. 
//              
//   This source code and the accompanying materials
//   are made available under the terms of the GNU Lesser Public License v2.1
//   which accompanies this distribution, and is available at
//   http://www.gnu.org/licenses/lgpl.txt distributed in the hope that it will be useful, 
//   but WITHOUT ANY WARRANTY; without even the implied warranty of 
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
//
////////////////////////////////////////////////////////////////////////////


/*
 * MOOSAsyncCommClient.cpp
 *
 *  Created on: Sep 18, 2012
 *      Author: pnewman
 */

#include <cmath>
#include <string>
#include <set>
#include <limits>
#include <iostream>
#include <iomanip>
#include <cassert>

#include "MOOS/libMOOS/Utils/MOOSUtils.h"
#include "MOOS/libMOOS/Utils/MOOSException.h"
#include "MOOS/libMOOS/Utils/MOOSScopedLock.h"
#include "MOOS/libMOOS/Utils/ConsoleColours.h"
#include "MOOS/libMOOS/Utils/ThreadPriority.h"
#include "MOOS/libMOOS/Utils/ThreadPrint.h"

#include "MOOS/libMOOS/Comms/MOOSAsyncCommClient.h"
#include "MOOS/libMOOS/Comms/XPCTcpSocket.h"

#ifdef max
#   undef min  // undefine so we can use std::min()
#   undef max  // undefine so we can use std::max()
#endif

namespace MOOS {

ThreadPrint  gMOOSAsyncCommsClientPrinter(std::cerr);

#define TIMING_MESSAGE_PERIOD 1.0


bool AsyncCommsReaderDispatch(void * pParam) {
    MOOSAsyncCommClient *pMe = (MOOSAsyncCommClient*) pParam;
    return pMe->ReadingLoop();
}

bool AsyncCommsWriterDispatch(void * pParam) {
    MOOSAsyncCommClient *pMe = (MOOSAsyncCommClient*) pParam;

    return pMe->WritingLoop();
}

///default constructor
MOOSAsyncCommClient::MOOSAsyncCommClient() {
    m_dfLastTimingMessage = 0.0;
    m_dfOutGoingDelay = 0.0;
    m_bPostNewestToFront = false;

//    SetCommsControlTimeWarpScaleFactor(0.0);
}
///default destructor
MOOSAsyncCommClient::~MOOSAsyncCommClient() {
    Close();
}

std::string MOOSAsyncCommClient::HandShakeKey() {
    return "asynchronous";
}

void MOOSAsyncCommClient::DoBanner() {
    return;
}

bool MOOSAsyncCommClient::StartThreads() {
    m_bQuit = false;

    if (!WritingThread_.Initialise(AsyncCommsWriterDispatch, this))
        return false;

    if (!ReadingThread_.Initialise(AsyncCommsReaderDispatch, this))
        return false;

    WritingThread_.Name(GetMOOSName()+" writing thread");
    ReadingThread_.Name(GetMOOSName()+" reading thread");

    if(!WritingThread_.Start())
        return false;

    if (!ReadingThread_.Start())
        return false;


    return true;
}

bool MOOSAsyncCommClient::Close(bool) {




    BASE::Close();

    if (!ReadingThread_.Stop())
        return false;

    OutGoingQueue_.Push(CMOOSMsg(MOOS_TERMINATE_CONNECTION,"-quit-", 0));

    if (!WritingThread_.Stop())
        return false;

    return true;
}

bool MOOSAsyncCommClient::Flush() {
    return true;
}

bool MOOSAsyncCommClient::Post(CMOOSMsg & Msg, bool bKeepMsgSourceName) {

    if(!BASE::Post(Msg, bKeepMsgSourceName)) // 先调用基类处理消息准备工作；失败时立即返回 false。
        return false;

    m_OutLock.Lock(); // 加锁保护发送队列，避免其他线程同时修改队列。
    {
        if (OutGoingQueue_.Size() > OUTBOX_PENDING_LIMIT) { // 队列超过待发送上限时，进入过载处理逻辑。
            std::cerr << MOOS::ConsoleColours::red() << "WARNING "
                    << MOOS::ConsoleColours::reset()
                    << "MOOSAsyncCommClient::Outbox is very full "
                        "- ditching half of the unsent mail\n";

            while (OutGoingQueue_.Size() > OUTBOX_PENDING_LIMIT / 2) // 持续丢弃队首旧消息，直到队列降至上限的一半。
                OutGoingQueue_.Pop(); // 移除一条未发送消息，防止发送队列无限增长。
        }
        OutGoingQueue_.AppendToMeInConstantTime(m_OutBox); // 将基类准备好的消息批量追加到异步发送队列。
        //std::cerr<<"OutGoingQueue_ : "<<OutGoingQueue_.Size()<<"\n";
    }
    m_OutLock.UnLock(); // 完成队列操作后释放锁。

    return true; // 消息已成功加入发送队列。
}

bool MOOSAsyncCommClient::OnCloseConnection() {
    return BASE::OnCloseConnection();
}

bool MOOSAsyncCommClient::IsAsynchronous() {
    return true;
}





bool MOOSAsyncCommClient::WritingLoop() {
    //we want errors not signals!
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif

    if (m_bBoostIOThreads) {
        MOOS::BoostThisThread();
    }


    while (!WritingThread_.IsQuitRequested())
    {

        //this is the connect loop...
        m_pSocket = new XPCTcpSocket(m_lPort);

        try
        {
            if (m_bDisableNagle){
                if(!m_bQuiet){
                    gMOOSAsyncCommsClientPrinter.Print("disabling nagle");
                }
                m_pSocket->vSetNoDelay(1);
            }
            m_pSocket->vSetRecieveBuf(m_nReceiveBufferSizeKB * 1024);
            m_pSocket->vSetSendBuf(m_nSendBufferSizeKB * 1024);
        }
        catch (XPCException & e)
        {
            std::cerr << "there was trouble configuring socket buffers: "
                    << e.sGetException() << "\n";
        }
        //reset counters
        m_nBytesSent = 0;
        m_nBytesReceived = 0;

        if (ConnectToServer())
        {
            ApplyRecurrentSubscriptions();

            //reset this counter here because a message is sent during handshaking
            m_nMsgsSent = 0;

            int nMSToWait = 333;

            while (!WritingThread_.IsQuitRequested() && IsConnected())
            {
                if (OutGoingQueue_.Size()==0)
                {
                    //this may timeout in which case we DoWriting() which may send
                    //a timing message (heart beat) in Do Writing...
                    OutGoingQueue_.WaitForPush(nMSToWait);
                }

                if (!DoWriting())
                {
                    OnCloseConnection();
                }

            }
        } else
        {
            //this is bad....if ConnectToServer() returns false
            //it wasn't simply that we could not get hold of the server
            //it was that we misbehaved badly. We should quit..
            OnCloseConnection();
            break;
        }

    }
    //clean up on exit....
    if (m_pSocket != NULL)
    {
        if (m_pSocket)
            delete m_pSocket;
        m_pSocket = NULL;
    }

    /*if(!m_bQuiet)
     MOOSTrace("CMOOSAsyncCommClient::WritingLoop() quits\n");*/

    m_bConnected = false;

    return true;
}



bool MOOSAsyncCommClient::DoWriting() {

    //this is the IO Loop
    // 这是一次写入线程的 I/O 处理循环：本次调用负责尽可能发送当前积累的消息。
    try {

        if (!IsConnected()) // 如果当前连接已经失效，通知上层写线程进入断线处理流程。
            return false;

        MOOSMSG_LIST StuffToSend;

        OutGoingQueue_.AppendToOtherInConstantTime(StuffToSend); // 将待发队列中的消息一次性转移到本地列表，减少锁竞争和逐条操作的开销。

        for (MOOSMSG_LIST::iterator q = StuffToSend.begin(); q
                != StuffToSend.end(); ++q)
        {
            if (q->IsType(MOOS_TERMINATE_CONNECTION)) // 终止消息是线程间传递的控制信号，不应被序列化后发送给服务器。
            {
                //std::cerr<<"writing thread receives terminate connection request from sibling reader thread\n";
                return false;
            }
            m_nMsgsSent++; // 统计本批次中的普通待发送消息数量。
        }

        //and once in a while we shall send a timing
        //message (this is the new style of timing
        if ((MOOSLocalTime(false) - m_dfLastTimingMessage) > TIMING_MESSAGE_PERIOD)
        {
            CMOOSMsg Msg(MOOS_TIMING, "_async_timing", 0.0, MOOSLocalTime()); // 构造异步客户端专用的时间消息；消息值为 0，辅助时间字段为当前本地时间。
            StuffToSend.push_front(Msg); // 将心跳放在队首，优先于普通业务消息发送。
            m_dfLastTimingMessage = MOOSLocalTime(false); // 记录本次心跳发送时间，避免每次调用都重复添加心跳。
        }

        if (StuffToSend.empty())
        {
            return true;
        }

        //convert our out box to a single packet
        CMOOSCommPkt PktTx;

        try
        {
            PktTx.Serialize(StuffToSend, true); // 序列化消息列表；true 表示按发送方向构造数据包。
            m_nBytesSent += PktTx.GetStreamLength(); // 用序列化后的数据包长度更新已发送字节数统计。
        }
        catch (const CMOOSException & e) {
            //clear the outbox
            MOOS::DeliberatelyNotUsed(e);
            throw CMOOSException(
                                 "Serialisation Failed - this must be a lot of mail...");
        }

        //finally the send....
        SendPkt(m_pSocket, PktTx); // 通过当前 socket 将完整数据包发送到 MOOSDB。

        MonitorAndLimitWriteSpeed();

    } catch (const CMOOSException & e) {
        MOOSTrace("Exception in DoWriting() : %s\n", e.m_sReason);
        return false;//jump out to connect loop....
    }

    return true;

}



bool MOOSAsyncCommClient::MonitorAndLimitWriteSpeed()
{
    double WarpDelayMS = 0.0;
    //m_dfOutGoingDelay is set by DB during comms
    //we also Optionally add a delay for time warps
    double TimeWarp =GetMOOSTimeWarp();
    if(TimeWarp>1.0)
    {
        WarpDelayMS=TimeWarp*GetCommsControlTimeWarpScaleFactor();
    }
    double TotalDelay = std::max(m_dfOutGoingDelay*1000,WarpDelayMS);
    unsigned int sleep_ms =
            static_cast<unsigned int> (TotalDelay);
    if (sleep_ms > 0)
    {
      //std::cerr << "I'm sleeping for " << TotalDelay << " ms ("<<TotalDelay/GetMOOSTimeWarp()<<" real ms)\n";
        MOOSPause(sleep_ms,false);
    }

    return true;
}

bool MOOSAsyncCommClient::ReadingLoop() {
    //note we will rely on our sibling writing thread to handle
    //the connected and reconnecting...
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif	

    if (m_bBoostIOThreads) {
        MOOS::BoostThisThread();
    }

    while (!ReadingThread_.IsQuitRequested())
    {
        if (IsConnected())
        {
            if (!DoReading())
            {
                OutGoingQueue_.Push(
                                    CMOOSMsg(MOOS_TERMINATE_CONNECTION,
                                             "-quit-", 0));

                //std::cout<<"reading failed!\n";

                while (IsConnected())//wait for connection to terminate...
                    MOOSPause(200);
            }

        }
        else
        {
            //we arent connected so do nothing...
            MOOSPause(100);
        }
    }
    //std::cerr<<"READING LOOP quiting...\n";
    return true;
}


bool MOOSAsyncCommClient::IsRunning() {
    return WritingThread_.IsThreadRunning() || ReadingThread_.IsThreadRunning();
}

bool MOOSAsyncCommClient::DoReading()
{

	try
	{
		CMOOSCommPkt PktRx; // 创建接收数据包对象，保存从 MOOSDB 读取的数据。

		ReadPkt(m_pSocket,PktRx); // 从当前 TCP socket 阻塞读取一个完整的数据包。

		m_nPktsReceived++; // 更新已接收的数据包数量。

		m_nBytesReceived+=PktRx.GetStreamLength(); // 累加本次接收的数据字节数。


		double dfLocalRxTime =MOOSLocalTime(); // 记录本地收到数据包的时间，用于时钟偏差计算。

		m_InLock.Lock(); // 加锁保护收件箱，避免与 Fetch() 等操作并发访问 m_InBox。
		{
			if(m_InBox.size()>m_nInPendingLimit) // 检查未读取消息是否超过收件箱容量上限。
			{
				MOOSTrace("Too many unread incoming messages [%d] : purging\n",m_InBox.size()); // 输出收件箱溢出警告。
				MOOSTrace("The user must read mail occasionally"); // 提醒用户及时读取邮件。
				m_InBox.clear(); // 丢弃当前积压的未读取消息，防止内存持续增长。
			}

            //how big was inbox before we add new mail?
            unsigned int nur = m_InBox.size(); // 记录添加当前数据包前的收件箱大小。

            //extract... and please leave NULL messages there
            PktRx.Serialize(m_InBox,false,false,NULL); // 将数据包反序列化，并把消息追加到收件箱。

			m_nMsgsReceived+=m_InBox.size()-nur; // 根据收件箱大小变化统计本次新增的消息数。


            //now Serialize simply adds to the front of a list so looking
            //at the first element allows us to check for timing information
            //as supported by the threaded server class
            MOOSMSG_LIST::iterator q = m_InBox.begin(); // 从收件箱头部开始定位本次数据包的第一条消息。
            std::advance(q,nur); // 跳过此前已有的 nur 条消息。

            switch(q->GetType()) // 根据第一条新消息类型处理内部时间同步消息。
            {
                case MOOS_TIMING:
                {
                    //timing messages don't count in statisics
                    m_nMsgsReceived--; // 时间同步消息不属于用户业务消息，修正接收消息统计。

                    //we have a fancy new DB upstream...
                    //one that supports Asynchronous Clients
                    if(m_bDoLocalTimeCorrection && GetNumPktsReceived()>1) // 启用时间校正且已有足够样本时更新时钟偏差。
                    {

                        UpdateMOOSSkew(q->GetTime(), // 使用消息时间作为客户端侧时间参考。
                                q->GetDouble(), // 使用消息数值作为服务器侧时间。
                                dfLocalRxTime); // 使用本地接收时间完成偏差估计。
                    }

                    if(m_bDBIsAsynchronous) // 异步 MOOSDB 可能通过该消息调整发送线程的速度。
                    {
                        //and we can update the outgoing thread's speed
                        //as controlled by the DB.
                        m_dfOutGoingDelay = q->GetDoubleAux(); // 保存服务器下发的发送延迟。

                    }

                    m_InBox.erase(q); // 内部时间消息已处理，从用户收件箱中删除。

                    break;
                }
                case MOOS_NULL_MSG:
                {
                    //looks like we have an old fashioned DB which sends timing
                    //info at the front of every packet in a null message
                    //we have no corresponding outgoing packet so not much we can
                    //do other than imagine it tooks as long to send to the
                    //DB as to receive...
                    double dfTimeSentFromDB = m_InBox.front().GetDouble(); // 读取旧版 MOOSDB 放在空消息中的服务器发送时间。
                    double dfSkew = dfTimeSentFromDB-dfLocalRxTime; // 估计服务器时间与本地接收时间之间的偏差。
                    double dfTimeSentToDBApprox =dfTimeSentFromDB+dfSkew; // 近似推算客户端发送时间。

                    m_InBox.pop_front(); // 删除仅用于时间同步的空消息，不交给用户处理。

                    if(m_bDoLocalTimeCorrection) // 如果启用了本地时间校正，则更新时钟偏差。
                    {
                        UpdateMOOSSkew(dfTimeSentToDBApprox, // 估计的客户端发送时间。
                                dfTimeSentFromDB, // 服务器发送时间。
                                dfLocalRxTime); // 客户端本地接收时间。
                    }

                    break;

                }
            }

            DispatchInBoxToActiveThreads(); // 将匹配 active queue 的业务消息分发给对应队列。

            m_bMailPresent = !m_InBox.empty(); // 根据收件箱是否仍有用户消息更新标志。


		}
        m_InLock.UnLock(); // 释放收件箱锁，允许其他线程继续访问消息。

        //and here we can optionally give users an indication
        //that mail has arrived...
        if(m_pfnMailCallBack!=NULL && m_bMailPresent) // 有用户回调且存在邮件时通知用户。
		{
            bool bUserResult = (*m_pfnMailCallBack)(m_pMailCallBackParam); // 通过函数指针调用用户注册的邮件回调。
			if(!bUserResult)
                MOOSTrace("user mail callback returned false..is all ok?\n"); // 记录用户回调返回 false 的情况。
		}
	}
	catch(const CMOOSException & e)
	{
        MOOS::DeliberatelyNotUsed(e); // 明确忽略异常对象本身，避免未使用参数警告。
        MOOSTrace("Exception in DoReading() : %s\n",e.m_sReason); // 输出读取或解析异常信息。
        return false; // 通知 ReadingLoop() 本次读取失败，需要关闭当前连接。
	}

    return true; // 本次读取、解析和分发全部成功。
}


}



