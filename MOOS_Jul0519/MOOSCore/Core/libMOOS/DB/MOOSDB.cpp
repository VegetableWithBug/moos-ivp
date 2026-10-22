/**
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
//   are made available under the terms of the GNU Public License
//   which accompanies this distribution, and is available at
//   http://www.gnu.org/licenses/gpl.txt
//          
//   This program is distributed in the hope that it will be useful, 
//   but WITHOUT ANY WARRANTY; without even the implied warranty of 
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
//
////////////////////////////////////////////////////////////////////////////
**/


// MOOSDB.cpp: implementation of the CMOOSDB class.
//
//////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#pragma warning(disable : 4786)
#endif


#include "MOOS/libMOOS/MOOSLib.h"
#include "MOOS/libMOOS/Thirdparty/getpot/GetPot.hpp"
#include "MOOS/libMOOS/Utils/ConsoleColours.h"
#include "MOOS/libMOOS/MOOSVersion.h"
#include "MOOS/libMOOS/GitVersion.h"
#include "MOOS/libMOOS/DB/MOOSDBLogger.h"
#include "MOOS/libMOOS/Utils/MOOSScopedPtr.h"



#include "MOOS/libMOOS/DB/MOOSDB.h"
#include "assert.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include <vector>
#include <iterator>
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////



bool CMOOSDB::OnRxPktCallBack(const std::string & sWho,MOOSMSG_LIST & MsgListRx,MOOSMSG_LIST & MsgListTx, void * pParam)
{
    CMOOSDB* pMe = (CMOOSDB*)(pParam);
    
    return pMe->OnRxPkt(sWho,MsgListRx,MsgListTx);
}

bool CMOOSDB::OnFetchAllMailCallBack(const std::string & sWho,MOOSMSG_LIST & MsgListTx, void * pParam)
{
    CMOOSDB* pMe = (CMOOSDB*)(pParam);

    return pMe->OnFetchAllMail(sWho,MsgListTx);
}

bool CMOOSDB::OnDisconnectCallBack(string & sClient, void * pParam)
{
    CMOOSDB* pMe = (CMOOSDB*)(pParam);
    
    return pMe->OnDisconnect(sClient);
}

bool CMOOSDB::OnConnectCallBack(string & sClient, void * pParam)
{
    CMOOSDB* pMe = (CMOOSDB*)(pParam);

    return pMe->OnConnect(sClient);
}


CMOOSDB::CMOOSDB()
{
    //here we set up default community names and DB Names
    m_sDBName = "MOOSDB#1";
    m_sCommunityName = "#1";
    m_dfSummaryTime = MOOS::Time();
    
    //her is the default port to listen on
    m_nPort = DEFAULT_MOOS_SERVER_PORT;
    
    m_bQuiet = false;

    //make our own variable called DB_TIME
    {
        CMOOSDBVar NewVar("DB_TIME");
        NewVar.m_cDataType = MOOS_DOUBLE;
        NewVar.m_dfVal= HPMOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = HPMOOSTime();
        m_VarMap["DB_TIME"] = NewVar;
    }
    
    //make our own variable called DB_TIME
    {
        CMOOSDBVar NewVar("DB_UPTIME");
        NewVar.m_cDataType = MOOS_DOUBLE;
        NewVar.m_dfVal= 0;
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = HPMOOSTime();
        m_VarMap["DB_UPTIME"] = NewVar;
    }

    //make our own variable called DB_CLIENTS
    {
        CMOOSDBVar NewVar("DB_CLIENTS");
        NewVar.m_cDataType = MOOS_DOUBLE;
        NewVar.m_dfVal= HPMOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = HPMOOSTime();
        m_VarMap["DB_CLIENTS"] = NewVar;
    }
    
    //make our own variable called DB_EVENT
    {
        CMOOSDBVar NewVar("DB_EVENT");
        NewVar.m_cDataType = MOOS_STRING;
        NewVar.m_dfVal= MOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = MOOSTime();
        m_VarMap["DB_EVENT"] = NewVar;
    }

    //make our own variable called DB_VARSUMMARY
    {
        CMOOSDBVar NewVar("DB_VARSUMMARY");
        NewVar.m_cDataType = MOOS_STRING;
        NewVar.m_dfVal= MOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = MOOSTime();
        m_VarMap["DB_VARSUMMARY"] = NewVar;
    }


    //make our own variable called DB_QOS
    {
        CMOOSDBVar NewVar("DB_QOS");
        NewVar.m_cDataType = MOOS_STRING;
        NewVar.m_dfVal= MOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = MOOSTime();
        m_VarMap["DB_QOS"] = NewVar;
    }

    //make our own variable called DB_RWSUMMARY
    {
        CMOOSDBVar NewVar("DB_RWSUMMARY");
        NewVar.m_cDataType = MOOS_STRING;
        NewVar.m_dfVal= MOOSTime();
        NewVar.m_sWhoChangedMe = m_sDBName;
        NewVar.m_sOriginatingCommunity = m_sCommunityName;
        NewVar.m_dfWrittenTime = MOOSTime();
        m_VarMap["DB_RWSUMMARY"] = NewVar;
    }






    //ignore broken pipes as is standard for network apps
#ifndef _WIN32
    signal(SIGPIPE,SIG_IGN);
#endif
    
    
    
}

CMOOSDB::~CMOOSDB()
{
    if(m_pCommServer.get()!=NULL)
        m_pCommServer->Stop();
}




void PrintHelpAndExit()
{
	std::cout<<MOOS::ConsoleColours::Yellow();
	std::cout<<"\nMOOSDB command line help:\n\n";
	std::cout<<MOOS::ConsoleColours::reset();
	std::cout<<"Common MOOS parameters:\n";
	std::cout<<"--moos_file=<string>               specify mission file name (default mission.moos)\n";
	std::cout<<"--moos_port=<positive_integer>     specify server port number (default 9000)\n";
	std::cout<<"--moos_time_warp=<positive_float>  specify time warp\n";
	std::cout<<"--moos_community=<string>          specify community name\n";
    std::cout<<"--moos_print_version               print build and version details\n";
    std::cout<<"--moos_suicide_channel=<str>       suicide monitoring channel (IP address) \n";
    std::cout<<"--moos_suicide_port=<int>          suicide monitoring port  \n";
    std::cout<<"--moos_suicide_phrase=<str>        suicide pass phrase  \n";
    std::cout<<"--moos_suicide_disable             disable suicide monitoring \n";
    std::cout<<"--moos_suicide_print               print suicide conditions \n";
    std::cout<<"--moos_no_colour                   dont use colour in printing \n";





	std::cout<<"\nDB Control:\n";

	std::cout<<"-d    (--dns)                      run with dns lookup\n";
	std::cout<<"-s    (--single_threaded)          run as a single thread (legacy mode)\n";
	std::cout<<"-b    (--moos_boost)               boost priority of communications\n";
	std::cout<<"--moos_timeout=<positive_float>    specify client timeout\n";
	std::cout<<"--response=<string-list>           specify tolerable client latencies in ms\n";
	std::cout<<"--warning_latency=<positive_float>    specify latency above which warning is issued in ms\n";
	std::cout<<"--tcpnodelay                       disable nagle algorithm \n";
	std::cout<<"--audit_port=<unsigned int>        specify port on which to transmit statistics\n";
    std::cout<<"--event_log=<file name>            specify file in which to record events\n";
    std::cout<<"--print_heart_beat                 indicate DB heartbeat every second\n";



//#ifdef MOOSDB_HAS_WEBSERVER
	std::cout<<"--webserver_port=<positive_integer> run webserver on given port\n";
//#endif
	std::cout<<"--help                             print help and exit\n";
	std::cout<<"\nexample:\n";
	std::cout<<"  ./MOOSDB --moos_port=9001 \n";
	std::cout<<"  ./MOOSDB --moos_port=9001 --response=x_app:20,y_app:100,*_instrument:0\n";
	exit(0);
}

void CMOOSDB::OnPrintVersionAndExit()
{
    std::cout<<"--------------------------------------------------\n";
    std::cout<<"MOOS version "<<MOOS_VERSION_NUMBER<<"\n";
    std::cout<<"Built on "<<__DATE__<<" at "<<__TIME__<<"\n";
    std::cout<<MOOS_GIT_VERSION<<"\n";
    std::cout<<"--------------------------------------------------\n";
    exit(0);
}


bool CMOOSDB::Run(int argc,  char * argv[] )
{

	MOOS::CommandLineParser P(argc,argv); // 根据程序启动参数创建命令行解析器。

    //disable colour printing maybe
    if(P.GetFlag("--moos_no_colour")){ // 检查是否要求关闭终端彩色输出。
        MOOS::ConsoleColours::Enable(false);
    }


	//mission file could be first free parameter
	std::string mission_file = P.GetFreeParameter(0, "Mission.moos"); // 获取第一个位置参数作为任务文件名，未提供时使用默认文件。

    //set up mission file
	m_MissionReader.SetFile(mission_file); // 设置任务文件，后续配置项将从该文件读取。
	
    ///////////////////////////////////////////////////////////
    //what is our community name?
    m_MissionReader.GetValue("COMMUNITY",m_sCommunityName); // 从任务文件读取社区名称。

    //is the community name being specified on the cli?
    P.GetVariable("--moos_community",m_sCommunityName); // 命令行参数优先覆盖任务文件中的社区名称。

    ///////////////////////////////////////////////////////////
    // make the DB name
    m_sDBName = "MOOSDB_"+m_sCommunityName; // 根据社区名称生成当前数据库实例名称。


    ///////////////////////////////////////////////////////////
    //what effective time speed up do we want?
        double dfWarp=1.0; // 初始化时间倍率，默认以正常速度运行。
        if(m_MissionReader.GetValue("MOOSTimeWarp",dfWarp)) // 尝试从任务文件读取时间倍率。
		SetMOOSTimeWarp(dfWarp); // 应用任务文件中的时间倍率。
    //overridden in command line?
        P.GetVariable("--moos_time_warp",dfWarp); // 读取命令行时间倍率，并允许其覆盖任务文件设置。
        if(dfWarp>0.0) // 仅对正数时间倍率进行设置。
                SetMOOSTimeWarp(dfWarp); // 应用最终确定的时间倍率。


    ///////////////////////////////////////////////////////////
    //is there a network - default  - true
	bool bDisableNameLookUp = true;
    m_MissionReader.GetValue("NoNetwork",bDisableNameLookUp);

    if(P.GetFlag("-d","--dns"))
    	bDisableNameLookUp = false;


	
    ///////////////////////////////////////////////////////////
	//get the port which the DB should listen on
    m_nPort = DEFAULT_MOOS_SERVER_PORT; // 先使用 MOOS 默认服务端口。
    m_MissionReader.GetValue("SERVERPORT",m_nPort); // 从任务文件读取服务端口。
    //command line overide?
    P.GetVariable("--moos_port",m_nPort); // 命令行端口参数优先覆盖任务文件设置。
    
    ///////////////////////////////////////////////////////////
    //is there an indication in the mission file that a webserver should run?

    int  nWebServerPort = -1; // 使用负数表示默认不启动 Web 服务器。
	m_MissionReader.GetValue("WEBSERVERPORT",nWebServerPort); // 从任务文件读取 Web 服务器端口。
    P.GetVariable("--webserver_port",nWebServerPort); // 允许命令行参数覆盖 Web 服务器端口。


	//if either the mission file or command line have set a webserver port then launch one
    if(nWebServerPort>0) // 端口为正数时启动 Web 服务器。
        m_pWebServer.reset(new CMOOSDBHTTPServer(GetDBPort(), nWebServerPort)); // 创建并保存 Web 服务器实例。

    double dfClientTimeout = 5.0; // 设置客户端超时时间默认值，单位为秒。
    m_MissionReader.GetValue("ClientTimeout",dfClientTimeout); // 从任务文件读取客户端超时时间。
    P.GetVariable("--moos_timeout",dfClientTimeout); // 允许命令行参数覆盖客户端超时时间。

    //do we want to fire up the event logger
    std::string sEventLogFileName; // 保存事件日志文件名。
    if(P.GetVariable("--event_log",sEventLogFileName)) // 检查是否指定了事件日志文件。
    {
        m_EventLogger.Run(sEventLogFileName); // 启动事件日志记录器。
    }


    ///////////////////////////////////////////////////////////
    double dfWarningLatencyMS = 50; // 设置客户端响应延迟告警阈值默认值，单位为毫秒。
	m_MissionReader.GetValue("WarningLatency",dfWarningLatencyMS); // 从任务文件读取延迟告警阈值。
    P.GetVariable("--warning_latency",dfWarningLatencyMS); // 允许命令行参数覆盖延迟告警阈值。


    if(P.GetFlag("--moos_print_version")) // 检查是否只打印版本信息并退出。
        OnPrintVersionAndExit(); // 输出版本信息后结束程序。

#ifdef DEFAULT_NO_NAGLE
    bool bTCPNoDelay = true; // 编译选项要求默认关闭 TCP Nagle 算法。
#else
    bool bTCPNoDelay = false; // 默认保留 TCP Nagle 算法。
#endif
    m_MissionReader.GetValue("tcpnodelay",bTCPNoDelay); // 从任务文件读取 TCP_NODELAY 配置。

    if(P.GetFlag("--tcpnodelay")) // 检查命令行是否要求关闭 Nagle 算法。
	    	bTCPNoDelay = true; // 启用 TCP_NODELAY。



    ///////////////////////////////////////////////////////////
    //are we looking for help?
    if(P.GetFlag("-h","--help")) // 检查是否请求显示帮助信息。
	    	PrintHelpAndExit(); // 打印帮助文本并退出程序。

    ///////////////////////////////////////////////////////////
	//are we looking for help?
    bool bBoost = P.GetFlag("--moos_boost","-b"); // 获取是否提升通信线程优先级的配置。

    ///////////////////////////////////////////////////////////
    //are we being asked to be old skool and use a single thread?
    bool bSingleThreaded = P.GetFlag("-s","--single_threaded"); // 获取是否使用旧版单线程通信模式的配置。


    //is the community name being specified on the cli?
    unsigned int nAuditPort=9020; // 设置审计统计信息发送端口的默认值。
    P.GetVariable("--audit_port",nAuditPort); // 读取命令行指定的审计端口。





    std::string sSuicideAddress; // 保存自杀监控通道地址。
    if(P.GetVariable("--moos_suicide_channel",sSuicideAddress)) // 检查是否配置了自杀监控通道。
    {
        m_SuicidalSleeper.SetChannel(sSuicideAddress); // 设置自杀监控通道地址。
    }

    int nSuicidePort(0); // 保存自杀监控端口，默认值为 0。
    if(P.GetVariable("--moos_suicide_port",nSuicidePort)) // 检查是否配置了自杀监控端口。
    {
        m_SuicidalSleeper.SetPort(nSuicidePort); // 设置自杀监控端口。
    }

    std::string sSuicidePhrase; // 保存自杀监控认证短语。
    if(P.GetVariable("--moos_suicide_phrase",sSuicidePhrase)) // 检查是否配置了认证短语。
    {
        m_SuicidalSleeper.SetPassPhrase(sSuicidePhrase); // 设置自杀监控认证短语。
    }

    if(P.GetFlag("--moos_suicide_print")) // 检查是否要求打印自杀监控配置。
    {
        std::cerr<<"suicide terms and conditions are:\n"; // 打印自杀监控配置标题。
        std::cerr<<" channel  "<<m_SuicidalSleeper.GetChannel()<<"\n"; // 打印监控通道地址。
        std::cerr<<" port     "<<m_SuicidalSleeper.GetPort()<<"\n"; // 打印监控端口。
        std::cerr<<" phrase   \""<<m_SuicidalSleeper.GetPassPhrase()<<"\"\n"; // 打印认证短语。

    }

    if(!P.GetFlag("--moos_suicide_disable")) // 未禁用自杀监控时启动监控线程。
    {
        m_SuicidalSleeper.SetName(m_sDBName); // 使用数据库名称标识当前监控对象。
        m_SuicidalSleeper.Run(); // 启动自杀监控服务。
    }





    
    LogStartTime(); // 记录 MOOSDB 的启动时间。
    
    if(bSingleThreaded) // 根据配置选择单线程或多线程通信服务器。
    {
        std::cout<<MOOS::ConsoleColours::yellow()<<"warning : running in single threaded mode performance will be affected by poor networks\n"<<MOOS::ConsoleColours::reset(); // 输出单线程模式性能警告。
        m_pCommServer.reset(new CMOOSCommServer); // 创建传统单线程通信服务器。
    }
    else
    {
        //std::cerr<<MOOS::ConsoleColours::green()<<"running in multi-threaded mode\n"<<MOOS::ConsoleColours::reset();
        m_pCommServer.reset(new MOOS::ThreadedCommServer); // 创建多线程通信服务器。
    }

    m_pCommServer->SetQuiet(m_bQuiet); // 将静默输出配置传递给通信服务器。

    m_pCommServer->SetOnRxCallBack(OnRxPktCallBack,this); // 注册收到数据包时调用的回调函数。

    m_pCommServer->SetOnDisconnectCallBack(OnDisconnectCallBack,this); // 注册客户端断开连接时的回调函数。

    m_pCommServer->SetOnConnectCallBack(OnConnectCallBack,this); // 注册客户端建立连接时的回调函数。

    m_pCommServer->SetOnFetchAllMailCallBack(OnFetchAllMailCallBack,this); // 注册客户端请求全部待发邮件时的回调函数。

    m_pCommServer->SetClientTimeout(dfClientTimeout); // 设置客户端连接超时时间。

    m_pCommServer->SetWarningLatencyMS(dfWarningLatencyMS); // 设置通信延迟告警阈值。

    m_pCommServer->SetTCPNoDelay(bTCPNoDelay); // 设置是否启用 TCP_NODELAY。

    m_pCommServer->BoostIOPriority(bBoost); // 设置是否提升通信 I/O 线程优先级。

    m_pCommServer->SetCommandLineParameters(argc,argv); // 将原始命令行参数传递给通信服务器。

    m_pCommServer->Run(m_nPort,m_sCommunityName,bDisableNameLookUp,nAuditPort); // 启动通信服务器并监听指定端口。

    m_EventLogger.AddEvent("DBStart","MOOSDB",MOOSFormat("Port=%d",m_nPort)); // 记录数据库启动事件和监听端口。
        
    return true; // 配置和启动流程完成，向调用者报告成功。
}

bool CMOOSDB::IsRunning()
{
	if(m_pCommServer.get()==NULL)
		return false;

	return m_pCommServer->IsRunning();
}


bool CMOOSDB::SetQuiet(bool bQuiet)
{
    m_bQuiet = bQuiet;
    if(m_pCommServer.get()!=NULL)
        m_pCommServer->SetQuiet(m_bQuiet);

    return true;
}


void CMOOSDB::UpdateDBClientsVar()
{
    STRING_LIST Clients;
    m_pCommServer->GetClientNames(Clients);

    std::ostringstream ss;
    std::copy(Clients.begin(),Clients.end(),ostream_iterator<std::string>(ss,","));

    CMOOSMsg DBC(MOOS_NOTIFY,"DB_CLIENTS",ss.str());
    DBC.m_sOriginatingCommunity = m_sCommunityName;
    DBC.m_sSrc = m_sDBName;
    OnNotify(DBC);

}

void CMOOSDB::UpdateQoSVar()
{
    CMOOSMsg DBQOS(MOOS_NOTIFY,"DB_QOS","");

    if(!m_pCommServer->GetTimingStatisticSummary(DBQOS.m_sVal))
        return;

    DBQOS.m_sSrc = m_sDBName;
    DBQOS.m_sOriginatingCommunity = m_sCommunityName;
    OnNotify(DBQOS);

}

template< class T>
void PrintCollection( const T & collection, ostream & out, const std::string & delim = "," )
{
    typename T::const_iterator q;
    for(q = collection.begin();q!=collection.end();)
    {
        out<<(*q);
        if(++q!=collection.end())
            out<<delim;
    }
}

void CMOOSDB::UpdateReadWriteSummaryVar()
{

    std::map<std::string,std::list<std::string> > Pub;
    std::map<std::string,std::list<std::string> > Sub;

    DBVAR_MAP::iterator p;

    for(p=m_VarMap.begin();p!=m_VarMap.end();++p)
    {
        CMOOSDBVar  & rVar = p->second;
        STRING_SET::iterator w;
        REGISTER_INFO_MAP::iterator v;

        std::stringstream ss;
        for(v = rVar.m_Subscribers.begin();v!=rVar.m_Subscribers.end();++v)
            Sub[v->second.m_sClientName].push_back(rVar.m_sName);

        for(w = rVar.m_Writers.begin();w!=rVar.m_Writers.end();++w)
            Pub[*w].push_back(rVar.m_sName);
    }


    STRING_LIST Clients;
    m_pCommServer->GetClientNames(Clients);
    STRING_LIST::iterator q;

    std::ostringstream ss;
    for(q=Clients.begin();q!=Clients.end();)
    {
        ss<<*q<<"=";
        PrintCollection(Sub[*q],ss,":");
        ss<<"&";
        PrintCollection(Pub[*q],ss,":");

        if(++q!=Clients.end())
            ss<<",";

    }

    CMOOSMsg DBS(MOOS_NOTIFY,"DB_RWSUMMARY",ss.str());
    DBS.m_sSrc = m_sDBName;
    DBS.m_sOriginatingCommunity = m_sCommunityName;
    OnNotify(DBS);

}

void CMOOSDB::UpdateDBTimeVars()
{
    CMOOSMsg DBT(MOOS_NOTIFY,"DB_TIME",MOOSTime());
    DBT.m_sOriginatingCommunity = m_sCommunityName;
    DBT.m_sSrc = m_sDBName;
    OnNotify(DBT);

    CMOOSMsg DBUpT(MOOS_NOTIFY,"DB_UPTIME",MOOSTime()-GetStartTime());
    DBUpT.m_sOriginatingCommunity = m_sCommunityName;
    DBUpT.m_sSrc = m_sDBName;
    OnNotify(DBUpT);

}

/**this will be called each time a new packet is recieved*/
bool CMOOSDB::OnRxPkt(const std::string & sClient,MOOSMSG_LIST & MsgListRx,MOOSMSG_LIST & MsgListTx)
{

    MOOSMSG_LIST::iterator p; // 创建输入消息列表的迭代器。
    
    for(p = MsgListRx.begin();p!=MsgListRx.end();++p) // 依次遍历当前数据包中的所有输入消息。
    {
        ProcessMsg(*p,MsgListTx); // 根据消息类型处理消息，并将需要立即回复的内容写入发送列表。
    }
    

    double dfNow = MOOS::Time(); // 获取当前 MOOS 时间，用于判断是否需要更新内部摘要变量。
    if(dfNow-m_dfSummaryTime>2.0) // 每隔两秒执行一次数据库内部状态摘要更新。
    {
        m_dfSummaryTime = dfNow; // 记录本次摘要更新的时间，避免短时间内重复执行。

        //good spot to update our internal time
        UpdateDBTimeVars(); // 更新 DB_TIME 和 DB_UPTIME 等数据库时间变量。

        //and send clients an occasional membersip list
        UpdateDBClientsVar(); // 更新并发布当前连接的客户端列表。

        //update a db summary var once in a while
        UpdateSummaryVar(); // 更新并发布数据库变量摘要。

        //update quality of service summary
        UpdateQoSVar(); // 更新并发布通信质量统计摘要。

        //update variable which publishes who is reading and writing what
        UpdateReadWriteSummaryVar(); // 更新并发布各客户端读写变量的关系摘要。
    }

    if(!MsgListRx.empty()) // 只有本次确实收到消息时，才检查该客户端的待发送邮件。
    {
        
        //now we fill in the packet with our replies to THIS CLIENT
        MOOSMSG_LIST_STRING_MAP::iterator q = m_HeldMailMap.find(sClient); // 查找该客户端对应的延迟发送邮件队列。
        
        if(q==m_HeldMailMap.end()) // 如果客户端尚未建立邮件队列，则创建一个空队列。
        {
            
            //CMOOSMsg & rMsg = MsgListRx.front();
            //there is no mail waiting to be sent to this client
            //should only happen at start up...
            //string sClient = MsgListRx.front().m_sSrc;
            
            MOOSMSG_LIST NewList; // 创建该客户端的新邮件列表。
            
            m_HeldMailMap[sClient] = NewList; // 将空邮件列表登记到客户端映射表中。
            
            q = m_HeldMailMap.find(sClient); // 重新查找并取得新建的客户端邮件队列。
            
            assert(q!=m_HeldMailMap.end()); // 确认客户端邮件队列已经成功创建。
        }
                
        if(q!=m_HeldMailMap.end()) // 确认迭代器有效后，检查该客户端是否有待发送邮件。
        {
            //MOOSTrace("%f OnRxPkt %d messages held for client %s\n",MOOSTime(),q->second.size(),sClient.c_str());

            if(!q->second.empty()) // 如果队列中存在待发送邮件，则将其加入当前发送列表。
            {
                //copy all the held mail to MsgListTx
                MsgListTx.splice(MsgListTx.begin(),
                		q->second,
                		q->second.begin(),
                		q->second.end()); // 将待发送邮件整体移动到发送列表头部，并清空原队列。
            }
        }
    }
    
    return true; // 当前数据包已经处理完成，通知通信服务器回调执行成功。
}

bool CMOOSDB::OnFetchAllMail(const std::string & sWho,MOOSMSG_LIST & MsgListTx)
{
	MOOSMSG_LIST_STRING_MAP::iterator q = m_HeldMailMap.find(sWho);
	if(q!=m_HeldMailMap.end())
	{
		if(!q->second.empty())
		{
            MsgListTx.splice(MsgListTx.begin(),
            		q->second,
            		q->second.begin(),
            		q->second.end());
		}
	}
	return true;
}

/** This functions decides what needs to be done on a message by message basis */
bool CMOOSDB::ProcessMsg(CMOOSMsg &MsgRx,MOOSMSG_LIST & MsgListTx)
{
    
    
    switch(MsgRx.m_cMsgType)
    {
    case MOOS_NOTIFY:    //NOTIFICATION
        return OnNotify(MsgRx);
        break;
    case MOOS_WILDCARD_UNREGISTER:
    case MOOS_UNREGISTER:
        return OnUnRegister(MsgRx);
        break;
    case MOOS_REGISTER:    //REGISTRATION
    case MOOS_WILDCARD_REGISTER:
        return OnRegister(MsgRx);
        break;
    case MOOS_NULL_MSG:
        break;    
    case MOOS_COMMAND:  //COMMAND
        break;
    case MOOS_SERVER_REQUEST:
        return DoServerRequest(MsgRx,MsgListTx);
        break;
    }
    
    return true;
}


/** called when the in focus client is telling us something
has changed. Ie this is a notify packet */
bool CMOOSDB::OnNotify(CMOOSMsg &Msg)
{
    double dfTimeNow = HPMOOSTime(); // 获取当前高精度 MOOS 时间，作为本次写入的时间戳。
    
    CMOOSDBVar & rVar  = GetOrMakeVar(Msg); // 查找消息对应的数据库变量；不存在时创建它。
    
    if(rVar.m_nWrittenTo==0) // 如果变量还从未被写入，则执行首次写入初始化。
    {
        rVar.m_cDataType=Msg.m_cDataType; // 用当前消息的数据类型确定变量的数据类型。


        //look to see if any existing wildcards make us want to subscribe
		//to this new message
        HASH_MAP_TYPE<std::string, std::set<MOOS::MsgFilter> >::const_iterator g; // 声明通配符订阅客户端的迭代器。
        for (g = m_ClientFilters.begin(); g != m_ClientFilters.end(); ++g) // 遍历所有客户端保存的通配符过滤器。
		{
			//for every client
            std::set<MOOS::MsgFilter>::const_iterator h; // 声明当前客户端过滤器集合的迭代器。
            for (h = g->second.begin(); h != g->second.end(); ++h) // 遍历当前客户端的每个过滤器。
			{
				//for every filter
                if (h->Matches(Msg)) // 判断当前通配符过滤器是否匹配这条新变量消息。
				{
					//add the filter owner (client *g) as a subscriber
                    rVar.AddSubscriber(g->first, h->period()); // 将过滤器所属客户端加入订阅者，并设置通知周期。
                    if(!m_bQuiet) // 非静默模式下输出通配符订阅建立信息。
					{
                        std::cout<<"+ subs of \""<<g->first<<"\" to \""
                                <<Msg.GetKey()<<"\" via wildcard \""<<h->as_string()
                                <<"\""<<std::endl; // 打印客户端、变量和匹配过滤器的信息。
					}
				}
			}
		}

    }
    
    if(rVar.m_cDataType==Msg.m_cDataType) // 只有消息类型与变量既有类型一致时才接受本次写入。
    {
        
        rVar.m_dfWrittenTime = dfTimeNow; // 记录数据库实际处理这次写入的时间。
        
        rVar.m_dfTime        = Msg.m_dfTime; // 保存消息自身携带的时间戳。
        
        rVar.m_sWhoChangedMe = Msg.m_sSrc; // 记录最后写入该变量的客户端。
        
        rVar.m_sSrcAux       = Msg.m_sSrcAux; // Added by mikerb 5-29-12
        
        if(Msg.m_sOriginatingCommunity.empty()) // 如果消息没有来源社区，则认为它来自当前社区。
        {
            //we are the server in the originating community
            rVar.m_sOriginatingCommunity = m_sCommunityName; // 设置变量的来源社区为当前数据库社区。
            Msg.m_sOriginatingCommunity = m_sCommunityName; // 同时补全消息对象中的来源社区字段。
        }
        else
        {
            //this message came from another community
            rVar.m_sOriginatingCommunity = Msg.m_sOriginatingCommunity; // 保留消息原有的远端来源社区。
        }
        
        switch(rVar.m_cDataType) // 根据变量类型，将消息值复制到对应的存储字段。
        {
        case MOOS_DOUBLE:
            rVar.m_dfVal = Msg.m_dfVal; // 保存数值型消息内容。
            break;
        case MOOS_STRING:
		case MOOS_BINARY_STRING:
            rVar.m_sVal = Msg.m_sVal; // 保存普通字符串或二进制字符串内容。
            break;
        }
        
        //record sSrc as a writer of this data
        rVar.m_Writers.insert(Msg.m_sSrc); // 记录该消息来源客户端曾经写入过此变量。
        
        //increment the number of times we have written to this variable
        rVar.m_nWrittenTo++; // 累加该变量的总写入次数。
        
        //how often is it being written?
        double dfDT = (dfTimeNow-rVar.m_Stats.m_dfLastStatsTime); // 计算距离上次统计更新经过的时间。
        int nWrites  = rVar.m_nWrittenTo-rVar.m_Stats.m_nLastStatsWrites; // 计算这段时间内新增的写入次数。
        if(dfDT>0.5) // 至少经过半秒才更新一次写入频率，避免统计过于抖动。
        {
            //this looks a little hookey - the numbers are arbitrary to give sensible
            //looking frequencies when timing is coarse
            if(dfDT>10.0) // 如果超过十秒没有及时更新，则认为当前写入频率为零。
            {
                //MIN
                rVar.m_dfWriteFreq = 0.0; // 将长期没有写入的变量频率置零。
            }
            else
            {
                //IIR FILTER COOEFFICENT
                double dfAlpha = 0.5; // 设置一阶 IIR 平滑滤波器的历史值权重。
                double df = dfDT/nWrites; // 计算每次写入的平均时间间隔。

                rVar.m_dfWriteFreq = dfAlpha*rVar.m_dfWriteFreq + (1.0-dfAlpha)/(df); // 用平滑后的间隔倒数更新写入频率。
            }
            rVar.m_Stats.m_nLastStatsWrites = rVar.m_nWrittenTo; // 保存本次统计使用的累计写入次数。
            rVar.m_Stats.m_dfLastStatsTime = dfTimeNow; // 保存本次统计更新时间。
        }
        
        //now comes the intersting part...
        //which clients have asked to be informed
        //of changes in this variable?
        REGISTER_INFO_MAP::iterator p; // 声明订阅者列表迭代器。
        
        
        for(p = rVar.m_Subscribers.begin();p!=rVar.m_Subscribers.end();++p) // 遍历订阅该变量的所有客户端。
        {
            
            CMOOSRegisterInfo & rInfo = p->second; // 获取当前客户端的订阅信息，包括通知周期和上次发送时间。
            //has enough time expired since the last time we
            //sent notification for the variable?
            if(rInfo.Expired(dfTimeNow)) // 只有达到该客户端的通知周期时才发送本次更新。
            {
                
                string  & sClient = p->second.m_sClientName; // 取得待通知客户端的名称。
                
                //the Msg we were passed has all the information we require already
                Msg.m_cMsgType = MOOS_NOTIFY; // 确保转发给订阅者的消息类型是通知消息。
                
                
                AddMessageToClientBox(sClient,Msg); // 将消息放入客户端待发送队列，稍后由通信服务器回包发送。
                

                //finally we remember when we sent this to the client in question
                rInfo.SetLastTimeSent(dfTimeNow); // 记录本次发送时间，控制下一次通知周期。
            }
        }
    }
    else
    {
        MOOSTrace("Attempting to update var \"%s\" which is type %c with data type %c by client %s (=%s)\n", // 输出类型不匹配的诊断信息。
            Msg.m_sKey.c_str(),
            rVar.m_cDataType,
            Msg.m_cDataType,
            Msg.m_sSrc.c_str(),
            Msg.GetAsString(20,4).c_str());

    }
    
    
    
    return true; // 通知处理完成；即使类型不匹配，也通过返回值结束本次回调。
}


/** we now want to store some message in anoth cleints message box, when they next call
in they shall be informed of the change by stuffing this msg into a return packet */
bool    CMOOSDB::AddMessageToClientBox(const string &sClient,CMOOSMsg & Msg)
{
    MOOSMSG_LIST_STRING_MAP::iterator q = m_HeldMailMap.find(sClient);
    
    if(q==m_HeldMailMap.end())
    {
        //there is no mail waiting to be sent to this client
        //should only happen at start up...
        MOOSMSG_LIST NewList;
        m_HeldMailMap[sClient] = NewList;
        
        q = m_HeldMailMap.find(sClient);
        
        assert(q!=m_HeldMailMap.end());
    }
    
    //q->second is now a reference to a list of messages that will be
    //sent to sClient the next time it calls into the database...   
    q->second.push_back(Msg);
    
    return true;
}


/** Called when a msg containing a unregistration (desubscribe) 
request is received */
bool CMOOSDB::OnUnRegister(CMOOSMsg &Msg)
{
	if(Msg.IsType(MOOS_UNREGISTER))
	{
		//what are we looking to un register for?
		bool bAlreadyThere = VariableExists(Msg.m_sKey);
		if(bAlreadyThere)
		{
			CMOOSDBVar & rVar  = GetOrMakeVar(Msg);
			rVar.RemoveSubscriber(Msg.m_sSrc);
		}
	}
	else if (Msg.IsType(MOOS_WILDCARD_UNREGISTER))
	{
		//here we parse out the filter
		std::string app_pattern = "";
		std::string var_pattern = "";
		double period = 0.0;


		MOOSValFromString(app_pattern,Msg.GetString(),"AppPattern");
		MOOSValFromString(var_pattern,Msg.GetString(),"VarPattern");
		MOOS::MsgFilter F(app_pattern,var_pattern,period);

		DBVAR_MAP::iterator q;
		for(q = m_VarMap.begin();q!=m_VarMap.end();++q)
		{
			CMOOSMsg M;
			Var2Msg(q->second,M);
			if(F.Matches(M))
			{
				M.m_cMsgType = MOOS_UNREGISTER;
				M.m_cDataType = MOOS_STRING;
				M.m_sSrc = Msg.GetSource();
				M.m_sKey = q->first;
				OnUnRegister(M);//smart...
			}
		}
	}
    
    return true;
}


/** Called when a msg containing a registration (subscription) 
request is received */
bool CMOOSDB::OnRegister(CMOOSMsg &Msg)
{
    
    //what are we looking to register for?
    if(Msg.IsType(MOOS_REGISTER)) // 判断当前消息是否是普通变量注册请求。
	{

		//if the variable already exists then post a notification message
		//to the client
        bool bAlreadyThere = VariableExists(Msg.m_sKey); // 检查目标变量在数据库中是否已经存在。

        CMOOSDBVar & rVar  = GetOrMakeVar(Msg); // 获取目标变量；不存在时创建变量记录。

        //PMN drops this check to allow notification
        //periods to be changed dynamically 21/12/17
//		if(rVar.HasSubscriber(Msg.m_sSrc))
//			return true;

		if(!rVar.AddSubscriber(Msg.m_sSrc,Msg.m_dfVal)) // 将客户端加入订阅者列表并设置订阅周期。
			return false; // 添加订阅失败时返回失败。

        double dfActualPeriod; // 保存数据库实际采用的通知周期。
        if(!rVar.GetUpdatePeriod(Msg.m_sSrc,dfActualPeriod)){ // 读取该客户端的实际更新周期。
            return false; // 无法读取更新周期时返回失败。
        }
        std::string detail =MOOSFormat("%s@%.1f",rVar.m_sName.c_str(),dfActualPeriod); // 生成包含变量名和周期的日志详情。
        m_EventLogger.AddEvent("register", // 记录普通变量注册事件。
                               Msg.m_sSrc, // 记录发起注册的客户端名称。
                               detail); // 记录变量名和实际通知周期。

		if(bAlreadyThere && rVar.m_nWrittenTo!=0) // 如果变量已存在且已有写入值，则立即向新订阅者发送当前值。
		{
			//when the client registered the variable already existed...
			//better tell them
            CMOOSMsg ReplyMsg; // 创建用于发送当前变量值的回复消息。
            Var2Msg(rVar,ReplyMsg); // 将数据库变量内容转换为 MOOS 消息。

            ReplyMsg.m_cMsgType = MOOS_NOTIFY; // 将回复消息类型设置为变量通知。

            AddMessageToClientBox(Msg.m_sSrc,ReplyMsg); // 将当前值放入客户端待发送队列。

        	 rVar.m_Subscribers[Msg.m_sSrc].SetLastTimeSent(MOOS::Time()); // 更新该客户端的最后发送时间。

		}
	}
    else if(Msg.IsType(MOOS_WILDCARD_REGISTER)) // 判断当前消息是否是通配符注册请求。
	{
		//here we parse out the filter
        std::string app_pattern = ""; // 保存应用名称匹配模式。
        std::string var_pattern = ""; // 保存变量名称匹配模式。
        double period = 0.0; // 保存通配符订阅的通知周期。


        MOOSValFromString(app_pattern,Msg.GetString(),"AppPattern"); // 从消息内容解析应用匹配模式。
        MOOSValFromString(var_pattern,Msg.GetString(),"VarPattern"); // 从消息内容解析变量匹配模式。
        MOOSValFromString(period,Msg.GetString(),"Interval"); // 从消息内容解析通知周期。
        MOOS::MsgFilter F(app_pattern,var_pattern,period); // 根据三个条件创建通配符过滤器。

		//store this filter we will need it later when new
		//as yet undiscovered variables are written
		m_ClientFilters[Msg.GetSource()].insert(F); // 保存该客户端的过滤器，供未来创建变量时使用。


        m_EventLogger.AddEvent("wildcard",Msg.m_sSrc,Msg.GetString()); // 记录通配符注册事件及其原始内容。


		//now iterate over all existing variables and see if they match
		//if the do simply register for them...
        DBVAR_MAP::iterator q; // 声明数据库变量表迭代器。
        for(q = m_VarMap.begin();q!=m_VarMap.end();++q) // 遍历数据库中当前已有的所有变量。
		{
            CMOOSMsg M; // 创建临时消息，用于表示当前数据库变量。
            Var2Msg(q->second,M); // 将当前变量转换为消息，以便交给过滤器判断。
            if(F.Matches(M)) // 判断当前变量是否匹配通配符过滤器。
			{
                M.m_cMsgType = MOOS_REGISTER; // 将临时消息改造成普通注册请求。
                M.m_cDataType = MOOS_DOUBLE; // 注册消息使用数值类型字段承载周期信息。
                M.m_dfVal = period; // 设置普通注册请求的通知周期。
                M.m_sSrc = Msg.GetSource(); // 将注册请求的来源设置为通配符请求客户端。

                if(!m_bQuiet) // 非静默模式下打印匹配变量的订阅信息。
				{
                    std::cout<<MOOS::ConsoleColours::yellow() // 设置终端输出颜色。
                            <<"+ subs of \"" // 输出订阅提示的固定文本。
                            <<Msg.GetSource()<<"\" to variables matching \"" // 输出客户端名称和提示文本。
                            <<var_pattern<<":"<<app_pattern<<"\"" // 输出变量模式和应用模式。
                            <<MOOS::ConsoleColours::reset()<<std::endl; // 恢复终端颜色并换行。
				}

                OnRegister(M);//smart... // 递归调用普通注册逻辑，完成当前匹配变量的订阅。
			}
		}



	}
    
    return true; // 注册请求处理完成。
}


/** return a reference to a DB variable is it already exists
and if not make one and then return a reference to it.
@param Msg      Msg.m_sKey contains name of variable
*/
CMOOSDBVar & CMOOSDB::GetOrMakeVar(CMOOSMsg &Msg)
{    
    
    //look up this variable name
    DBVAR_MAP::iterator p = m_VarMap.find(Msg.m_sKey);
    

    if(p==m_VarMap.end())
    {
        //we need to make a new variable here for this key
        //as we don't know about it!
        
        CMOOSDBVar NewVar(Msg.m_sKey);
        
        switch(Msg.m_cMsgType)
        {
        case MOOS_REGISTER:
        	//interesting case is when this method is being used to
        	//register for a variable that has not been written to. Here
        	//we simply make the variable but don't commit to its data type
        	NewVar.m_cDataType = MOOS_NOT_SET;
        	break;
        case MOOS_NOTIFY:
        	//we are making a new variable
    		//new variable will have data type of request message
        	NewVar.m_cDataType = Msg.m_cDataType;
        	break;
        default:
        	///nothing to do  - I wonder what this is?
        	break;
        }



        //index our new creation
        m_VarMap[Msg.m_sKey] = NewVar;
        
        //check we can get it back ok!!
        p = m_VarMap.find(Msg.m_sKey);
        
        assert(p!=m_VarMap.end());
        
#ifdef DB_VERBOSE
        
        MOOSTrace("\nMaking new variable: \n");
        MOOSTrace("    Name=\"%s\"\n",NewVar.m_sName.c_str());
        MOOSTrace("    Src =\"%s\"\n",Msg.m_sSrc.c_str());
        MOOSTrace("    Type =\"%c\"\n",NewVar.m_cDataType);
#endif

        m_EventLogger.AddEvent("create",Msg.GetSource(),Msg.GetName());

    }
    

    //ok we know what you are talking about
    CMOOSDBVar & rVar = p->second;
    
    //return the reference
    return rVar;
}


bool CMOOSDB::OnConnect(string &sClient)
{
    m_EventLogger.AddEvent("connect",sClient,"client connects");

    //notify ourselves....
    CMOOSMsg DBC(MOOS_NOTIFY,"DB_EVENT",MOOSFormat("connected=%s",sClient.c_str()));
    DBC.m_sOriginatingCommunity = m_sCommunityName;
    DBC.m_sSrc = m_sDBName;
    OnNotify(DBC);


    return true;
}

bool CMOOSDB::OnDisconnect(string &sClient)
{
    //for all variables remove subscriptions to sClient
    if(!m_bQuiet)
    {
        std::cout<<MOOS::ConsoleColours::yellow()<<sClient<<" is leaving...           ";
    }
    
    DBVAR_MAP::iterator p;
    
    for(p=m_VarMap.begin();p!=m_VarMap.end();++p)
    {
        CMOOSDBVar  & rVar = p->second;
        
        rVar.RemoveSubscriber(sClient);
    }
    if(m_ClientFilters.find(sClient)!=m_ClientFilters.end())
    {
    	m_ClientFilters[sClient].clear();
    }
    
    m_HeldMailMap.erase(sClient);
    
    if(!m_bQuiet)
        std::cout<<MOOS::ConsoleColours::Green()<<"[OK]\n"<<MOOS::ConsoleColours::reset();

    m_EventLogger.AddEvent("disconnect",sClient,"client disconnects");


    //notify ourselves....
    CMOOSMsg DBC(MOOS_NOTIFY,"DB_EVENT",MOOSFormat("disconnected=%s",sClient.c_str()));
    DBC.m_sOriginatingCommunity = m_sCommunityName;
    DBC.m_sSrc = m_sDBName;
    OnNotify(DBC);

    return true;
}

bool CMOOSDB::DoServerRequest(CMOOSMsg &Msg, MOOSMSG_LIST &MsgTxList)
{
    //explictly requesting the server to do something...
    
    if(Msg.m_sKey=="ALL")
    {
        return OnServerAllRequested(Msg,MsgTxList);
    }
    else if(Msg.m_sKey.find("PROC_SUMMARY")!=string::npos)
    {
        return OnProcessSummaryRequested(Msg,MsgTxList);
    }
    else if (Msg.m_sKey.find("VAR_SUMMARY") != string::npos) 
    {
        return OnVarSummaryRequested(Msg, MsgTxList);
    }
    else if(Msg.m_sKey.find("DB_CLEAR")!=string::npos)
    {
        return OnClearRequested(Msg,MsgTxList);
    }
    
    
    
    return false;
}

void CMOOSDB::UpdateSummaryVar()
{

    std::stringstream ss;
    DBVAR_MAP::iterator p;

    for(p=m_VarMap.begin();p!=m_VarMap.end();++p)
    {
        ss<<std::left<<std::setw(20);
        ss<<p->first<<" ";

        ss<<std::left<<std::setw(20);
        ss<<MOOS::TimeToDate(p->second.m_dfWrittenTime,false,true)<<" ";

        ss<<std::left<<std::setw(20);
        if(p->second.m_sWhoChangedMe.empty())
        {
            ss<<"(write pending)"<<" ";
        }
        else
        {
            ss<<p->second.m_sWhoChangedMe<<" ";
        }

        //write frequency
        ss << std::fixed << std::setw( 4 ) << std::setprecision( 1 ) << p->second.m_dfWriteFreq<< "Hz ";

        ss<<std::left<<std::setw(2);
        ss<<p->second.m_cDataType<<" ";

        switch(p->second.m_cDataType)
        {
            case MOOS_DOUBLE:
                ss<<p->second.m_dfVal;
                break;
            case MOOS_STRING:
            {
                unsigned int s = p->second.m_sVal.size();
                if(s>25)
                    ss<<(p->second.m_sVal.substr(0,22)+"...");
                else
                    ss<<p->second.m_sVal;

                break;
            }
            case MOOS_BINARY_STRING:
            {
                unsigned int s = p->second.m_sVal.size();
                std::string bss;
                if(s<1024)
                    bss = MOOSFormat("*binary* %-4d B",s);
                else if(s<1024*1024)
                    bss = MOOSFormat("*binary* %.3f KB",s/(1024.0));
                else
                    bss = MOOSFormat("*binary* %.3f MB",s/(1024.0*1024.0));

                ss<<bss;
                break;
            }
        }


        ss<<"\n";

    }

    CMOOSMsg DBC(MOOS_NOTIFY,"DB_VARSUMMARY",ss.str());
    DBC.m_sOriginatingCommunity = m_sCommunityName;
    DBC.m_sSrc = m_sDBName;
    OnNotify(DBC);

}

bool CMOOSDB::OnProcessSummaryRequested(CMOOSMsg &Msg, MOOSMSG_LIST &MsgTxList)
{
    DBVAR_MAP::iterator p;
    STRING_LIST::iterator q;
    STRING_LIST Clients;
    
    m_pCommServer->GetClientNames(Clients);

    for(q=Clients.begin();q!=Clients.end();++q)
    {
        string sWho = *q;
        
        string sPublished= "PUBLISHED=";
        string sSubscribed = "SUBSCRIBED=";
        
        for(p=m_VarMap.begin();p!=m_VarMap.end();++p)
        {
            CMOOSDBVar  & rVar = p->second;
            
            if(rVar.m_Writers.find(sWho)!=rVar.m_Writers.end())
            {
                if(!sPublished.empty())
                {
                    sPublished+=",";
                }
                sPublished+=rVar.m_sName;
                
            }
            
            if(rVar.m_Subscribers.find(sWho)!=rVar.m_Subscribers.end())
            {
                if(!sSubscribed.empty())
                {
                    sSubscribed+=",";
                }
                sSubscribed+=rVar.m_sName;
            }
        }
        
        
        CMOOSMsg MsgReply;
        
        MsgReply.m_nID        = Msg.m_nID;
        
        MsgReply.m_cMsgType   = MOOS_NOTIFY;
        MsgReply.m_cDataType  = MOOS_STRING;
        MsgReply.m_dfTime     = MOOSTime()-m_dfStartTime; //for display
        MsgReply.m_sSrc       = m_sDBName;
        MsgReply.m_sKey       = "PROC_SUMMARY";
        MsgReply.m_sVal       = sWho+":"+sSubscribed+","+sPublished;
        MsgReply.m_dfVal      = -1;
        
        
        MsgTxList.push_front(MsgReply);
    }
    
    return true;
    
}

bool CMOOSDB::OnServerAllRequested(CMOOSMsg &Msg, MOOSMSG_LIST &MsgTxList)
{
    
    DBVAR_MAP::iterator p;
    
    for(p=m_VarMap.begin();p!=m_VarMap.end();++p)
    {
        CMOOSDBVar  & rVar = p->second;
        
        CMOOSMsg MsgVar;
        
        MsgVar.m_nID        = Msg.m_nID;
        
        MsgVar.m_cMsgType   = MOOS_NOTIFY;
        MsgVar.m_cDataType  = rVar.m_cDataType;
        MsgVar.m_dfTime     = rVar.m_dfWrittenTime-m_dfStartTime; //for display
        MsgVar.m_sSrc       = rVar.m_sWhoChangedMe;
        MsgVar.m_sKey       = rVar.m_sName;
        MsgVar.m_sVal       = rVar.m_sVal;
		MsgVar.m_sSrcAux    = rVar.m_sSrcAux;
        MsgVar.m_dfVal      = rVar.m_dfVal;
        MsgVar.m_dfVal2     = rVar.m_dfWriteFreq;
        MsgVar.m_sOriginatingCommunity = rVar.m_sOriginatingCommunity;
        
        if(MsgVar.m_dfTime<0) 
        {
            MsgVar.m_dfTime =-1;
        }
        
        MsgTxList.push_front(MsgVar);
        
        
    }
    
    
    return true;
}

//Suggested addition by MIT users 2006 - shorter version of OnProcessSummary
bool CMOOSDB::OnVarSummaryRequested(CMOOSMsg &Msg, MOOSMSG_LIST &MsgTxList)
{
    std::string TheVars;
    DBVAR_MAP::iterator p;
    for(p = m_VarMap.begin(); p != m_VarMap.end(); ++p) 
    {
        //look to a comma
        if(p!=m_VarMap.begin())
            TheVars += ",";

        TheVars += p->first;
    }
    
    CMOOSMsg Reply;

    //so the client knows the query result correspondences
    Reply.m_nID = Msg.m_nID; 
    Reply.m_cMsgType = MOOS_NOTIFY;
    Reply.m_cDataType = MOOS_STRING;
    Reply.m_dfTime = MOOSTime();
    Reply.m_sSrc = m_sDBName;
    Reply.m_sKey = "VAR_SUMMARY";
    Reply.m_sVal = TheVars;
    Reply.m_dfVal = -1;

    MsgTxList.push_front(Reply);

    return true;
}





bool CMOOSDB::OnClearRequested(CMOOSMsg &Msg, MOOSMSG_LIST &MsgTxList)
{
	MOOS::DeliberatelyNotUsed(Msg);
	MOOS::DeliberatelyNotUsed(MsgTxList);

    MOOSTrace("Clear Down Requested:\n");
    
    MOOSTrace("    Resetting %d variables...",m_VarMap.size());
    
    DBVAR_MAP::iterator p;
    
    for(p = m_VarMap.begin();p!=m_VarMap.end();++p)
    {
        CMOOSDBVar & rVar = p->second;
        rVar.Reset();
    }
    MOOSTrace("done\n");
    
    
    MOOSTrace("    Removing %d existing notification queues...",m_HeldMailMap.size());
    MOOSMSG_LIST_STRING_MAP::iterator q;
    
    for(q = m_HeldMailMap.begin();q!=m_HeldMailMap.end();++q)
    {
        MOOSMSG_LIST & rList = q->second;
        rList.clear();
    }
    MOOSTrace("done\n");
    
    //MOOSTrace("    resetting DB start Time...done\n");
    LogStartTime();
    
    
    return true;
    
}


void CMOOSDB::LogStartTime()
{
    m_dfStartTime = MOOSTime();
    
    return;
  
    
}



bool CMOOSDB::VariableExists(const string &sVar)
{
    DBVAR_MAP::iterator p=m_VarMap.find(sVar);
    
    return (!m_VarMap.empty()) && (p!=m_VarMap.end());
}

void CMOOSDB::Var2Msg(CMOOSDBVar &Var, CMOOSMsg &Msg)
{
    Msg.m_dfTime = Var.m_dfTime;
    Msg.m_cDataType = Var.m_cDataType;
    Msg.m_sSrc = Var.m_sWhoChangedMe;
    Msg.m_sSrcAux = Var.m_sSrcAux;   // Added by mikerb 5-29-12
    Msg.m_sKey = Var.m_sName;
    Msg.m_sOriginatingCommunity = Var.m_sOriginatingCommunity;
    switch(Var.m_cDataType)
    {
    case MOOS_DOUBLE:
        Msg.m_dfVal = Var.m_dfVal;
        break;
    case MOOS_STRING:
	case MOOS_BINARY_STRING:
        Msg.m_sVal = Var.m_sVal;
        break;
    }
}


