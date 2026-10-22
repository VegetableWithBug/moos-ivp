#include "Antler.h"

using namespace std;
#include <sstream>
#include <iostream>
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>




#define DEBUG_LAUNCH 0
CAntler::CAntler()
{

	
	m_JobLock.UnLock();    
    m_bNewJob = false;
    m_sAntlerName = "Monarch";
    m_sDBHost = "localhost";
    m_nDBPort = 9000;
    m_bQuitCurrentJob = false;
	m_eVerbosity=CHATTY;
}

//this is the vanilla version of Run - called to run from a single mission file
bool CAntler::Run(const std::string &  sMissionFile,std::set<std::string> Filter )
{
    m_bHeadless = false;
    m_sMissionFile = sMissionFile;
    m_Filter = Filter;
    return Spawn(m_sMissionFile);
}

//this version will wait for a mission fiel to be sent via a DB
bool CAntler::Run(const std::string & sHost,  int nPort, const std::string & sAntlerName)
{
    //this is more interesting...
    m_sAntlerName = sAntlerName;
    m_sDBHost = sHost;
    m_nDBPort = nPort;
    m_bHeadless = true;
    
    
    if(!ConfigureMOOSComms())
        return true;
        
    
    MOOSTrace("   This is headless Antler called \"%s\"\n   Waiting for mission file from %s:%d\n",m_sAntlerName.c_str(),m_sDBHost.c_str(),m_nDBPort);
    
       
	const char * sSpin = "-\\|/";
    while(1)
    {
        //wait to be signalled that there is work to do...
        int i = 0;
        while(!m_bNewJob)
        {
            MOOSPause(500);
            MOOSTrace("   Speak to me Monarch....%c\r",sSpin[i++%3]);
        }
            
        //no more launching until this community is complete
        m_JobLock.Lock();
        Spawn(m_sReceivedMissionFile,true);
        m_bNewJob = false;
        m_JobLock.UnLock();
        
    }
}



bool CAntler::DoRemoteControl()
{
    
    
	    
    while(1)
    {
        MOOSPause(100);
        if(!m_pMOOSComms->IsConnected())
            continue;
        
        //better check mail
        MOOSMSG_LIST NewMail;
        if(m_pMOOSComms->Fetch(NewMail))
        {
            CMOOSMsg Msg;
            if(m_bHeadless)
            {
                
                if(m_pMOOSComms->PeekMail(NewMail,"MISSION_FILE",Msg))
                {
                    MOOSTrace("\n|***** Dynamic Brief *****|\n\n");
                    
                    //make a new file name
                    m_sReceivedMissionFile = MOOSFormat("runtime_%s.moos",MOOSGetTimeStampString().c_str());   
                    
                    MOOSTrace("   %s received [%d bytes]\n",m_sReceivedMissionFile.c_str(),Msg.GetString().size());
                    MOOSTrace("   shutting down all current spawned processes:\n");
                    
                    //tell the current job to quit
                    m_bQuitCurrentJob = true;
                    
                    //wait for that to happen
                    m_JobLock.Lock();        
                    
                    //here we copy the mission file contained in the message to 
                    std::stringstream ss(Msg.GetString());
                    
                    //suck out the Antler filter line
                    std::string sFilter;
                    std::getline(ss, sFilter);
					MOOSTrace("%s\n", sFilter.c_str());
                    MOOSChomp(sFilter,"ANTLERFILTER:", true);
                    std::stringstream ssF(sFilter);
                    
                    //fill in the filter set
                    std::copy(istream_iterator<std::string>(ssF), 
                              istream_iterator<string>(),
                              std::inserter(m_Filter,m_Filter.begin()));
                    
                    
                    //write out the whole file
                    std::ofstream Out(m_sReceivedMissionFile.c_str());
                    if(!Out.is_open())
                    {
                        m_JobLock.UnLock();
                        return MOOSFail("failed to open mission file for writing");
                    }
                    
                    //you've gotta lurve C++ ...
                    Out<<ss.rdbuf();
                    
                    Out.close();
                    
                    //we no longer want the current job to quit (it already has)
                    m_bQuitCurrentJob = false;
                    
                    //signal that we have more work to do
                    m_bNewJob =true;
                    
                    //let thread 0 continue
                    m_JobLock.UnLock();
                    
                    
                }
            }
            else
            {
                if(m_pMOOSComms->PeekAndCheckMail(NewMail, "ANTLER_STATUS", Msg))
                {
                    std::string sWhat;
                    MOOSValFromString(sWhat, Msg.GetString(),"Action");
                    std::string sProc;
                    MOOSValFromString(sProc, Msg.GetString(),"Process");
                    std::string sID;
                    MOOSValFromString(sID, Msg.GetString(), "AntlerID");
                    
                    MOOSTrace("   [rmt] Process %-15s has %s (by %s)\n",sProc.c_str(),sWhat.c_str(),sID.c_str());
                }    
            }
            
        }
    }        
}

bool CAntler::SetVerbosity(VERBOSITY_LEVEL eLevel)
{
	m_eVerbosity = eLevel;
	switch(eLevel)
	{
		case CHATTY:
			break;
		case TERSE:
			InhibitMOOSTraceInThisThread(true);
			break;
		case QUIET:
			InhibitMOOSTraceInThisThread(true);
			if(m_pMOOSComms!=NULL)
				m_pMOOSComms->SetQuiet(true);
			
	}
	return true;
}

bool CAntler::ConfigureMOOSComms()
{
    
	
    
    //start a monitoring thread
    m_RemoteControlThread.Initialise(_RemoteControlCB, this);
    m_RemoteControlThread.Start();
    
    
    m_pMOOSComms = new CMOOSCommClient;
    m_pMOOSComms->SetOnConnectCallBack(_MOOSConnectCB,this);
	m_pMOOSComms->SetOnDisconnectCallBack(_MOOSDisconnectCB,this);
    m_pMOOSComms->SetQuiet(true);
    
    std::string sMe =MOOSFormat("Antler{%s}",m_sAntlerName.c_str());
    
    //try and connect to a DB
    if(!m_pMOOSComms->Run(m_sDBHost.c_str(), (long int)m_nDBPort, sMe.c_str(), 1))
        return MOOSFail("could not set up MOOSComms\n");
    
      
    return true;
}



bool CAntler::SendMissionFile( )
{
    MOOSTrace("\n\n|***** Propagate *****|\n\n");
    CMOOSFileReader FR;
    FR.SetFile(m_sMissionFile);
    std::stringstream ss;

    //copy the filters in
    ss<<"ANTLERFILTER:";
    std::copy (m_Filter.begin(), m_Filter.end(), ostream_iterator <std::string> (ss, " "));
    ss<<std::endl;
    
    while(!FR.eof())
    {
        std::string sL = FR.GetNextValidLine()+"\n";
        ss<<sL;
    }
    m_pMOOSComms->Notify("MISSION_FILE",ss.str());
    
    
    MOOSTrace("   Monarch published thinned mission file [%d bytes]\n\n",ss.str().size());
    return true;
    
}
bool CAntler::OnMOOSConnect()
{
    if(m_bHeadless)
    {
        MOOSTrace("  Connecting to a DB\n");    
        m_pMOOSComms->Register("MISSION_FILE",0);
      
    }
    else
    {
        m_pMOOSComms->Register("ANTLER_STATUS",0);
        SendMissionFile();
    }
    return true;
}
bool CAntler::OnMOOSDisconnect()
{
    if(m_bHeadless)
    {
        
        MOOSTrace("   DB Connection Lost\n");    
        
        if(m_bKillOnDBDisconnect)
        {
            //look likes the monarch is dead.....
            MOOSTrace("   shutting down all current spawned processes:\n");
            
            //tell the current job to quit
            m_bQuitCurrentJob = true;
            
            //wait for that to happen
            m_JobLock.Lock();        
            m_JobLock.UnLock();        
        }
       
    }
    return true;
}

bool CAntler::PublishProcessQuit(const std::string & sProc)
{
	if(!m_bHeadless)
        return false;
    
    if(!m_pMOOSComms->IsConnected())
        return false;

    m_pMOOSComms->Notify("ANTLER_STATUS",MOOSFormat("Action=Quit,Process=%s,AntlerID=%s",sProc.c_str(),m_sAntlerName.c_str()));
    
    
    return true;
}

bool CAntler::PublishProcessLaunch(const std::string & sProc)
{
	if(!m_bHeadless)
        return false;
    
    if(!m_pMOOSComms->IsConnected())
        return false;
    
    m_pMOOSComms->Notify("ANTLER_STATUS",MOOSFormat("Action=Launched,Process=%s,AntlerID=%s",sProc.c_str(),m_sAntlerName.c_str()));
    
    return true;
}


bool CAntler::KillNicely(MOOSProc* pProc)
{
#ifndef _WIN32
		
	if(m_bSupportGentleKill)
	{
		if(pProc->m_bNewConsole)
		{
			//we need to be crafty....
			std::string sCmd = "ps -e -o ppid= -o pid=";
			
			FILE* In = popen(sCmd.c_str(),"r");
			
			if(In!=NULL)
			{
				bool bFound = false;
				char Line[256];
				while(fgets(Line,sizeof(Line),In))
				{
					std::stringstream L(Line);
					int ppid,pid;
					L>>ppid;
					L>>pid;
					
					if(pProc->m_ChildPID==ppid)
					{
						kill(pid,SIGTERM);
						bFound = true;
					}
				}	
				pclose(In);
				return bFound;
			}
			else
			{
				return false;
			}
		}
		else
		{
			//the proc ID we have is of the actuall MOOSProcess and not its parent
			//we can just kill it...
			kill(pProc->m_ChildPID,SIGTERM);
		}
	}
	else
	{
		kill(pProc->m_ChildPID,SIGTERM);
	}
#else
	//MOOSTrace("Warning - gentle killing of win32 processes is not implemented\n");
	pProc->pWin32Proc->vTerminate();   
    return true;

#endif
	return true;
}


bool CAntler::Spawn(const std::string &  sMissionFile, bool bHeadless)
{
    
    MOOSTrace("\n\n|****** Launch ******|\n\n"); // 输出本次任务启动的标题。
    m_nCurrentLaunch = 0; // 重置当前已启动进程计数。
    
    
    //set up the mission file reader
    if(!m_MissionReader.SetFile(sMissionFile)) // 尝试打开并解析指定的 MOOS 任务文件。
        return MOOSFail("error reading mission file\n"); // 任务文件读取失败时报告错误并结束。
    
    
    m_MissionReader.SetAppName("ANTLER"); // 设置当前配置读取上下文为 ANTLER 配置块。
    
    STRING_LIST      sParams; // 保存 ANTLER 配置块中的每一行配置。
    
    if(!m_MissionReader.GetConfiguration(  m_MissionReader.GetAppName(),sParams)) // 读取任务文件中的 ANTLER 配置块。
        return MOOSFail("error reading antler config block from mission file\n"); // 配置块读取失败时报告错误并结束。
    
    
    //fetch all the lines in teg Antler configuration block
    STRING_LIST::iterator p; // 声明配置行迭代器。
    sParams.reverse(); // 反转配置顺序，使进程按任务文件期望的顺序启动。
    
    int nTimeMSBetweenSpawn=DEFAULTTIMEBETWEENSPAWN; // 设置进程启动之间的默认间隔。
    m_MissionReader.GetConfigurationParam("MSBetweenLaunches",nTimeMSBetweenSpawn); // 读取任务文件指定的启动间隔。
    
    //here we'll figure out a what paths to use when looking for  executables
    m_sDefaultExecutablePath = "SYSTEMPATH"; // 默认要求从系统 PATH 查找可执行程序。
    m_MissionReader.GetConfigurationParam("ExecutablePath",m_sDefaultExecutablePath); // 读取任务文件指定的统一可执行文件目录。
    
    if(!MOOSStrCmp("SYSTEMPATH",m_sDefaultExecutablePath)) // 如果配置的不是 SYSTEMPATH，则使用自定义目录。
    {
        //MOOSTrace("\"ExecutablePath\" is %s\n",m_sDefaultExecutablePath.c_str());
        if(*m_sDefaultExecutablePath.rbegin()!='/') // 检查自定义目录末尾是否已经有路径分隔符。
        {
            //look to add extra / if needed
            m_sDefaultExecutablePath+='/'; // 补充路径分隔符，便于后续拼接程序名。
        }
        
    }
    else
    {
        MOOSTrace("Unless directed otherwise using system path to locate binaries \n");
        m_sDefaultExecutablePath=""; // 使用系统 PATH 时不再给程序名前面拼接固定目录。
    }
    
	
	//are we being ask to support gentle process killing?
#ifdef _WIN32
    m_bSupportGentleKill= false; // Windows 版本默认关闭优雅终止功能。
#else
    m_bSupportGentleKill = true; // Unix 版本默认启用优雅终止功能。
    m_MissionReader.GetConfigurationParam("GentleKill",m_bSupportGentleKill); // 读取任务文件对优雅终止功能的覆盖设置。
#endif
	
	
	
	//no cycle through each line in the configuration block. If it begins with run then it means launch
    for(p = sParams.begin();p!=sParams.end();p++) // 依次处理 ANTLER 配置块中的每一行。
    {
        std::string sLine = *p; // 复制当前配置行，避免直接修改配置列表内容。
        
        std::string sWhat = MOOSChomp(sLine,"="); // 取出等号前的指令名，并让 sLine 保留指令参数。
        
        if(MOOSStrCmp(sWhat,"RUN")) // 只处理 RUN 指令，其他配置项在这里跳过。
        {
            //OK we are being asked to run a process
            
            //try to create a process
            MOOSProc* pNew  = CreateMOOSProcess(sLine); // 根据 RUN 参数解析并启动一个 MOOS 进程。
            
            if(pNew!=NULL)
            {
				//this a really important bit of text most folk will want to see it...
                InhibitMOOSTraceInThisThread(m_eVerbosity==QUIET); // 根据静默级别暂时控制启动成功信息的输出。
				{
                    MOOSTrace("   [%.3d] Process: %-15s ~ %-15s launched successfully\n", // 输出已启动进程的序号、可执行文件和 MOOS 名称。
							  m_nCurrentLaunch,
							  pNew->m_sApp.c_str(),
							  pNew->m_sMOOSName.c_str());
				}
                InhibitMOOSTraceInThisThread(m_eVerbosity!=CHATTY); // 恢复原有输出抑制状态。
				
                m_ProcList.push_front(pNew); // 将新进程记录到运行中进程列表。
				m_nCurrentLaunch++; // 更新当前已启动进程数量。
                PublishProcessLaunch(pNew->m_sApp); // 在分布式模式下发布进程启动状态。
            }
            
	        //wait a while
            MOOSPause(nTimeMSBetweenSpawn); // 按配置暂停，避免连续启动进程造成资源竞争。
            
        }
    }
    
    
    
    if(bHeadless==false) // 非无界面模式下，当前 Antler 可能作为分布式启动主节点。
    {
        bool bMaster=false; // 保存是否启用分布式 Antler 的配置。
        m_MissionReader.GetConfigurationParam("EnableDistributed",bMaster); // 读取分布式启动开关。
        if(bMaster) // 如果当前实例是分布式启动主节点。
        {
            
            m_MissionReader.GetValue("ServerHost",m_sDBHost); // 读取用于分布式通信的数据库主机地址。
            m_MissionReader.GetValue("ServerPort",m_nDBPort); // 读取用于分布式通信的数据库端口。
            
            
            if(!ConfigureMOOSComms()) // 建立 Antler 与 MOOSDB 之间的通信并发布任务信息。
                return MOOSFail("failed to start MOOS comms"); // 通信初始化失败时结束。
            
        }
    }
    else
    {
        m_bKillOnDBDisconnect = true; // 无界面模式默认在数据库断开时终止子进程。
        m_MissionReader.GetConfigurationParam("KillOnDBDisconnect",m_bKillOnDBDisconnect); // 读取任务文件对该行为的覆盖设置。
    }
    

    
    //now wait on all our processes to close....
    while(m_ProcList.size()!=0) // 只要仍有子进程未退出，就持续监视并回收它们。
    {
        MOOSPROC_LIST::iterator q; // 声明运行中进程列表迭代器。
        
        for(q = m_ProcList.begin();q!=m_ProcList.end();q++) // 遍历当前所有已启动进程。
        {
            MOOSProc * pMOOSProc = *q; // 取得当前进程信息。
            
#ifdef _WIN32
                if(m_bQuitCurrentJob) // 如果收到停止当前任务的请求，则终止当前进程。
            {
				KillNicely(pMOOSProc); // 使用平台相关方式请求进程优雅退出。
            }
            
            
            pMOOSProc->pWin32Proc->vWaitForTerminate(100); // 最多等待 100 毫秒检查 Windows 进程状态。
            if(    pMOOSProc->pWin32Proc->dwGetExitCode()!=STILL_ACTIVE) // 判断 Windows 子进程是否已经退出。
            {

				//this a really important bit of text most folk will want to see it...
                InhibitMOOSTraceInThisThread(m_eVerbosity==QUIET); // 根据输出级别控制退出信息显示。
				{
                    MOOSTrace("   [%.3d] Process: %-15s has quit\n", // 输出已退出进程的信息。
							  --m_nCurrentLaunch,
							  pMOOSProc->m_sApp.c_str());
				}
                InhibitMOOSTraceInThisThread(m_eVerbosity!=CHATTY); // 恢复原有输出抑制状态。

                
                PublishProcessQuit(pMOOSProc->m_sApp); // 在分布式模式下发布进程退出状态。
                delete pMOOSProc->pWin32Attrib; // 释放 Windows 进程属性对象。
                delete pMOOSProc->pWin32Proc; // 释放 Windows 进程对象。
                delete pMOOSProc; // 释放进程信息结构体。
                
                m_ProcList.erase(q); // 从运行中进程列表移除已退出进程。
                break; // 迭代器失效后跳出循环，下一轮重新获取迭代器。
            }
#else
            if(m_bQuitCurrentJob) // Unix 下如果当前任务需要停止，则终止子进程。
            {
                MOOSTrace("   actively killing running child %s\n",pMOOSProc->m_sApp.c_str()); // 输出正在终止的子进程名称。
				KillNicely(pMOOSProc); // 向子进程发送优雅终止信号。
                //just give it a little time - for pities sake - no need for this pause
                MOOSPause(300); // 给进程少量时间完成退出和清理。
            }
            
            
            int nStatus = 0; // 保存 waitpid 返回的子进程退出状态。
            if(waitpid(pMOOSProc->m_ChildPID,&nStatus,WNOHANG)>0) // 非阻塞检查子进程是否已退出。
            {
				
                InhibitMOOSTraceInThisThread(m_eVerbosity==QUIET); // 根据输出级别控制退出信息显示。
				{
                    MOOSTrace("   [%.3d] Process: %-15s has quit\n", // 输出已退出进程的信息。
							  --m_nCurrentLaunch,
							  pMOOSProc->m_sApp.c_str());
				}
                InhibitMOOSTraceInThisThread(m_eVerbosity!=CHATTY); // 恢复原有输出抑制状态。
				
                
                PublishProcessQuit(pMOOSProc->m_sApp); // 在分布式模式下发布进程退出状态。
                
                m_ProcList.erase(q); // 从运行中进程列表移除已退出进程。
                break; // 迭代器失效后跳出循环，下一轮重新获取迭代器。
            }
            MOOSPause(100); // 短暂等待，避免高频轮询占满 CPU。
#endif
            
        }
              
    }
    

    
    return 0; // 所有由 Antler 启动的进程均已退出，Spawn 正常完成。
    
}




bool CAntler::MakeExtraExecutableParameters(std::string sParam,STRING_LIST & ExtraCommandLineParameters,std::string sProcName,std::string sMOOSName)
{
    
    ExtraCommandLineParameters.clear();
    
    std::string sExtraParamsName;
    if(!MOOSValFromString(sExtraParamsName, sParam, "ExtraProcessParams", true))
        return true;//nothing to do
    
    std::string sExtraParams;
    
    //OK look for this configuration string
    if(!m_MissionReader.GetConfigurationParam(sExtraParamsName,sExtraParams))
        return MOOSFail("   warning cannot find extra parameters named \"%s\"\n",sExtraParamsName.c_str());
    
    while(!sExtraParams.empty())
        ExtraCommandLineParameters.push_back(MOOSChomp(sExtraParams,","));
    
    return true;
    
}

bool CAntler::MakeConsoleLaunchParams(std::string sParam,STRING_LIST & LaunchList,std::string sProcName,std::string sMOOSName)
{
    //sParam is a string in the Run=ProcName @ sParam ~ MOOSName
    std::string sLaunchConfigurationName;
    std::string sLaunchConfiguration;
#ifdef _WIN32
	bool bNIX = false;
	
#else
	bool bNIX= true; 
#endif
    
    
	std::string sLaunchKey = bNIX?"XConfig":"Win32Config"; 
    if(!MOOSValFromString(sLaunchConfigurationName, sParam, sLaunchKey))
    {
        //some applications are v.important in MOOS if not told otherwise they get special colours
		string sBgColor = bNIX? "#DDDDFF" : "";
		string sFgColor = bNIX? "#000000" : "";
        
        if(MOOSStrCmp(sProcName,"iRemote"))
        {
			sBgColor = bNIX ? "#CC0000" : "RED";
			sFgColor = bNIX ? "#FFFFFF" : "";
        }
        if(MOOSStrCmp(sProcName,"MOOSDB"))
        {
			sBgColor = bNIX? "#003300" : "BLUE";
			sFgColor = bNIX? "#FFFFFF" : "";
        }
        if(bNIX)
		{
			sLaunchConfiguration = "-geometry,"+MOOSFormat("80x12+2+%d",(m_nCurrentLaunch++)*50)+
			",+sb,"+
			",-fg,"+sFgColor+
			",-bg,"+sBgColor+
			",-T,"+MOOSFormat("%s %s",sProcName.c_str(),sMOOSName.empty()?"":MOOSFormat("as MOOSName \"%s\"",sMOOSName.c_str()).c_str());
		}
		else
		{
			sLaunchConfiguration = sBgColor;
		}
    }
    else
    {
        
        //OK look for this configuration string
        if(!m_MissionReader.GetConfigurationParam(sLaunchConfigurationName,sLaunchConfiguration))
			return MOOSFail("   warning: could not find resource string called \"%s\"",sLaunchConfigurationName.c_str()) ;
    }
    
    //OK now simply chomp our way through a space delimited list...
    while(!sLaunchConfiguration.empty())
    {
        MOOSTrimWhiteSpace(sLaunchConfiguration);
        std::string sP = MOOSChomp(sLaunchConfiguration,",");
        MOOSTrimWhiteSpace(sP);
        if(!sP.empty())
        	LaunchList.push_back(sP);
    }
    
    return !LaunchList.empty();
    
    
}



CAntler::MOOSProc* CAntler::CreateMOOSProcess(string sConfiguration)
{
    
    
    //what tis its name? (if no @ symbol we just get the name and no cmdline)
    string sProcName = MOOSChomp(sConfiguration,"@");
    
    //further parameters are to left left of @
    string sParam = sConfiguration;
    
    if(sProcName.empty())
    {
        MOOSTrace("no process specified - RUN=???\n");
        return NULL;
    }
    
    //std::string sFullProcName = m_sDefaultExecutablePath+sProcName;
    
    //if things go well this will eventually point to a new Process
    MOOSProc * pNewProc    = NULL;
    
	//look for tilde demarking end of param=val block
    string sOption = MOOSChomp(sParam,"~");
    
    
    
    bool bDistributed=false;
    m_MissionReader.GetConfigurationParam("EnableDistributed",bDistributed);

    if(bDistributed)
    {
               
        
        if(m_bHeadless)
        {
            //we are a drone
            std::string sAntlerRequired;
            if(!MOOSValFromString(sAntlerRequired, sOption, "AntlerID", true))
                return NULL; //this is for primary Antler
            
                        
            if(!MOOSStrCmp(sAntlerRequired, m_sAntlerName))
                return NULL; //for some other Antler
            
            //OK it is for us...
            //MOOSTrace("Headless Antler found a RUN directive...\n");
        }
        else
        {
            //we are a TopMOOS
            std::string sAntlerRequired;
            if(MOOSValFromString(sAntlerRequired, sOption, "AntlerID", true))
                return NULL; //this is for a drone
            
        }
    }
    else
    {
        //we run everything
    }
    
    //do we want a new console?
    bool bNewConsole = false;
	MOOSValFromString(bNewConsole, sOption, "NEWCONSOLE",true);
    
    //do we want to inhibit the passing of MOOS parameters (mission file and MOOSName)
    bool bInhibitMOOSParams = false;
    MOOSValFromString(bInhibitMOOSParams, sOption, "InhibitMOOSParams", true);
    
    //by default process are assumed to be on the system path
    //users can specify an alternative path for all process by setting "ExectutablePath=<Path>" in the mission file
    //configuration block.
	//user has the option of specifying paths individually process by process.
    //alternativelt they can specify that a particular process shouod be located by  system wide and not
    //in the default executable path
    std::string sSpecifiedPath;
    std::string sFullProcName=sProcName;
    if(MOOSValFromString(sSpecifiedPath, sOption, "PATH",true))
    {
        if(MOOSStrCmp(sSpecifiedPath,"SYSTEM"))
        {
           	//do nothing - the system should know where to look 
        }
        else
        {
            //ok we are being told to look in a special place
            sFullProcName=sSpecifiedPath+"/"+sProcName;
        }
    }
    else
    {
        //we just use the Anter-wide Exepath
        sFullProcName = m_sDefaultExecutablePath+sProcName;
    }
    
    //name of process as registered...is the first param after "~"
    string sMOOSName = MOOSChomp(sParam);
    
    //here we figure out what MOOS name is implied if none is given (thanks to N.P and F.A)
    if(sMOOSName.empty())
	{
        std::string sTmp = sProcName;
        if(sTmp.rfind('/') != string::npos) 
        {
            sTmp = sTmp.substr(sTmp.rfind('/')+1);
        }
        if(sTmp.empty()) 
        { 
            // ended with a / ?
            MOOSTrace("Error in configuration  -MOOS Name cannot end in \" / \" : %s\n",sProcName.c_str());
            return NULL;
        }
        sMOOSName = sTmp;
	}
    
    
    //here we bail according to our filters 
    if(!m_Filter.empty() && m_Filter.find(sMOOSName)==m_Filter.end())
    {
        return NULL;
    }
        
    //it is pssible to specifiy complicated parameters to the process being launched. (For example
    //xterm being passed a whole load of configurations and then the name of the MOOS process it should
    //itself launch. This next call fills in a string list of such parameters
    STRING_LIST sLaunchParams;
    if(bNewConsole)
    	MakeConsoleLaunchParams(sOption, sLaunchParams,sProcName,sMOOSName);
    
    
    //here we figure what extra command line parameters should be given to the MOOS Process
    //being launched.
    STRING_LIST ExtraCommandLineParameters;
    MakeExtraExecutableParameters(sOption,ExtraCommandLineParameters,sProcName,sMOOSName);
    
    
    
    //All good up to here, now make a new process info holder stucture...
    pNewProc = new MOOSProc; // 在堆上创建一个进程描述对象，用于保存即将启动的进程信息。
    pNewProc->m_sApp = sFullProcName; // 保存可执行文件的完整路径或程序名。
    pNewProc->m_ExtraCommandLineParameters =ExtraCommandLineParameters; // 保存需要额外传给子进程的命令行参数。
    pNewProc->m_ConsoleLaunchParameters = sLaunchParams; // 保存终端或新控制台的启动参数。
    pNewProc->m_bInhibitMOOSParams = bInhibitMOOSParams; // 保存是否禁止传入任务文件和 MOOS 名称参数。
	pNewProc->m_sMOOSName = sMOOSName; // 保存该进程使用的 MOOS 应用名称。
    pNewProc->m_bNewConsole = bNewConsole; // 保存是否为该进程创建新的控制台窗口。
    pNewProc->m_sMissionFile = m_MissionReader.GetFileName(); // 保存当前使用的任务文件名。
    
    //finally spawn each according to his own
#ifndef _WIN32
    if(DoNixOSLaunch(pNewProc))
#else
        if(DoWin32Launch(pNewProc))
#endif
        {
            return pNewProc;
        }
        else
        {        
            delete pNewProc;
            return NULL;
        }
}


bool CAntler::ShutDown()
{
	
	MOOSPROC_LIST::iterator q;

	MOOSTrace("\n\n|***** Shutdown *****|\n\n");

	for(q = m_ProcList.begin();q!=m_ProcList.end();q++)
	{
		MOOSProc * pMOOSProc = *q;

		if(KillNicely(pMOOSProc))
		{
	 
			int nStatus = 0;
#ifndef _WIN32
			
			MOOSTrace("\n   Signalling %-15s ",pMOOSProc->m_sApp.c_str());
			if(waitpid(pMOOSProc->m_ChildPID,&nStatus,0)>0)
			{
				MOOSTrace("[OK]");
			}		
#endif
		}

	 }
	
	MOOSTrace("\n\n   All spawned processes shutdown.\n\n   That was the MOOS \n\n");
	 
	 
	return true;
}



#ifdef _WIN32
bool CAntler::DoWin32Launch(CAntler::MOOSProc * pNewProc)
{
    
    
    try
    {
        
        // make the command line ProcessBinaryName MissionFile ProcessMOOSName
        string sCmd = pNewProc->m_sApp+" ";
        if(pNewProc->m_bInhibitMOOSParams==false)
            sCmd+=pNewProc->m_sMissionFile+" "+pNewProc->m_sMOOSName+" ";
        
        //continuing this task, here we pass extra parameters to the MOOS process if required
		for(STRING_LIST::iterator p = pNewProc->m_ExtraCommandLineParameters.begin();p!=pNewProc->m_ExtraCommandLineParameters.end();p++)
        {
            sCmd+=*p+" ";
        }
        
        
        //make a new process attributes class
        pNewProc->pWin32Attrib = NULL;
        pNewProc->pWin32Attrib = new XPCProcessAttrib(NULL,(char *)sCmd.c_str());
        
        if(pNewProc->m_bNewConsole)
        {
            pNewProc->pWin32Attrib->vSetCreationFlag(CREATE_NEW_CONSOLE | CREATE_SUSPENDED);        
        }
        else
        {
            pNewProc->pWin32Attrib->vSetCreationFlag(CREATE_SUSPENDED);
        }
        
        
        //we shall use our own start up image as a starting point
        STARTUPINFO StartUp;
        GetStartupInfo (&StartUp);
        
        //set up the title
        StartUp.lpTitle = (char*)pNewProc->m_sApp.c_str();
        
        //set up white text
        StartUp.dwFillAttribute = 
        FOREGROUND_INTENSITY| 
        FOREGROUND_BLUE |
        FOREGROUND_RED |
        FOREGROUND_GREEN|
        BACKGROUND_INTENSITY ;
        
        
		//give users basic control over backgroun as an RGB combination
        for(STRING_LIST::iterator q=pNewProc->m_ConsoleLaunchParameters.begin();q!=pNewProc->m_ConsoleLaunchParameters.end();q++)
        {
            if(MOOSStrCmp(*q, "BACKGROUND_BLUE"))
            {
                StartUp.dwFillAttribute|=BACKGROUND_BLUE;
                StartUp.dwFlags|=STARTF_USEFILLATTRIBUTE;
            }
            if(MOOSStrCmp(*q, "BACKGROUND_GREEN"))
            {
                StartUp.dwFillAttribute|=BACKGROUND_GREEN;
                StartUp.dwFlags|=STARTF_USEFILLATTRIBUTE;
            }
            if(MOOSStrCmp(*q, "BACKGROUND_RED"))
            {
                StartUp.dwFillAttribute|=BACKGROUND_RED;
                StartUp.dwFlags|=STARTF_USEFILLATTRIBUTE;
            }
            
        }
        
        pNewProc->pWin32Attrib->vSetStartupInfo(&StartUp);
        
        //no create an object capable of laucnhing win32 processes
        pNewProc->pWin32Proc = NULL;
        pNewProc->pWin32Proc =new XPCProcess(* pNewProc->pWin32Attrib);
        
        //go!
        pNewProc->pWin32Proc->vResume();
        
    }
    catch (XPCException & e)
    {
        if(pNewProc->pWin32Attrib!=NULL)
        {
            delete pNewProc->pWin32Attrib;
        }
        if(pNewProc->pWin32Proc!=NULL)
        {
            delete pNewProc->pWin32Proc;
        }
       	MOOSTrace("*** %s Launch Failed:***\n\a\a",pNewProc->m_sApp.c_str());
       	MOOSTrace("%s\n",e.sGetException());
        return false;
    }
    
    return true;
    
}
#else

bool CAntler::DoNixOSLaunch(CAntler::MOOSProc * pNewProc)
{
    
    //make a child process
    if((pNewProc->m_ChildPID = fork())<0) // 创建子进程；返回负数表示创建失败。
    {
        //hell!
        MOOSTrace("fork failed, not good\n"); // 输出 fork 失败信息。
        return false; // 通知调用方启动失败。
    }
    else if(pNewProc->m_ChildPID ==0) // 子进程中的 fork 返回值为 0。
    {
        //I'm the child now..
        
        STRING_LIST::iterator p = pNewProc->m_ConsoleLaunchParameters.begin(); // 初始化终端启动参数迭代器。
	unsigned int nExecParams = pNewProc->m_ConsoleLaunchParameters.size()+pNewProc->m_ExtraCommandLineParameters.size()+ 6; // 计算 argv 所需的最大容量。
        const char ** pExecVParams = new const char* [nExecParams]; // 分配 execvp 所需的参数数组。
        int i = 0; // 保存当前写入 argv 的位置。
        
        //do we need to configure an xterm?
        if(pNewProc->m_bNewConsole) // 如果要求新控制台，则先配置终端程序。
        {
            pExecVParams[i++] = DEFAULT_NIX_TERMINAL; // 将默认 Unix 终端程序放入 argv 首项。
            if(!pNewProc->m_ConsoleLaunchParameters.empty()) // 如果存在终端配置参数，则继续追加。
            {
                while(p!=pNewProc->m_ConsoleLaunchParameters.end()) // 遍历所有终端配置参数。
                {
                    pExecVParams[i++] = (p++->c_str()); // 添加当前终端参数并移动迭代器。
                }
            }
            
            pExecVParams[i++] = "-e"; // 告诉终端后续参数是要执行的程序及其参数。
            
        }
        
        //here we fill in the process name we really care about
        pExecVParams[i++] = (pNewProc->m_sApp.c_str()) ; // 添加待启动的 MOOS 可执行文件路径。
        if(!pNewProc->m_bInhibitMOOSParams) // 未禁止时，将任务文件和 MOOS 名称传给子程序。
        {
            //we do the usual thing of supplying Mission file and MOOSName
            pExecVParams[i++] = pNewProc->m_sMissionFile.c_str(); // 添加任务文件路径。
            pExecVParams[i++] = pNewProc->m_sMOOSName.c_str(); // 添加该进程使用的 MOOS 名称。
        }
        
        
        //here we pass extra parameters to the MOOS process if required
        for(p = pNewProc->m_ExtraCommandLineParameters.begin();p!=pNewProc->m_ExtraCommandLineParameters.end();p++) // 遍历额外命令行参数。
        {
            pExecVParams[i++] = (p->c_str()); // 将当前额外参数追加到 argv。
        }
        
        //terminate list
        pExecVParams[i++] = NULL; // 以空指针结束 argv，满足 execvp 的接口要求。
#if(DEBUG_LAUNCH)
        for(int j = 0;j<i;j++) // 调试模式下遍历并打印最终 argv。
            MOOSTrace("argv[%d]:\"%s\"\n",j,pExecVParams[j]); // 输出每个命令行参数。
#endif
        
        //and finally replace ourselves with a new xterm process image
	char * const * pParamList = const_cast<char * const *> (pExecVParams); // 转换为 execvp 要求的可写字符指针类型。
        if(execvp(pExecVParams[0], pParamList)==-1) // 执行目标程序；成功时不会返回。
        {
            MOOSTrace("Failed exec - not good. Called exec as follows:\n"); // 输出 exec 执行失败信息。
            exit(EXIT_FAILURE); // 子进程无法启动目标程序时立即退出。
            
        }
        
    }
    
    //Parent execution stream continues here...
	//we need to change the process group of the spawned process so that ctrl-C
	//does get sent to all the launched xterms. Instead we will catch ctrl-C ourself
	//and find the MOOS process running within the sub process, signal it to die
	//so subprocesses can clean up nicely
    if(m_bSupportGentleKill) // 如果启用优雅终止，则为子进程设置独立进程组。
        setpgid(pNewProc->m_ChildPID,0); // 让后续信号可以作用于该子进程组。
    
    return true; // 父进程已成功创建子进程并完成启动设置。
}

#endif

