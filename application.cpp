#include "application.h"
#include <atomic>
#include <memory>
#include <Windows.h>

bool application::initialize()
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

void application::finalize()
{
	CoUninitialize();
}
