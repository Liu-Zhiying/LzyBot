#include <Windows.h>
#include <iostream>
#include <string>
#include <cstring>
#include <aclapi.h>
#include <fstream>

#pragma comment(lib, "advapi32.lib")

#define MCL_WORKING_DIR L"\\MCL"
#define JAVA_RUNNER_PATH L"\\JDK17X64\\bin\\java.exe"
#define BOT_WORKING_DIR L"\\win-x64"
#define BOT_PATH L"\\win-x64\\LzyBot.exe"
#define JAVA_RUNNER_ARGUS L" -jar .\\mcl.jar"
#define MUTEX_NAME L"Global\\StartLzyBot"

WCHAR szServiceName[] = L"LzyBot";
volatile LONG iServiceStatus = 0;
SERVICE_STATUS_HANDLE ServiceStatusHandle;

PSECURITY_DESCRIPTOR GetPublicMutexSecirityDescriptor()
{
	SID_IDENTIFIER_AUTHORITY SIA = SECURITY_WORLD_SID_AUTHORITY;
	EXPLICIT_ACCESSW ea;
	PSID pSid = NULL;
	PACL pAcl = NULL;
	PSECURITY_DESCRIPTOR pSD = NULL;
	AllocateAndInitializeSid(&SIA, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &pSid);
	if (pSid)
	{
		ea.grfAccessMode = SET_ACCESS;
		ea.grfAccessPermissions = MUTEX_ALL_ACCESS;
		ea.grfInheritance = NO_INHERITANCE;
		ea.Trustee.MultipleTrusteeOperation = NO_MULTIPLE_TRUSTEE;
		ea.Trustee.pMultipleTrustee = NULL;
		ea.Trustee.ptstrName = (LPWSTR)pSid;
		ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;

		SetEntriesInAclW(1, &ea, NULL, &pAcl);
		if (pAcl)
		{
			pSD = LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
			if (pSD)
			{
				if (!InitializeSecurityDescriptor(pSD, SECURITY_DESCRIPTOR_REVISION)
					|| !SetSecurityDescriptorDacl(pSD, TRUE, pAcl, FALSE))
				{
					LocalFree(pSD);
					pSD = NULL;
				}	
			}
			LocalFree(pAcl);
		}
		FreeSid(pSid);
	}
	return pSD;
}

std::wstring GetModulePathWithoutFileName()
{
	WCHAR szBinPath[MAX_PATH];
	GetModuleFileNameW(GetModuleHandleW(NULL), szBinPath, MAX_PATH);
	std::wstring result = szBinPath;
	size_t pos = result.find_last_of('\\');
	result = result.erase(pos, result.size() - pos);
	return result;
}

DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	switch (dwControl)
	{
	case SERVICE_CONTROL_STOP:
	{
		//递增提示服务主函数服务应该结束
		InterlockedAdd(&iServiceStatus, 1);
		//等待再次递增，代表服务清理完成
		while (InterlockedAdd(&iServiceStatus, 0) != 2);
		//提示服务结束
		SERVICE_STATUS ServiceStatus;
		ServiceStatus.dwCurrentState = SERVICE_STOPPED;
		ServiceStatus.dwServiceType = SERVICE_WIN32;
		ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		ServiceStatus.dwCheckPoint = 0;
		ServiceStatus.dwWin32ExitCode = 0;
		ServiceStatus.dwServiceSpecificExitCode = 0;
		ServiceStatus.dwWaitHint = 0;
		SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
		break;
	}
	default:
		break;
	}
	return NO_ERROR;
}

VOID WINAPI ServiceMain(DWORD dwNumServicesArgs, LPWSTR* lpServiceArgVectors)
{
	SERVICE_STATUS ServiceStatus;
	ServiceStatus.dwCurrentState = SERVICE_RUNNING;
	ServiceStatus.dwServiceType = SERVICE_WIN32;
	ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
	ServiceStatus.dwCheckPoint = 0;
	ServiceStatus.dwWin32ExitCode = 0;
	ServiceStatus.dwServiceSpecificExitCode = 0;
	ServiceStatus.dwWaitHint = 0;
	//注册服务事件处理函数
	ServiceStatusHandle = RegisterServiceCtrlHandlerExW(szServiceName, ServiceControlHandler, NULL);
	//设置服务状态
	SetServiceStatus(ServiceStatusHandle, &ServiceStatus);
	PROCESS_INFORMATION ps1, ps2;
	ps1.hProcess = NULL;
	ps1.hThread = NULL;
	ps2.hProcess = NULL;
	ps2.hThread = NULL;
	BOOL bSuccess1 = FALSE, bSuccess2 = FALSE;
	//轮询目标进程（即MCL进程和机器人主进程）没有则创建
	while (!InterlockedAdd(&iServiceStatus, 0))
	{
		if (!bSuccess1 || !bSuccess2
			|| WaitForSingleObject(ps1.hProcess, 0) == WAIT_FAILED
			|| WaitForSingleObject(ps2.hProcess, 0) == WAIT_FAILED)
		{
			if (ps2.hThread)
			{
				CloseHandle(ps2.hThread);
				ps2.hThread = NULL;
			}
			if (ps1.hThread)
			{
				CloseHandle(ps1.hThread);
				ps1.hThread = NULL;
			}
			if (ps1.hProcess)
			{
				TerminateProcess(ps1.hProcess, 0);
				ps1.hProcess = NULL;
			}
			if (ps2.hProcess)
			{
				TerminateProcess(ps2.hProcess, 0);
				ps2.hProcess = NULL;
			}
			
			WCHAR szCmdLine[MAX_PATH];
			memcpy(szCmdLine, JAVA_RUNNER_ARGUS, (wcslen(JAVA_RUNNER_ARGUS) + 1) * sizeof(WCHAR));
			STARTUPINFOW StartupInfo;
			memset(&StartupInfo, 0, sizeof StartupInfo);
			StartupInfo.cb = sizeof StartupInfo;
			std::wstring CurrentDir = GetModulePathWithoutFileName();
			std::wstring temp1 = CurrentDir + JAVA_RUNNER_PATH;
			std::wstring temp2 = CurrentDir + MCL_WORKING_DIR;
			bSuccess1 = CreateProcessW(temp1.c_str(), szCmdLine, NULL, NULL, false, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, temp2.c_str(), &StartupInfo, &ps1);
			temp1 = CurrentDir + BOT_PATH;
			temp2 = CurrentDir + BOT_WORKING_DIR;
			Sleep(10000);
			bSuccess2 = CreateProcessW(temp1.c_str(), NULL, NULL, NULL, false, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, temp2.c_str(), &StartupInfo, &ps2);
		}
		Sleep(100);
	}
	if (ps1.hThread)
	{
		CloseHandle(ps1.hThread);
		ps1.hThread = NULL;
	}
	if (ps2.hThread)
	{
		CloseHandle(ps2.hThread);
		ps1.hThread = NULL;
	}
	//清理（结束进程）
	if (ps1.hProcess)
	{
		TerminateProcess(ps1.hProcess, 0);
		ps1.hProcess = NULL;
	}
	if (ps2.hProcess)
	{
		TerminateProcess(ps2.hProcess, 0);
		ps2.hProcess = NULL;
	}
	//递增，代表清理完成
	InterlockedAdd(&iServiceStatus, 1);
}

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		//测试启动模式，以独立命令行窗口启动MCL + 机器人主程序
		std::cout << "测试启动模式：" << std::endl;
		std::cout << "$启动器路径 r：注册服务" << std::endl;
		std::cout << "$启动器路径 u：取消注册服务" << std::endl;
		std::cout << "$启动器路径 s：服务模式启动（请勿手动使用）" << std::endl;
		std::cout << "首先启动MCL控制台，10s后启动机器人主程序" << std::endl;
		HANDLE hMutex = OpenMutex(MUTEX_ALL_ACCESS, FALSE, MUTEX_NAME);
		if (hMutex)
		{
			std::cout << "服务模式已经启动，请关闭服务，再使用测试启动模式！" << std::endl;
			CloseHandle(hMutex);
			return 0;
		}
		WCHAR szCmdLine[MAX_PATH];
		memcpy(szCmdLine, JAVA_RUNNER_ARGUS, (wcslen(JAVA_RUNNER_ARGUS) + 1) * sizeof(WCHAR));
		STARTUPINFOW StartupInfo;
		memset(&StartupInfo, 0, sizeof StartupInfo);
		StartupInfo.cb = sizeof StartupInfo;
		PROCESS_INFORMATION ps;
		BOOL bSuccess;
		std::wstring CurrentDir = GetModulePathWithoutFileName();
		std::wstring temp1 = CurrentDir + JAVA_RUNNER_PATH;
		std::wstring temp2 = CurrentDir + MCL_WORKING_DIR;
		bSuccess = CreateProcessW(temp1.c_str(), szCmdLine, NULL, NULL, false, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, temp2.c_str(), &StartupInfo, &ps);
		if (!bSuccess)
		{
			std::cout << "MCL启动失败!";
		}
		else
		{
			CloseHandle(ps.hProcess);
			CloseHandle(ps.hThread);
		}
		Sleep(10000);
		temp1 = CurrentDir + BOT_PATH;
		temp2 = CurrentDir + BOT_WORKING_DIR;
		bSuccess = CreateProcessW(temp1.c_str(), NULL, NULL, NULL, false, NORMAL_PRIORITY_CLASS | CREATE_NEW_CONSOLE, NULL, temp2.c_str(), &StartupInfo, &ps);
		if (!bSuccess)
		{
			std::cout << "机器人主程序启动失败!";
		}
		else
		{
			CloseHandle(ps.hProcess);
			CloseHandle(ps.hThread);
		}
		return 0;
	}
	else
	{
		if (!std::strcmp("r", argv[1]))
		{
			SC_HANDLE SCHnandle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!SCHnandle)
			{
				std::cout << "Windows服务管理器打开失败！" << std::endl;
				return 0;
			}
			std::wstring ServiceCmdLine = L"";
			WCHAR szBinPath[MAX_PATH];
			GetModuleFileNameW(GetModuleHandle(NULL), szBinPath, MAX_PATH);
			ServiceCmdLine += szBinPath;
			ServiceCmdLine += L" s";
			SC_HANDLE ServiceHandle = CreateServiceW(SCHnandle, szServiceName, szServiceName, GENERIC_ALL, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, ServiceCmdLine.c_str(), NULL, 0, NULL, NULL, NULL);
			if (!ServiceHandle)
			{
				CloseServiceHandle(SCHnandle);
				std::cout << "Windows服务创建失败！" << std::endl;
				return 0;
			}
			if (!StartServiceW(ServiceHandle, 0, NULL))
			{
				std::cout << "Windows服务创建成功，但是启动失败！" << std::endl;
			}
			CloseServiceHandle(ServiceHandle);
			CloseServiceHandle(SCHnandle);
		}
		else if (!std::strcmp("u", argv[1]))
		{
			SC_HANDLE SCHnandle = OpenSCManagerW(NULL, NULL, SC_MANAGER_ALL_ACCESS);
			if (!SCHnandle)
			{
				std::cout << "Windows服务管理器打开失败！" << std::endl;
				return 0;
			}
			SC_HANDLE ServiceHandle = OpenServiceW(SCHnandle, szServiceName, SERVICE_ALL_ACCESS);
			if (!ServiceHandle)
			{
				CloseServiceHandle(SCHnandle);
				std::cout << "无对应的Windows服务！" << std::endl;
				return 0;
			}
			SERVICE_STATUS ServiceStatus;
			if (!ControlService(ServiceHandle, SERVICE_CONTROL_STOP, &ServiceStatus) && ServiceStatus.dwCurrentState != SERVICE_STOPPED)
			{
				CloseServiceHandle(ServiceHandle);
				CloseServiceHandle(SCHnandle);
				std::cout << "Windows服务关闭失败！" << std::endl;
				return 0;
			}
			if (!DeleteService(ServiceHandle))
			{
				std::cout << "Windows服务删除失败！" << std::endl;
			}
			CloseServiceHandle(ServiceHandle);
			CloseServiceHandle(SCHnandle);
		}
		else if (!std::strcmp("s", argv[1]))
		{
			PSECURITY_DESCRIPTOR pSD = GetPublicMutexSecirityDescriptor();
			if (!pSD)
			{
				std::cout << "初始化失败！" << std::endl;
				return 0;
			}
			SECURITY_ATTRIBUTES sa;
			sa.nLength = sizeof sa;
			sa.lpSecurityDescriptor = pSD;
			sa.bInheritHandle = FALSE;
			HANDLE hMutex = CreateMutexExW(&sa, MUTEX_NAME, 0, MUTEX_ALL_ACCESS);
			LocalFree(pSD);
			if (GetLastError() != ERROR_SUCCESS)
			{
				if (hMutex)
				{
					CloseHandle(hMutex);
				}
				std::cout << "程序服务模式只能运行一个实例！" << std::endl;
				return 0;
			}

			SERVICE_TABLE_ENTRYW serviceTables[2];
			serviceTables[0].lpServiceName = szServiceName;
			serviceTables[0].lpServiceProc = ServiceMain;
			serviceTables[1].lpServiceName = NULL;
			serviceTables[1].lpServiceProc = NULL;
			//启动服务程序
			StartServiceCtrlDispatcherW(serviceTables);

			if (hMutex)
			{
				CloseHandle(hMutex);
			}
		}
		else
		{
			std::cout << "参数错误" << std::endl;
		}
	}
}