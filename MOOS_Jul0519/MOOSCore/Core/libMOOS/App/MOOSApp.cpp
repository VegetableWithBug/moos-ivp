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
//   are made available under the terms of the GNU Lesser Public License v2.1
//   which accompanies this distribution, and is available at
//   http://www.gnu.org/licenses/lgpl.txt  This program is distributed in the hope that it will be useful, 
//   but WITHOUT ANY WARRANTY; without even the implied warranty of 
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
//
////////////////////////////////////////////////////////////////////////////
**/



// MOOSApp.cpp: implementation of the CMOOSApp class.
//
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#pragma warning(disable : 4786)
#pragma warning(disable : 4503)
#endif



#include "MOOS/libMOOS/Utils/MOOSPlaybackStatus.h"
#include "MOOS/libMOOS/App/MOOSApp.h"
#include "MOOS/libMOOS/Utils/ConsoleColours.h"
#include "MOOS/libMOOS/MOOSVersion.h"
#include "MOOS/libMOOS/GitVersion.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <iterator>


#ifdef ASYNCHRONOUS_CLIENT
#include "MOOS/libMOOS/Thirdparty/PocoBits/Event.h"
#endif



using namespace std;

// predicate for sorting on time
bool MOOSMsgTimeSorter(const CMOOSMsg  &M1, const CMOOSMsg &M2) 
{
    return (M1.GetTime() < M2.GetTime());
}

//////////////////////////////////////////////////
//  these are file-scope methods which allow
//  redirection of a call back into the CMOOSApp Class
//  they don't concern the average user...
///////////////////////////////////////////////////
bool MOOSAPP_OnConnect(void * pParam)
{
    if(pParam!=NULL)
    {
        CMOOSApp* pApp = (CMOOSApp*)pParam;

        //we may have private work to do
        pApp->OnConnectToServerPrivate();

        //client work
        return pApp->OnConnectToServer();

    }
    return false;
}

bool MOOSAPP_OnDisconnect(void * pParam)
{
    if(pParam!=NULL)
    {
        CMOOSApp* pApp = (CMOOSApp*)pParam;

        //private work
        pApp->OnDisconnectToServerPrivate();

        //client work
        return pApp->OnDisconnectFromServer();

    }
    return false;
}


bool MOOSAPP_OnMail(void *pParam)
{
    if(pParam!=NULL)
    {
        CMOOSApp* pApp = (CMOOSApp*)pParam;
        
        //client mail work
        return pApp->OnMailCallBack();
        
    }
    return false;
    
}

bool MOOSAPP_OnMessage(CMOOSMsg & M, void *pParam)
{

    if(pParam!=NULL)
    {
        CMOOSApp* pApp = (CMOOSApp*)pParam;

        //client mail work
        return pApp->OnMessage(M);

    }
    return false;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
CMOOSApp::CMOOSApp()
{
    m_dfFreq=DEFAULT_MOOS_APP_FREQ;
    m_nCommsFreq=DEFAULT_MOOS_APP_COMMS_FREQ;
    m_dfMaxAppTick = DEFAULT_MOOS_APP_FREQ; //we can respond to mail very quickly but by default we are cautious
    m_IterationMode = REGULAR_ITERATE_AND_MAIL;
    m_nIterateCount = 0;
    m_nMailCount = 0;
    m_bServerSet = false;
    m_dfAppStartTime = -1;
    m_bDebug = false;
    m_bSimMode = false;
    m_bQuiet = false;
    m_bUseMOOSComms = true;
    m_dfLastRunTime = -1;
    m_bCommandMessageFiltering = false;
    m_dfLastStatusTime = -1;
    m_bSortMailByTime = true;
	m_bAppError = false;
    m_bQuitOnIterateFail = false;
	m_bQuitRequested = false;
    
    SetMOOSTimeWarp(1.0);
    
#ifdef ASYNCHRONOUS_CLIENT
    m_pMailEvent = new MOOS::Poco::Event;
    UseMailCallBack();
#endif

    EnableIterateWithoutComms(false);
}

CMOOSApp::~CMOOSApp()
{
#ifdef ASYNCHRONOUS_CLIENT
    delete m_pMailEvent;
#endif
}



void CMOOSApp::OnPrintExampleAndExit()
{
	std::cout<<" tragically, there is no example configuration for this app\n";
	exit(0);
}

void CMOOSApp::OnPrintInterfaceAndExit()
{
	std::cout<<" sadly, there is no description of the interface for this app\n";
	exit(0);
}


void CMOOSApp::PrintDefaultCommandLineSwitches()
{
	std::cout<<MOOS::ConsoleColours::Yellow()<<"\n";
	std::cout<<"--------------------------------------------------\n";
	std::cout<<GetAppName()<<"'s standard MOOSApp switches are:\n";
	std::cout<<"--------------------------------------------------\n";
	std::cout<<MOOS::ConsoleColours::reset()<<"\n";
	std::cout<<"\nvariables:\n";
	std::cout<<"  --moos_app_name=<string>    : name of application\n";
	std::cout<<"  --moos_name=<string>        : name with which to register with MOOSDB\n";
	std::cout<<"  --moos_file=<string>        : name of configuration file\n";
	std::cout<<"  --moos_host=<string>        : address of machine hosting MOOSDB\n";
	std::cout<<"  --moos_port=<number>        : port on which DB is listening \n";
	std::cout<<"  --moos_app_tick=<number>    : frequency of application (if relevant) \n";
	std::cout<<"  --moos_max_app_tick=<number>: max frequency of application (if relevant) \n";
	std::cout<<"  --moos_comms_tick=<number>  : frequency of comms (if relevant) \n";
    std::cout<<"  --moos_tw_delay_factor=<num>: comms delay as % of time warp (if relevant) \n";



	std::cout<<"  --moos_iterate_Mode=<0,1,2> : set app iterate mode \n";
	std::cout<<"  --moos_time_warp=<number>   : set time warp \n";
    std::cout<<"  --moos_suicide_channel=<str>: suicide monitoring channel (IP address) \n";
    std::cout<<"  --moos_suicide_port=<int>   : suicide monitoring port  \n";
    std::cout<<"  --moos_suicide_phrase=<str> : suicide pass phrase  \n";

	std::cout<<"\nflags:\n";
	std::cout<<"  --moos_iterate_no_comms     : enable iterate without comms \n";
	std::cout<<"  --moos_filter_command       : enable command message filtering \n";
	std::cout<<"  --moos_no_sort_mail         : don't sort mail by time \n";
	std::cout<<"  --moos_no_comms             : don't start communications \n";
	std::cout<<"  --moos_quiet                : don't print banner information \n";
	std::cout<<"  --moos_quit_on_iterate_fail : quit if iterate fails \n";
	std::cout<<"  --moos_no_colour            : disable colour printing \n";
    std::cout<<"  --moos_suicide_disable      : disable suicide monitoring \n";
    std::cout<<"  --moos_suicide_print        : print suicide conditions \n";



	std::cout<<"\nhelp:\n";
	std::cout<<"  --moos_print_example        : print an example configuration block \n";
	std::cout<<"  --moos_print_interface      : describe the interface (subscriptions/pubs) \n";
	std::cout<<"  --moos_print_version        : print the version of moos in play \n";
	std::cout<<"  --moos_help                 : print help on moos switches\n";
    std::cout<<"  --moos_configuration_audit  : print configuration terms searched for\n";
	std::cout<<"  --help                      : print help on moos switches and custom help\n";


}

void CMOOSApp::OnPrintVersionAndExit()
{
	std::cout<<"--------------------------------------------------\n";
	std::cout<<"MOOS version "<<MOOS_VERSION_NUMBER<<"\n";
	std::cout<<"Built on "<<__DATE__<<" at "<<__TIME__<<"\n";
	std::cout<<MOOS_GIT_VERSION<<"\n";
	std::cout<<"--------------------------------------------------\n";
	exit(0);
}


void CMOOSApp::OnPrintHelpAndExit()
{
	std::cout<<" distressingly, there is no custom help for this app\n";
	exit(0);
}


//this is an overloaded 3 parameter version which allows explicit setting of the registration name
bool CMOOSApp::Run(const std::string & sName,const std::string & sMissionFile,const std::string & sMOOSName)
{
    //fill in specialised MOOSName
    m_sMOOSName = sMOOSName;
    return Run(sName,sMissionFile);
}

//this is an overloaded 3 parameter version which allows explicit setting of the registration name
bool CMOOSApp::Run(const std::string &  sName,int argc,  char * argv[])
{
	SetCommandLineParameters(argc,argv);
	return Run(sName);
}

bool  CMOOSApp::Run(const std::string &  sName,const std::string & sMissionFile, int argc,  char * argv[])
{
	SetCommandLineParameters(argc,argv);
	return Run(sName,sMissionFile);
}

//the main MOOSApp Run function
bool CMOOSApp::Run( const std::string & sName,
                    const std::string & sMissionFile)
{

    if(m_CommandLineParser.GetFlag("--moos_no_colour")) // 检查是否要求关闭终端颜色输出。
	{
        MOOS::ConsoleColours::Enable(false); // 全局关闭 MOOS 的彩色日志输出。
	}


    m_sAppName      = sName; // 保存调用者传入的应用名，作为默认值。
    m_CommandLineParser.GetOption("--moos_app_name",m_sAppName); // 命令行参数存在时，用它覆盖默认应用名。

    m_sMissionFile  = sMissionFile; // 保存传入的任务文件路径，作为默认值。
    m_CommandLineParser.GetVariable("--moos_file",m_sMissionFile); // 用 --moos_file 覆盖任务文件路径。

    m_MissionReader.SetAppName(m_sAppName); // 告诉任务文件读取器当前应用名，用于读取对应配置块。

    if(m_sMOOSName.empty()) // 如果尚未设置 MOOS 注册名，则默认使用应用名。
        m_sMOOSName=m_sAppName; // 设置默认的 MOOSDB 客户端注册名。

    m_CommandLineParser.GetVariable("--moos_name",m_sMOOSName); // 用 --moos_name 覆盖 MOOS 注册名。


    if(m_CommandLineParser.GetFlag("--moos_help")) // 如果请求标准帮助信息，则打印后退出。
	{
        PrintDefaultCommandLineSwitches(); // 打印 MOOSApp 支持的命令行选项。
        exit(0); // 正常结束进程，不进入应用运行阶段。
	}

    if(m_CommandLineParser.GetFlag("--help")) // 如果请求完整帮助信息，则打印标准和自定义帮助。
	{
        PrintDefaultCommandLineSwitches(); // 打印标准 MOOSApp 选项。
        std::cout<<"\ncustom help:\n"; // 输出自定义帮助部分的标题。
        OnPrintHelpAndExit(); // 调用派生类可重载的自定义帮助函数，通常会退出。
        exit(0); // 若自定义帮助函数没有退出，则在这里正常结束进程。
	}

    if(m_CommandLineParser.GetFlag("--moos_print_example")) // 请求打印示例配置时执行示例输出并退出。
        OnPrintExampleAndExit(); // 调用可由派生类重载的示例配置输出函数。

    if(m_CommandLineParser.GetFlag("--moos_print_interface")) // 请求打印应用接口时执行接口输出并退出。
        OnPrintInterfaceAndExit(); // 调用可由派生类重载的接口说明函数。

    if(m_CommandLineParser.GetFlag("--moos_print_version")) // 请求打印版本信息时输出版本并退出。
        OnPrintVersionAndExit(); // 打印 MOOS 版本、构建时间和 Git 版本。




	if(!Configure()) // 读取任务文件并解析应用、通信和运行模式配置。
	{
		std::cerr<<"configure returned false. Quitting\n"; // 配置失败时输出错误信息。
		return false; // 终止 Run()，不启动应用主循环。
	}

	if(m_CommandLineParser.IsAvailable()) // 如果还有未处理的命令行参数，交给应用处理。
		OnProcessCommandLine(); // 调用可由派生类重载的命令行后处理函数。
    
	m_dfAppStartTime = MOOSTime(); // 记录应用启动时间，用于运行时间和状态统计。

    if(m_bUseMOOSComms) // 根据配置决定是否启动 MOOS 通信客户端。
    {

        if(!ConfigureComms()) // 读取服务器地址、端口和通信频率，并启动通信线程。
        {
            return false; // 通信配置失败，应用无法继续启动。
        }

        int t = 0; // 累计等待时间，单位为毫秒。
        int dT = 50; // 每次检查连接状态之间暂停 50 毫秒。
        while(!m_Comms.IsConnected()) // 在通信线程后台连接 MOOSDB 时等待连接建立。
        {
            MOOSPause(dT); // 暂停一小段时间，避免忙等占用 CPU。
			t+=dT; // 累加等待时间。
            if(t>5000) // 最多等待约 5 秒，避免启动阶段无限阻塞。
                break; // 超时后继续启动，后续通信线程仍会自行重连。
        }
        MOOSPause(500); // 给通信启动日志留出输出时间，仅影响显示，不影响线程安全。
    }


    if(!OnStartUpPrepare()) // 调用派生类启动准备回调。
    {
        MOOSTrace("Derived OnStartUpPrepare() returned false... Quitting\n"); // 报告启动准备失败。
        return false; // 中止应用启动。
    }

    if(!OnStartUp()) // 调用派生类主要启动初始化回调。
    {
        MOOSTrace("Derived OnStartUp() returned false... Quitting\n"); // 报告初始化失败。
        return false; // 中止应用启动。
    }


    if(!OnStartUpComplete()) // 调用派生类启动完成回调，执行最后的初始化工作。
    {
        MOOSTrace("Derived OnStartUpComplete() returned false... Quitting\n"); // 报告启动收尾失败。
        return false; // 中止应用启动。
    }

    if(m_CommandLineParser.GetFlag("--moos_configuration_audit")) // 只审计配置参数时，不进入主循环。
    {
        PrintSearchedConfigurationFileParameters(); // 打印任务文件中实际查询过的配置项。
        return true; // 完成审计后正常返回。
    }


    DoBanner(); // 输出应用启动信息和当前运行频率等摘要。




    /****************************  THE MAIN MOOS APP LOOP **********************************/

    while(!m_bQuitRequested) // 持续运行，直到应用收到退出请求。
    {
        bool bOK = DoRunWork(); // 执行一轮收邮件、内部维护和派生类 Iterate() 逻辑。

        if(m_bQuitOnIterateFail && !bOK) // 如果配置为迭代失败退出，并且本轮执行失败。
            return MOOSFail("MOOSApp Exiting as requested"); // 输出错误并立即结束应用。


    }

    /***************************   END OF MOOS APP LOOP ***************************************/

    m_Comms.Close(); // 退出主循环后停止通信线程并清理通信资源。

    return true; // 应用正常退出。
}


bool CMOOSApp::GetFlagFromCommandLineOrConfigurationFile(std::string sOption,bool bPrependMinusMinusForCommandLine)
{
    bool bC,bF,bFlag;
    bF = m_MissionReader.GetConfigurationParam(sOption,bFlag);

    if(bPrependMinusMinusForCommandLine)
        sOption = "--"+sOption;

    bC = m_CommandLineParser.GetFlag(sOption);
    if(bC)
    {
        return true;
    }
    if(bF)
    {
        //std::cerr<<"found in config file:"<<sOption<<"\n";
        return bFlag;
    }
    return false;
}



bool CMOOSApp::Configure()
{

    //are we being asked to be quiet?
    SetQuiet(GetFlagFromCommandLineOrConfigurationFile("moos_quiet") || m_bQuiet);

	//can we see the mission file?
	if(!m_MissionReader.SetFile(m_sMissionFile.c_str()))
	{
	    if(!m_bQuiet)
	    {
            std::cerr<<MOOS::ConsoleColours::yellow();
            std::cerr<<"note mission file "<<m_sMissionFile<<" was not found\n";
            std::cerr<<MOOS::ConsoleColours::reset();
	    }
	}

	//are we being asked to iterate without comms
    EnableIterateWithoutComms(GetFlagFromCommandLineOrConfigurationFile("moos_iterate_no_comms"));

    //are we having our suicide address being set?
    std::string sSuicideAddress;
    if(GetParameterFromCommandLineOrConfigurationFile("moos_suicide_channel",sSuicideAddress))
    {
        m_SuicidalSleeper.SetChannel(sSuicideAddress);
    }

    //are we having our suicide port being set?
    int nSuicidePort=-1;
    if(GetParameterFromCommandLineOrConfigurationFile("moos_suicide_port",nSuicidePort))
    {
        m_SuicidalSleeper.SetPort(nSuicidePort);
    }

    //are we having our suicide phrase being set?
    std::string sSuicidePhrase;
    if(GetParameterFromCommandLineOrConfigurationFile("moos_suicide_phrase",sSuicidePhrase))
    {
        m_SuicidalSleeper.SetPassPhrase(sSuicidePhrase);
    }

    //are we being told to print out how to get us to commit suicide?
    if(GetFlagFromCommandLineOrConfigurationFile("moos_suicide_print"))
    {
        std::cerr<<"suicide terms and conditions are:\n";
        std::cerr<<" channel  "<<m_SuicidalSleeper.GetChannel()<<"\n";
        std::cerr<<" port     "<<m_SuicidalSleeper.GetPort()<<"\n";
        std::cerr<<" phrase   \""<<m_SuicidalSleeper.GetPassPhrase()<<"\"\n";
    }

    //are we being told to disable suicide?
    if(!GetFlagFromCommandLineOrConfigurationFile("moos_suicide_disable"))
    {

        //no - then allow it....
        m_SuicidalSleeper.SetName(GetAppName());
        m_SuicidalSleeper.Run();
    }


	//what is the global time warp
	double dfTimeWarp = 1.0;

    if(GetParameterFromCommandLineOrConfigurationFile("moos_time_warp",dfTimeWarp) ||
            m_MissionReader.GetValue("MOOSTimeWarp", dfTimeWarp))
	{
		SetMOOSTimeWarp(dfTimeWarp);
	}

    double dfTimeWarpCommsFactor = 0.0;
    if(GetParameterFromCommandLineOrConfigurationFile("moos_tw_delay_factor",dfTimeWarpCommsFactor))
    {
        if(!m_Comms.SetCommsControlTimeWarpScaleFactor(dfTimeWarpCommsFactor))
        {
            std::cerr<<MOOS::ConsoleColours::Red();
            std::cerr<<"ERROR: failed to set moos_tw_delay_factor\n";
            std::cerr<<MOOS::ConsoleColours::reset();
        }
    }


	//are we expected to use MOOS comms?
    if(GetFlagFromCommandLineOrConfigurationFile("moos_no_comms"))
    {
        m_bUseMOOSComms = false;
    }
    //alternative (older version)
    m_MissionReader.GetValue("UseMOOSComms", m_bUseMOOSComms);


	//are we being asked to sort mail by time..
    m_bSortMailByTime = true;
    if(GetFlagFromCommandLineOrConfigurationFile("moos_no_sort_mail"))
    {
        m_bSortMailByTime = false;
    }
    //alternative
    m_MissionReader.GetConfigurationParam("SortMailByTime",m_bSortMailByTime);


	//are we being asked to quit if iterate fails?
    if(GetFlagFromCommandLineOrConfigurationFile("moos_quit_on_iterate_fail"))
    {
        m_bQuitOnIterateFail = true;
    }



	//OK now figure out our tick speeds  above what is set by default
	//in derived class constructors this can be set in the process config block
	//by the mission architect
    GetParameterFromCommandLineOrConfigurationFile("moos_app_tick",m_dfFreq);
	m_MissionReader.GetConfigurationParam("APPTICK",m_dfFreq);

    GetParameterFromCommandLineOrConfigurationFile("moos_max_app_tick",m_dfMaxAppTick);
	m_MissionReader.GetConfigurationParam("MAXAPPTICK",m_dfMaxAppTick);

	unsigned int nMode = 0;

    GetParameterFromCommandLineOrConfigurationFile("moos_iterate_mode",nMode);
	m_MissionReader.GetConfigurationParam("ITERATEMODE",nMode);


	switch(nMode)
	{
		case 0: SetIterateMode(REGULAR_ITERATE_AND_MAIL); break;
		case 1: SetIterateMode(COMMS_DRIVEN_ITERATE_AND_MAIL); break;
		case 2: SetIterateMode(REGULAR_ITERATE_AND_COMMS_DRIVEN_MAIL); break;
		default:SetIterateMode(REGULAR_ITERATE_AND_MAIL); break;

	}



	//do we want to enable command filtering (default is set in constructor)
    if(GetFlagFromCommandLineOrConfigurationFile("moos_filter_command"))
    {
        m_bCommandMessageFiltering = true;
    }
    //alternative
    m_MissionReader.GetConfigurationParam("CatchCommandMessages",m_bCommandMessageFiltering);


	return IsConfigOK();

}

void CMOOSApp::PrintSearchedConfigurationFileParameters()
{
    std::list<std::string> L = m_MissionReader.GetSearchedParameters(GetAppName());
    std::cout<<MOOS::ConsoleColours::Green();
    std::cout<<"\nThis application has searched the configuration file for the following parameters:\n\n   ";
    std::cout<<MOOS::ConsoleColours::green();
    std::copy(L.begin(),L.end(),std::ostream_iterator<std::string>(std::cerr,"\n   ") );
    std::cout<<MOOS::ConsoleColours::reset();

}

bool CMOOSApp::OnProcessCommandLine()
{
	//use things like
	//m_CommandLineParser.GetVariable("--moos_name",std::string);

	return true;
}

bool CMOOSApp::IsConfigOK()
{
	//put checks in here....if needed (or overload)
	return true;
}

bool CMOOSApp::SetQuiet(bool bQ)
{
	m_bQuiet = bQ;
	m_Comms.SetQuiet(m_bQuiet);
	return true;
}

void CMOOSApp::SetMOOSName(const std::string &sMOOSName)
{
	m_sMOOSName=sMOOSName;
}


void CMOOSApp::DoBanner()
{
	if(m_bQuiet)
		return;
	MOOSTrace("%s is Running:\n",GetAppName().c_str());
	MOOSTrace(" |-Baseline AppTick   @ %.1f Hz\n",m_dfFreq);
	if(m_Comms.IsAsynchronous())
	{
		MOOSTrace(" |--Comms is Full Duplex and Asynchronous\n");
		switch(m_IterationMode)
		{
		case REGULAR_ITERATE_AND_MAIL:
			std::cout<<" -Iterate Mode 0 :\n   |-Regular iterate and message delivery at "<<m_dfFreq<<" Hz\n";
			break;
		case COMMS_DRIVEN_ITERATE_AND_MAIL:
			std::cout<<" |--Iterate Mode 1 :\n   |-Dynamic iterate speed driven by message delivery ";
			if(m_dfMaxAppTick==0.0)
				std::cout<<"at an unlimited rate\n";
			else
				std::cout<<"at up to "<<m_dfMaxAppTick<<"Hz\n";
			break;
		case REGULAR_ITERATE_AND_COMMS_DRIVEN_MAIL:
			std::cout<<" |--Iterate Mode 2 :\n   -Regular iterate at "<<m_dfFreq<<" Hz. \n   |-Dynamic message delivery ";
			if(m_dfMaxAppTick==0.0)
				std::cout<<"at an unlimited rate\n";
			else
				std::cout<<"at up to "<<m_dfMaxAppTick<<"Hz\n";

			break;
		}
	}
	else
	{
		MOOSTrace(" |\t Comms is Synchronous\n");
		MOOSTrace(" |\t Baseline CommsTick @ %d Hz\n",m_nCommsFreq);
	}

	if(GetMOOSTimeWarp()!=1.0)
		MOOSTrace("\t|-Time Warp @ %.1f \n",GetMOOSTimeWarp());
	if(m_Comms.GetCommsControlTimeWarpScaleFactor()>0.0  && GetMOOSTimeWarp()>1.0)
	    MOOSTrace("\t|-Time Warp delay @ %.1f ms \n",m_Comms.GetCommsControlTimeWarpScaleFactor()*GetMOOSTimeWarp());


	std::cout<<"\n\n";
    std::cout<<std::flush;


}

/*called by a third party to request a MOOS App to quit - only useful for 
 example if a MOOSApp is run in a secondary thread */
bool CMOOSApp::RequestQuit()
{
	m_bQuitRequested = true;
	return true;
}

bool CMOOSApp::DoRunWork()
{

	bool bIterateRequired = true;  // 这次循环是否需要执行用户的 Iterate()，默认是需要的。

	SleepAsRequired(bIterateRequired);  // 根据 AppTick/消息触发模式决定是否需要睡眠，并更新是否应执行 Iterate。

	//std::cerr<<"bIterate:"<<bIterateRequired<<"\n";  // 调试输出：输出是否要执行迭代的状态。

    //store for derived class use the last time iterate was called;  // 记录上一次 Iterate() 执行时刻，供派生类和计时逻辑使用。
    m_dfLastRunTime = MOOSLocalTime();  // 设定当前本地时间为“最近一次运行时刻”。

    //local vars  // 定义局部变量，用于临时存储收到的 MOOS 消息。
    MOOSMSG_LIST MailIn;  // 存放本轮从 MOOSDB 拉取到的新邮件列表。
    if(m_bUseMOOSComms)  // 如果此应用启用了 MOOS 通信功能，再去拉取邮件并处理。
    {
        if( m_Comms.Fetch(MailIn))  // 从通信客户端中获取当前 inbox 中的消息；成功返回 true。
        {
            /////////////////////////////  // 邮件处理区块。
            //   process mail  // 处理接收到的新邮件。
            if(m_bSortMailByTime)  // 若配置要求按时间排序消息，则按时间升序排序。
                MailIn.sort(MOOSMsgTimeSorter);  // 将 MailIn 按 CMOOSMsg 的时间戳排序。
            
            
            //call our own private version  // 先调用内部私有方法，负责统一处理命令消息等底层逻辑。
            OnNewMailPrivate(MailIn);  // 执行私有的邮件处理逻辑，例如过滤命令消息。
            
            //classes will have their own personal versions of this  // 再调用派生类重写的 OnNewMail()，让应用自定义处理业务逻辑。
            OnNewMail(MailIn);  // 调用用户/派生类定义的邮件处理回调。
            
            m_nMailCount++;  // 统计本轮接收到了多少次新邮件。
        }
        

        if(m_Comms.IsConnected() ||  CanIterateWithoutComms() )  // 如果已连接到 MOOSDB，或者允许无通信也可继续迭代，则继续执行循环逻辑。
        {
            //do private work  // 执行内部状态维护工作，比如周期性状态上报。
            IteratePrivate();  // 调用内部私有 Iterate 逻辑，负责发送状态等后台任务。
            
            if(bIterateRequired)  // 只有在本轮确实需要执行应用 Iterate() 时才调用用户逻辑。
            {
				//////////////////////////////////////  // 应用特定处理区域。
				//  do application specific processing  // 执行用户定义的应用处理逻辑。

                /** called just after Iterate has finished - another place to overload*/  // 在 Iterate() 执行前调用的准备回调，供派生类重载。
            	bool bOK = OnIteratePrepare();  // 调用派生类的迭代前准备逻辑。
            	if(m_bQuitOnIterateFail && !bOK)  // 如果配置了“迭代失败就退出”，且返回 false，则停止应用。
            		return false;  // 提前返回，退出 Run() 循环。

				bOK = Iterate();  // 调用用户自定义的主循环处理函数。
				if(m_bQuitOnIterateFail && !bOK)  // 如果 Iterate() 返回 false 且要求退出，则终止。
					return false;  // 直接退出该轮处理并终止应用。

				bOK = OnIterateComplete();  // 调用迭代结束后的收尾回调。
            	if(m_bQuitOnIterateFail && !bOK)  // 如果收尾逻辑失败且要求退出，则立即退出。
            		return false;  // 中止当前运行循环。


            }
            
            m_nIterateCount++;  // 记录本轮 Iterate 执行完成次数（或本轮循环次数）。
        }
    }
    else  // 如果不使用 MOOS 通信，则采用简化逻辑：直接执行 Iterate()，不读取邮件。
    {
        //do private work  // 执行内部辅助工作，比如状态上报。
        IteratePrivate();  // 在无通联模式下仍执行内部状态维护逻辑。

        if(bIterateRequired)  // 如果当前应该执行 Iterate，则执行用户逻辑。
        {
			/////////////////////////////////////////  // 无通讯模式下的处理逻辑。
			//  do application specific processing  // 执行应用主循环处理。
			bool bOK = Iterate();  // 直接调用用户自定义的 Iterate()。

			if(m_bQuitOnIterateFail && !bOK)  // 若设置了“迭代失败退出”，则在失败时立即返回。
				return false;  // 返回 false 让外层 Run() 退出。
        }
        
        m_nIterateCount++;  // 递增迭代计数器。
    }
    

    


    return true;  // 当前轮处理成功结束，继续下一轮循环。
    
}

bool CMOOSApp::SetIterateMode(IterateMode Mode)
{
	if(!m_Comms.IsAsynchronous() && Mode!=REGULAR_ITERATE_AND_MAIL)
	{
		std::cerr<<"can only set iterate mode to REGULAR_ITERATE_AND_MAIL"
				" for old MOOS Clients\n"<<Mode;
		m_IterationMode = REGULAR_ITERATE_AND_MAIL;
		return false;
	}

	m_IterationMode =Mode;
	return true;


}

void CMOOSApp::SleepAsRequired(bool &  bIterateShouldRun)
{

	bIterateShouldRun = true;

	//do we need to sleep at all?
	if(m_dfFreq<=0.0)
	{
		//no we are being told to go flat out
		return;
	}

	//first thing we do is look to see how long we need to sleep in
	//vanilla case
	int nAppPeriod_ms = static_cast<int> (1000.0/m_dfFreq);

	//how long since we ran?
	int nElapsedTime_ms = static_cast<int> (1000.0*( MOOSLocalTime() - m_dfLastRunTime));

	if (nElapsedTime_ms < 0){
		nElapsedTime_ms = 0;
	}

	//so how long do we need to pause for
	int nSleep = (nAppPeriod_ms - nElapsedTime_ms);


	if(nSleep<1){
		//std::cerr<<"no sleep for "<<nSleep<<"\n";
		return;//nothing to do...
	}

	if(!m_Comms.IsAsynchronous())
	{
		//we just always sleep;
		MOOSPause(nSleep);
		return;
	}

#ifdef ASYNCHRONOUS_CLIENT


	//OK, we are a modern client and we have three distinct behaviours
	switch(m_IterationMode)
	{
	case  REGULAR_ITERATE_AND_MAIL:
		//we always to sleep - this behaves like old MOOS did - AppTick governs it all
		//std::cerr<<"sleeping for "<<nSleep<<"\n";
		MOOSPause(nSleep);
		break;
	case  REGULAR_ITERATE_AND_COMMS_DRIVEN_MAIL:
		//On NewMail is called as often as is needed but iterate is only called
		//at AppTick rates.
		if(m_Comms.GetNumberOfUnreadMessages() || m_pMailEvent->tryWait(nSleep))
		{
			if(MOOSLocalTime()-m_dfLastRunTime<1.0/m_dfFreq)
			{
				//we have mail but we are in a mode where we don't have
				//to call Iterate
				bIterateShouldRun= false;
			}
		}
		break;
	case COMMS_DRIVEN_ITERATE_AND_MAIL:
		//both OnNewMail and Iterate will be called as fast as mail comes in up
		//to a limiting speed.
		if(m_Comms.GetNumberOfUnreadMessages()>0 || m_pMailEvent->tryWait(nSleep))
		{

			//we got woken by the arrival of mail.....
			//do we need to sleep some more to ensure we don't iterate
			//too fast?
			double dfTimeSinceRun = MOOSLocalTime()-m_dfLastRunTime;
			double dfMinPeriod = m_dfMaxAppTick> 0.0 ? 1.0/m_dfMaxAppTick : 0.0;
			if(dfTimeSinceRun<dfMinPeriod)
			{
				//yes we do..
				nSleep=static_cast<int> (1000*(dfMinPeriod-dfTimeSinceRun));
				if(nSleep>0)
				{
					MOOSPause(nSleep);
				}
			}
		}
		break;
	default:
		break;
	}

#endif

}


bool CMOOSApp::AddMessageRouteToActiveQueue(const std::string & sQueueName,
                    const std::string & sMsgName)
{
    if(!m_Comms.HasActiveQueue(sQueueName))
    {
        return false;
    }
    return m_Comms.AddMessageRouteToActiveQueue(sQueueName,sMsgName);

}

bool CMOOSApp::AddMessageRouteToActiveQueue(const std::string & sQueueName,
		const std::string & sMsgName,
		bool (*pfn)(CMOOSMsg &M, void * pYourParam),
		void * pYourParam )
{
	//first do we have that queue made already?
	if(!m_Comms.HasActiveQueue(sQueueName))
	{
		//no we had better make one...
		m_Comms.AddActiveQueue(sQueueName,pfn,pYourParam);
	}
	//finally make sure this message routes to this message.
	return m_Comms.AddMessageRouteToActiveQueue(sQueueName,sMsgName);
}

bool CMOOSApp::AddActiveMessageQueueCallback(const std::string & sQueueName,
    		const std::string & sMsgName,
    		bool (*pfn)(CMOOSMsg &M, void * pYourParam),
    		void * pYourParam )
{
	return AddMessageRouteToActiveQueue(sQueueName,sMsgName,pfn,pYourParam);
}



bool CMOOSApp::AddMessageRouteToOnMessage(const std::string & sMsgName)
{
	return AddMessageRouteToActiveQueue(sMsgName+"_CB",sMsgName,MOOSAPP_OnMessage,this);
}
//deprecated version
bool CMOOSApp::AddMessageCallback(const std::string & sMsgName)
{
	return AddMessageRouteToOnMessage(sMsgName);
}



void CMOOSApp::SetServer(const char *sServerHost, long lPort)
{
    m_sServerHost = sServerHost;
    m_lServerPort = lPort;
    m_bServerSet = true;
}

bool CMOOSApp::CheckSetUp()
{
    if(m_sServerHost.empty())
    {
        MOOSTrace("MOOS Server host not specified\n");
        return false;
    }

    if(m_lServerPort==0)
    {
        MOOSTrace("MOOS Server port not specified\n");
        return false;
    }

    return true;
}


void CMOOSApp::SetAppError(bool bErr, const std::string & sErr)
{
    m_bAppError = bErr;
    m_sAppError =  m_bAppError ? sErr : "";
}

void CMOOSApp::SetCommandLineParameters(int argc,  char * argv[])
{
	m_CommandLineParser.Open(argc,argv);
}



bool CMOOSApp::UseMailCallBack()
{
    /* This attaches a callback to thw comms object's mail handler*/
    m_Comms.SetOnMailCallBack(MOOSAPP_OnMail,this);
    return true;
}

////////////////////// DEFAULT HANDLERS //////////////////////
bool CMOOSApp::OnNewMail(MOOSMSG_LIST &)
{
    return true;
}

bool CMOOSApp::Iterate()
{
    if(m_nIterateCount==0)
    {
        MOOSTrace("Warning default Iterate handler invoked...doing nothing\n");
    }
    return true;
}

bool CMOOSApp::OnConnectToServer()
{
    MOOSTrace("- default OnConnectToServer called\n");
    return true;
}

bool CMOOSApp::OnMessage(CMOOSMsg &M)
{
	MOOSTrace("- default handler for %s invoked. Did you mean to write your own?",M.GetKey().c_str());
	return true;
}


bool CMOOSApp::OnDisconnectFromServer()
{
//    MOOSTrace("- default OnDisconnectFromServer called\n");
    return true;
}


/* this block of functions simply notifies the DB that a string variable has changed*/
bool CMOOSApp::Notify(const std::string &sVar, const std::string & sVal, double dfTime)
{
	return m_Comms.Notify(sVar,sVal,dfTime);
}

bool CMOOSApp::Notify(const std::string &sVar, const std::string & sVal, const std::string & sSrcAux, double dfTime)
{
	return m_Comms.Notify(sVar,sVal,sSrcAux,dfTime);
}

bool CMOOSApp::Notify(const std::string &sVar, const char * sVal,double dfTime)
{
	return m_Comms.Notify(sVar,sVal,dfTime);
}

bool CMOOSApp::Notify(const std::string &sVar, const char * sVal,const std::string & sSrcAux, double dfTime)
{
	return m_Comms.Notify(sVar,sVal,sSrcAux,dfTime);
}


/** notify the MOOS community that something has changed (double)*/
bool CMOOSApp::Notify(const std::string & sVar,double dfVal, double dfTime)
{
	return m_Comms.Notify(sVar,dfVal,dfTime);
}
bool CMOOSApp::Notify(const std::string & sVar,double dfVal, const std::string & sSrcAux,double dfTime)
{
	return m_Comms.Notify(sVar,dfVal,sSrcAux,dfTime);
}


/** notify the MOOS community that something has changed binary data*/
bool CMOOSApp::Notify(const std::string & sVar,void *  pData, unsigned int nDataSize, double dfTime)
{
	return m_Comms.Notify(sVar,pData,nDataSize,dfTime);
}
bool CMOOSApp::Notify(const std::string & sVar,void *  pData, unsigned int nDataSize, const std::string & sSrcAux,double dfTime)
{
	return m_Comms.Notify(sVar,pData,nDataSize,sSrcAux,dfTime);
}

bool CMOOSApp::Notify(const std::string & sVar,const std::vector<unsigned char> & vData, double dfTime)
{
	return m_Comms.Notify(sVar,vData,dfTime);
}

bool CMOOSApp::Notify(const std::string & sVar,const std::vector<unsigned char> & vData,const std::string & sSrcAux, double dfTime)
{
	return m_Comms.Notify(sVar,vData,sSrcAux,dfTime);
}



/** Register for notification in changes of named variable*/
bool CMOOSApp::Register(const std::string & sVar,double dfInterval)
{
	return m_Comms.Register(sVar,dfInterval);
}

/** Register with wildcards based on application and variable patterns (* and ? allowed)*/
bool CMOOSApp::Register(const std::string & sVarPattern,const std::string & sAppPattern, double dfInterval)
{
	return m_Comms.Register(sVarPattern,sAppPattern,dfInterval);
}



/** UnRegister for notification in changes of named variable*/
bool CMOOSApp::UnRegister(const std::string & sVar)
{
	return m_Comms.UnRegister(sVar);
}




/** this is a call back from MOOSComms and its use is specialised (not for general consumption)*/
bool CMOOSApp::OnMailCallBack()
{
#ifdef ASYNCHRONOUS_CLIENT
    m_pMailEvent->set();
    return true;
#else
    std::cerr<<"OnMailCallback is deprecated for Asynchronous Clients\n";
    return false;
#endif
}

bool CMOOSApp::OnCommandMsg(CMOOSMsg /*CmdMsg*/)
{
    MOOSTrace("- default OnCommandMsg called\n");
    return true;
}


bool CMOOSApp::ConfigureComms()
{
    m_sServerHost = "LOCALHOST"; // 设置 MOOSDB 主机默认地址。
    if(!m_CommandLineParser.GetVariable("--moos_host",m_sServerHost)) // 优先读取命令行中的服务器地址。
	{
        if(!m_MissionReader.GetValue("SERVERHOST",m_sServerHost)) // 命令行没有提供时，再从任务文件读取 SERVERHOST。
		{
            // 保留默认的 LOCALHOST；未配置服务器地址不是致命错误。
		}
	}

    m_lServerPort = 9000; // 设置 MOOSDB 默认端口。
    if(!m_CommandLineParser.GetVariable("--moos_port",m_lServerPort)) // 优先读取命令行中的服务器端口。
	{
        if(!m_MissionReader.GetValue("SERVERPORT",m_lServerPort)) // 命令行没有提供时，再从任务文件读取 SERVERPORT。
		{
            // 保留默认的 9000 端口；未配置端口不是致命错误。
		}
	}

    std::stringstream ss; // 创建字符串流，用于将数值端口转换为字符串。
    ss<<m_lServerPort; // 将端口号写入字符串流。
    m_sServerPort = ss.str(); // 保存端口号的字符串形式，兼容旧接口或旧代码。

    if(!CheckSetUp()) // 检查服务器地址和端口是否满足基本启动条件。
        return false; // 配置不完整时停止通信初始化。


    m_MissionReader.GetConfigurationParam("COMMSTICK",m_nCommsFreq); // 从任务文件读取通信线程频率。
    m_CommandLineParser.GetOption("--moos_comms_tick",m_nCommsFreq); // 命令行参数覆盖任务文件中的通信频率。
    m_nCommsFreq = m_nCommsFreq <0 ? 1 : m_nCommsFreq; // 将负数频率纠正为最小值 1，避免无效配置。

    m_Comms.SetOnConnectCallBack(MOOSAPP_OnConnect,this); // 注册连接成功回调，并传入当前 CMOOSApp 对象指针。
    
    m_Comms.SetOnDisconnectCallBack(MOOSAPP_OnDisconnect,this); // 注册断开连接回调，并传入当前 CMOOSApp 对象指针。

    if(m_sMOOSName.empty()) // 如果没有单独设置 MOOS 注册名，则使用应用名。
        m_sMOOSName = m_sAppName; // 确保客户端连接时拥有有效的注册名。
    
    m_Comms.Run(m_sServerHost.c_str(),m_lServerPort,m_sMOOSName.c_str(),m_nCommsFreq); // 启动通信客户端线程，异步连接 MOOSDB。

    return true; // 通信配置和启动请求完成。
}

/** over load this if you want to do something fancy at statup...*/
bool CMOOSApp::OnStartUp()
{
    return true;
}

int CMOOSApp::GetIterateCount()
{
    return m_nIterateCount;
}

double CMOOSApp::GetAppStartTime()
{
    return m_dfAppStartTime;
}

/** return the application frequency*/
double CMOOSApp::GetAppFreq()
{
	return m_dfFreq;
}

/** get the comms frequency*/
unsigned int CMOOSApp::GetCommsFreq()
{
	return m_nCommsFreq;
}


void CMOOSApp::SetAppFreq(double  dfFreq,double dfMaxFreq)
{
    if(m_dfFreq<=MOOS_MAX_APP_FREQ)
    {
        m_dfFreq = dfFreq;
    }
    else
    {
    	std::cout<<"Setting baseline apptick to allowable maximum of "<<MOOS_MAX_APP_FREQ<<std::endl;
    }

    if(dfMaxFreq>=0.0)
    {
    	m_dfMaxAppTick = dfMaxFreq;
    }
}

bool CMOOSApp::SetCommsFreq(unsigned int nFreq)
{
    if(nFreq<=MOOS_MAX_COMMS_FREQ)
    {
        m_nCommsFreq = nFreq;
        return m_Comms.SetCommsTick(m_nCommsFreq);
    }
    return false;
}

bool CMOOSApp::IsSimulateMode()
{
    return m_bSimMode;
}

void CMOOSApp::WaitForEmptyOutbox()
{
	while(m_Comms.GetNumberOfUnsentMessages())
	{
		MOOSPause(100);
	}
}


bool CMOOSApp::AddMOOSVariable(string sName, string sSubscribeName, string sPublishName,double dfCommsTime)
{
    CMOOSVariable NewVar(sName,sSubscribeName,sPublishName,dfCommsTime);

    //does it already exist?
    if(m_MOOSVars.find(sName)!=m_MOOSVars.end())
        return false;

    m_MOOSVars[sName] = NewVar;

    return true;
}

//this function publishes all the fresh variables belong to the
//apllication. Useful in many sensor applications!
bool CMOOSApp::PublishFreshMOOSVariables()
{
    MOOSVARMAP::iterator p;

    for(p = m_MOOSVars.begin();p!=m_MOOSVars.end();++p)
    {
        CMOOSVariable & Var = p->second;
        if(Var.IsFresh())
        {
            if(Var.IsDouble())
            {
                m_Comms.Notify(Var.GetPublishName(),Var.GetDoubleVal(),Var.GetTime());
            }
            else
            {
                m_Comms.Notify(Var.GetPublishName(),Var.GetStringVal(),Var.GetTime());
            }

            Var.SetFresh(false);
        }
    }

    return true;
}

bool CMOOSApp::SetMOOSVar(const string &sVarName, double dfVal, double dfTime)
{
    MOOSVARMAP::iterator p = m_MOOSVars.find(sVarName);

    if(p==m_MOOSVars.end())
        return false;

    CMOOSVariable & rVar = p->second;

    return rVar.Set(dfVal,dfTime);
}

bool CMOOSApp::SetMOOSVar(const string &sVarName,const  string &sVal, double dfTime)
{
    MOOSVARMAP::iterator p = m_MOOSVars.find(sVarName);

    if(p==m_MOOSVars.end())
        return false;

    CMOOSVariable  & rVar = p->second;

    return rVar.Set(sVal,dfTime);


}

bool CMOOSApp::SetMOOSVar(const CMOOSVariable& Var)
{
    MOOSVARMAP::iterator p = m_MOOSVars.find(Var.GetName ());
	
    if(p==m_MOOSVars.end())
        return false;
	
    p->second = Var;
    p->second.SetFresh (true);
    return true;
}

bool CMOOSApp::UpdateMOOSVariables(MOOSMSG_LIST &NewMail)
{
    //we only subscribe to things if we are in simulator mode
    MOOSVARMAP::iterator p;
    double dfTimeNow = MOOSTime();
    for(p = m_MOOSVars.begin();p!=m_MOOSVars.end();++p)
    {
        CMOOSVariable & rVar = p->second;
        CMOOSMsg Msg;
        if(m_Comms.PeekMail(NewMail,rVar.GetSubscribeName(),Msg))
        {
            if(!Msg.IsSkewed(dfTimeNow))
            {
                rVar.Set(Msg);
                rVar.SetFresh(true);
            }
        }
    }

    return true;
}

bool CMOOSApp::RegisterMOOSVariables()
{
    bool bSuccess = true;

    MOOSVARMAP::iterator p;

    for(p = m_MOOSVars.begin();p!=m_MOOSVars.end();++p)
    {
        CMOOSVariable & rVar = p->second;
        if(!rVar.GetSubscribeName().empty())
        {
            double dfCommsTime = rVar.GetCommsTime();
            if(dfCommsTime<0)
            {
                dfCommsTime = 0;
            }

            bSuccess &= m_Comms.Register(rVar.GetSubscribeName(),dfCommsTime);
        }
    }

    return bSuccess;
}

bool CMOOSApp::MOOSDebugWrite(const string &sTxt)
{
    if(m_Comms.IsConnected())
    {
        MOOSTrace(" %s says: %s\n",GetAppName().c_str(),sTxt.c_str());
        return m_Comms.Notify("MOOS_DEBUG",sTxt);
    }
    else
    {
        return false;
    }
}

bool CMOOSApp::CanIterateWithoutComms()
{
    return m_bIterateWithoutComms;
}

void CMOOSApp::EnableIterateWithoutComms(bool bEnable)
{
    m_bIterateWithoutComms = bEnable;
}



double CMOOSApp::GetTimeSinceIterate()
{
    return MOOSLocalTime()-m_dfLastRunTime;
}

double CMOOSApp::GetLastIterateTime()
{
    return m_dfLastRunTime;
}


std::string CMOOSApp::GetMissionFileName()
{
    return m_sMissionFile;
}

string CMOOSApp::GetAppName()
{
    return m_sAppName;
}

bool CMOOSApp::UseMOOSComms(bool bUse)
{
    m_bUseMOOSComms = bUse;

    return true;
}

CMOOSVariable * CMOOSApp::GetMOOSVar(string sName)
{
    MOOSVARMAP::iterator p =  m_MOOSVars.find(sName);
    if(p==m_MOOSVars.end())
    {
        return NULL;
    }
    else
    {
        return &(p->second);
    }
}


std::string CMOOSApp::GetCommandKey()
{
    std::string sCommandKey = GetAppName()+"_CMD";
    MOOSToUpper(sCommandKey);
    return sCommandKey;
}

bool CMOOSApp::LookForAndHandleAppCommand(MOOSMSG_LIST & NewMail)
{
    MOOSMSG_LIST::iterator q;
    bool bResult = true;
    for(q=NewMail.begin();q!=NewMail.end();++q)
    {
        if(MOOSStrCmp(q->GetKey(),GetCommandKey()))
        {
            //give a derived class a chance to respond
            bResult&= OnCommandMsg(*q);
        }
    }
    return bResult;
}


void CMOOSApp::EnableCommandMessageFiltering(bool bEnable)
{
    m_bCommandMessageFiltering = bEnable;
    if(bEnable)
    {
        //we had better register for the message
        m_Comms.Register(GetCommandKey(),0);
    }
    else
    {
        //we are no longer interested
        m_Comms.UnRegister(GetCommandKey());
    }
}

double CMOOSApp::GetCPULoad()
{
	double dfLoad=0;
	m_ProcessMonitor.GetPercentageCPULoad(dfLoad);
	return dfLoad;
}

std::string CMOOSApp::MakeStatusString()
{
    std::set<std::string> Published = m_Comms.GetPublished();
    std::set<std::string> Registered = m_Comms.GetRegistered();

    std::stringstream ssStatus;
    ssStatus<<"AppErrorFlag="<<(m_bAppError?"true":"false")<<",";
    if(m_bAppError)
        ssStatus<<"AppErrorReason="<<m_sAppError<<",";
    
    ssStatus<<"Uptime="<<MOOSTime()-GetAppStartTime()<<",";

    double dfCPULoad;
    if(m_ProcessMonitor.GetPercentageCPULoad(dfCPULoad))
    {
    	ssStatus<<"cpuload="<<std::setprecision(4)<<dfCPULoad<<",";
    }

    size_t memory_now, memory_max;
    if(m_ProcessMonitor.GetMemoryUsage(memory_now,memory_max))
    {
    	ssStatus<<"memory_kb="<<std::setprecision(4)<<memory_now/(1024.0)<<",";
    	ssStatus<<"memory_max_kb="<<std::setprecision(4)<<memory_max/(1024.0)<<",";
    }


    ssStatus<<"MOOSName="<<GetAppName()<<",";

    ssStatus<<"Publishing=\"";
    std::copy(Published.begin(),Published.end(),std::ostream_iterator<string>(ssStatus,","));
	ssStatus<<"\",";

    ssStatus<<"Subscribing=\"";
    std::copy(Registered.begin(),Registered.end(),std::ostream_iterator<string>(ssStatus,","));
	ssStatus<<"\"";

    return ssStatus.str();
}

void CMOOSApp::OnDisconnectToServerPrivate()
{
    return;
}

//called just before calling a derived classes OnConnectToServer()
void CMOOSApp::OnConnectToServerPrivate()
{
    if(m_bCommandMessageFiltering)
    {
        m_Comms.Register(GetCommandKey(),0);
    }
}

/** here we do our private mail processing*/
void CMOOSApp::OnNewMailPrivate(MOOSMSG_LIST & Mail)
{
    //look to handle a command string
    if(m_bCommandMessageFiltering)
        LookForAndHandleAppCommand(Mail);
}

void CMOOSApp::IteratePrivate()
{
    if(fabs(m_dfLastStatusTime-MOOSTime())>STATUS_PERIOD)  // 若距上次状态发布时间超过 STATUS_PERIOD 秒，则生成并发布新的状态。
    {  // 开始执行本次状态上报。
        std::string sStatus = MOOSToUpper(GetAppName())+"_STATUS";  // 取应用名并转成大写，再追加 _STATUS 作为状态变量名。
        MOOSToUpper(sStatus);  // 确保整个状态变量名均为大写。
        m_Comms.Notify(sStatus,MakeStatusString());  // 生成状态文本并通过 MOOS 通信发布到数据库。
        m_dfLastStatusTime = MOOSTime();  // 记录本次发布时间，供下次判断上报间隔。
    }  // 状态上报判断结束；未到周期时本函数不做其他操作。
}
