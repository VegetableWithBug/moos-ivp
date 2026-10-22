///////////////////////////////////////////////////////////////////////////
//
//   MOOS - Mission Oriented Operating Suite 
//  
//   A suit of Applications and Libraries for Mobile Robotics Research 
//   Copyright (C) 2001-2005 Massachusetts Institute of Technology and 
//   Oxford University. 
//    
//   This software was written by Paul Newman at MIT 2001-2002 and Oxford 
//   University 2003-2005. email: pnewman@robots.ox.ac.uk. 
//      
//   This file is part of a  MOOS Core Component. 
//        
//   This program is free software; you can redistribute it and/or 
//   modify it under the terms of the GNU General Public License as 
//   published by the Free Software Foundation; either version 2 of the 
//   License, or (at your option) any later version. 
//          
//   This program is distributed in the hope that it will be useful, 
//   but WITHOUT ANY WARRANTY; without even the implied warranty of 
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
//   General Public License for more details. 
//            
//   You should have received a copy of the GNU General Public License 
//   along with this program; if not, write to the Free Software 
//   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 
//   02111-1307, USA. 
//
//////////////////////////    END_GPL    //////////////////////////////////





#include "Antler.h"
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>

using namespace std;




//this is our main object...everything after this is configuration...
CAntler gAntler;

#ifndef _WIN32
//this is a signal handler
void CatchMeBeforeDeath(int sig) 
{
	gAntler.ShutDown();
	exit(0);
}
#endif


int main(int argc ,char *argv[]) // 程序入口；argc 是参数数量，argv 保存命令行参数字符串。
{
	
#ifndef _WIN32
	//register a handler for shutdown
    signal(SIGINT, CatchMeBeforeDeath); // 注册 Ctrl+C 信号处理函数。
    signal(	SIGQUIT, CatchMeBeforeDeath); // 注册退出信号处理函数。
    signal(SIGTERM, CatchMeBeforeDeath); // 注册终止信号处理函数。
#endif
	
      
    
    //here we look for overloading directoves which are of form --name=value
    std::vector<std::string> vArgv; // 保存普通位置参数，例如任务文件名。
    std::map<std::string, std::string> OverLoads; // 保存 --Name=Value 形式的任务配置覆盖项。
    for(int i = 0;i<argc;i++) // 遍历所有命令行参数，包括 argv[0]。
    {
        std::string t(argv[i]); // 复制当前参数，后续解析会修改这份副本。
        if(t.find("--")==0) // 判断参数是否以 -- 开头，即是否为 Antler 覆盖指令。
        {
            //this looks like an overloading directive
            MOOSChomp(t,"--"); // 删除参数开头的 --，保留参数名和值。
            std::string sVar = MOOSChomp(t,"="); // 从参数中取出等号前的配置名，并让 t 保留配置值。
            if(t.empty()) // 没有配置值时，继续判断它是否为无值开关。
            {
				if(MOOSStrCmp(sVar, "quiet")) // 识别 --quiet 开关。
				{
					gAntler.SetVerbosity(CAntler::QUIET); // 设置 Antler 为静默输出模式。
				}
				else if(MOOSStrCmp(sVar, "terse")) // 识别 --terse 开关。
				{
					gAntler.SetVerbosity(CAntler::TERSE); // 设置 Antler 为简洁输出模式。
				}
				else
				{
					MOOSTrace("error incomplete overloading of parameter  --%s=value (are there extraneous whitespaces?)\n",sVar.c_str()); // 报告缺少配置值且又不是合法开关的参数。
					return -1; // 参数格式错误，返回失败状态。
				}
			}
            else
            {
	            	OverLoads[sVar] = t; // 保存配置覆盖项，稍后写入任务文件副本。
            }
        }
        else
        {
            //nope this is a regular parameter so it gets passed onto what is yet to come
            vArgv.push_back(t); // 保存普通参数，供后面的模式判断使用。
        }
    }
    
      
	MOOSTrace("*************************************\n"); // 打印 Antler 启动标识的上边框。
    MOOSTrace("*  This is Antler, head of MOOS...  *\n"); // 打印程序名称和用途。
    MOOSTrace("*  P. Newman 2008                   *\n"); // 打印程序原始作者和年份信息。
    MOOSTrace("*************************************\n"); // 打印 Antler 启动标识的下边框。
	
    
    switch(vArgv.size()) // 根据过滤后的普通参数数量选择运行模式。
    {
        case 2:
        {
            //standard principal Antler
            std::string sMissionFile = vArgv[1]; // 取第一个用户参数作为任务文件名；vArgv[0] 是程序名。
            
            if((int)vArgv.size()!=argc) // 如果普通参数数目少于原始参数数目，说明存在 -- 覆盖参数。
            {
                //we are overloading somehow...
                CProcessConfigReader MR; // 创建任务文件读取器。
                MR.SetFile(sMissionFile); // 读取原始任务文件。
                
                sMissionFile+="++"; // 为生成的覆盖版本任务文件追加 ++ 后缀。
                
                //we need to take a copy of the missions file and fill in overloads
                if(!MR.MakeOverloadedCopy(sMissionFile,OverLoads)) // 根据覆盖项生成新的任务文件副本。
                    return MOOSFail("error making overloaded mission file\n"); // 生成失败时输出错误并返回失败状态。
                
            }
            
                    
            return gAntler.Run(sMissionFile) ? 0 :-1; // 启动任务文件中的全部进程，成功返回 0，否则返回 -1。
        }
        case 3:
        {
            //standard principal Antler but only run a subset of processes
            //arg 3 must be string quoted
            std::string sMissionFile = vArgv[1]; // 取任务文件名。
            
            
            if((int)vArgv.size()!=argc) // 如果存在 -- 覆盖参数，则先生成覆盖后的任务文件。
            {
                //we are overloading somehow...
                CProcessConfigReader MR; // 创建任务文件读取器。
                MR.SetFile(sMissionFile); // 读取原始任务文件。
                
                sMissionFile+="++"; // 为生成的任务文件副本追加 ++ 后缀。
                
                //we need to take a copy of the missions file and fill in overloads
                if(!MR.MakeOverloadedCopy(sMissionFile,OverLoads)) // 写入命令行覆盖配置并生成副本。
                    return MOOSFail("error making overloaded mission file\n"); // 生成失败时返回错误。
                
            }
            
            
            
            //make a set of processes we want to launch
            std::stringstream S(vArgv[2]); // 将进程过滤字符串包装成输入流。
            std::set<std::string> Filter; // 保存需要启动的进程名称集合。
            //this rather fancy looking bit f stl simply iterates over a list of strings
            std::copy(istream_iterator<std::string>(S), 
                      istream_iterator<string>(),
                      std::inserter(Filter,Filter.begin())); // 按空格拆分过滤字符串并插入集合。
                       
            return gAntler.Run(sMissionFile,Filter); // 仅启动过滤集合中的进程，并返回运行结果。
        }
        case 4:
        {            
            //headless MOOS - driven my another Antler somewhere else
            std::string sDBHost = vArgv[1]; //where is DB?
            int nPort = atoi(vArgv[2].c_str()); //what port
            std::string sName = vArgv[3]; //what is our Antler name?
            return gAntler.Run(sDBHost,nPort,sName) ? 0 :-1; // 以无界面远程模式连接指定 MOOSDB 并返回运行结果。
        }
        default:
        {
			MOOSTrace("usage:\n pAntler missionfile.moos\nor\n pAntler missionfile.moos \"P1,P2,P3...\"\nor pAntler DBHost DBPort AntlerName\n"); // 参数数量不符合任何支持的调用形式时打印用法。
			MOOSTrace("\n  --quiet to suppress console output, --terse for limited output\n"); // 打印静默和简洁输出选项说明。
			
            return -1; // 参数错误，返回失败状态。
        }
    }
  
    
}
