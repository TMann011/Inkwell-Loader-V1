#include "service.hpp"
#include <Windows.h>
#include <string>
#include <iostream>

#include "utils.hpp"
#include "nt.hpp"

NTSTATUS service::RegisterAndStart(const std::wstring& driver_path, const std::wstring& serviceName) {
	const static DWORD ServiceTypeKernel = 1;
	const std::wstring servicesPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName;
	const std::wstring nPath = L"\\??\\" + driver_path;

	HKEY dservice;
	LSTATUS status = RegCreateKeyW(HKEY_LOCAL_MACHINE, servicesPath.c_str(), &dservice); //Returns Ok if already exists
	if (status != ERROR_SUCCESS) {
		kdmLog("[-] Can't create service key" << std::endl);
		return STATUS_REGISTRY_IO_FAILED;
	}

	status = RegSetKeyValueW(dservice, NULL, L"ImagePath", REG_EXPAND_SZ, nPath.c_str(), (DWORD)(nPath.size()*sizeof(wchar_t)));
	if (status != ERROR_SUCCESS) {
		RegCloseKey(dservice);
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		kdmLog("[-] Can't create 'ImagePath' registry value" << std::endl);
		return STATUS_REGISTRY_IO_FAILED;
	}
	
	status = RegSetKeyValueW(dservice, NULL, L"Type", REG_DWORD, &ServiceTypeKernel, sizeof(DWORD));
	if (status != ERROR_SUCCESS) {
		RegCloseKey(dservice);
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		kdmLog("[-] Can't create 'Type' registry value" << std::endl);
		return STATUS_REGISTRY_IO_FAILED;
	}
	
	RegCloseKey(dservice);

	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL) {
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		return STATUS_UNSUCCESSFUL;
	}

	//auto RtlAdjustPrivilege = (nt::RtlAdjustPrivilege)GetProcAddress(ntdll, "RtlAdjustPrivilege");
	//auto NtLoadDriver = (nt::NtLoadDriver)GetProcAddress(ntdll, "NtLoadDriver");

	ULONG SE_LOAD_DRIVER_PRIVILEGE = 10UL;
	BOOLEAN SeLoadDriverWasEnabled;
	NTSTATUS ntStatus = nt::RtlAdjustPrivilege(SE_LOAD_DRIVER_PRIVILEGE, TRUE, FALSE, &SeLoadDriverWasEnabled);
	if (!NT_SUCCESS(ntStatus)) {
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		kdmLog("Fatal error: failed to acquire SE_LOAD_DRIVER_PRIVILEGE. Make sure you are running as administrator." << std::endl);
		return ntStatus;
	}

	std::wstring wdriver_reg_path = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + serviceName;
	UNICODE_STRING serviceStr;
	RtlInitUnicodeString(&serviceStr, wdriver_reg_path.c_str());

	ntStatus = nt::NtLoadDriver(&serviceStr);

	kdmLog("[+] NtLoadDriver Status 0x" << std::hex << ntStatus << std::endl);

	if (ntStatus == STATUS_IMAGE_CERT_REVOKED) {
		kdmLog("[-] Your vulnerable driver list is enabled and have blocked the driver loading, you must disable vulnerable driver list to use kdmapper with intel driver" << std::endl);
		kdmLog("[-] Registry path to disable vulnerable driver list: HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\CI\\Config" << std::endl);
		kdmLog("[-] Set 'VulnerableDriverBlocklistEnable' as dword to 0" << std::endl);
	}
	else if (ntStatus == STATUS_ACCESS_DENIED || ntStatus == STATUS_INSUFFICIENT_RESOURCES) {
		kdmLog("[-] Access Denied or Insufficient Resources (0x" << std::hex << ntStatus << "), Probably some anticheat or antivirus running blocking the load of vulnerable driver" << std::endl);
	}
	
	if (!NT_SUCCESS(ntStatus)) {
		//Remove the service
		status = RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
		if (status != ERROR_SUCCESS) {
			kdmLog("[-] Can't delete service registry key after NtLoadDriver failure" << std::endl);
		}
	}

	return ntStatus;
}

NTSTATUS service::StopAndRemove(const std::wstring& serviceName) {
	HMODULE ntdll = GetModuleHandleA("ntdll.dll");
	if (ntdll == NULL)
		return STATUS_UNSUCCESSFUL;

	std::wstring wdriver_reg_path = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + serviceName;
	UNICODE_STRING serviceStr;
	RtlInitUnicodeString(&serviceStr, wdriver_reg_path.c_str());

	HKEY driver_service;
	std::wstring servicesPath = L"SYSTEM\\CurrentControlSet\\Services\\" + serviceName;
	LSTATUS status = RegOpenKeyW(HKEY_LOCAL_MACHINE, servicesPath.c_str(), &driver_service);
	if (status != ERROR_SUCCESS) {
		if (status == ERROR_FILE_NOT_FOUND) {
			return STATUS_SUCCESS; //already removed
		}
		return STATUS_REGISTRY_IO_FAILED;
	}
	RegCloseKey(driver_service);

	NTSTATUS st = nt::NtUnloadDriver(&serviceStr);
	kdmLog("[+] NtUnloadDriver Status 0x" << std::hex << st << std::endl);
	
	// Always delete registry tree to prevent orphaned service detection by EAC/BE
	RegDeleteTreeW(HKEY_LOCAL_MACHINE, servicesPath.c_str());
	return st;
}

bool service::PurgeOrphanedTempServices() {
	HKEY hServicesKey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &hServicesKey) != ERROR_SUCCESS) {
		return false;
	}

	DWORD dwIndex = 0;
	wchar_t szSubKeyName[256];
	DWORD cchSubKeyName = sizeof(szSubKeyName) / sizeof(wchar_t);
	std::vector<std::wstring> keysToDelete;

	while (RegEnumKeyExW(hServicesKey, dwIndex++, szSubKeyName, &cchSubKeyName, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
		cchSubKeyName = sizeof(szSubKeyName) / sizeof(wchar_t);
		std::wstring subKeyPath = L"SYSTEM\\CurrentControlSet\\Services\\" + std::wstring(szSubKeyName);
		HKEY hSubKey;
		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, subKeyPath.c_str(), 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
			wchar_t szImagePath[MAX_PATH];
			DWORD dwSize = sizeof(szImagePath);
			DWORD dwType = 0;
			if (RegQueryValueExW(hSubKey, L"ImagePath", NULL, &dwType, (LPBYTE)szImagePath, &dwSize) == ERROR_SUCCESS) {
				std::wstring imgPath(szImagePath);
				if (imgPath.find(L"\\Temp\\") != std::wstring::npos || imgPath.find(L"\\temp\\") != std::wstring::npos) {
					keysToDelete.push_back(std::wstring(szSubKeyName));
				}
			}
			RegCloseKey(hSubKey);
		}
	}
	RegCloseKey(hServicesKey);

	for (const auto& keyName : keysToDelete) {
		std::wstring fullPath = L"SYSTEM\\CurrentControlSet\\Services\\" + keyName;
		UNICODE_STRING serviceStr;
		std::wstring wdriver_reg_path = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\" + keyName;
		RtlInitUnicodeString(&serviceStr, wdriver_reg_path.c_str());
		nt::NtUnloadDriver(&serviceStr);
		RegDeleteTreeW(HKEY_LOCAL_MACHINE, fullPath.c_str());
	}

	return true;
}
