#include <cstdint>
#include <Windows.h>
#include "application.h"
#include "renderer_dx12.h"

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	SetCurrentDirectory(WORKING_DIR);

	if(!application::initialize())
	{
		return 0;
	}

	WNDCLASSEX wcx;
	wcx.cbSize = sizeof(wcx);
	wcx.style = 0;
	wcx.lpfnWndProc = WindowProc;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = hInstance;
	wcx.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wcx.lpszMenuName = nullptr;
	wcx.lpszClassName = "LearningGrimoireOfTheDirectX12";
	wcx.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);
	if(!RegisterClassEx(&wcx))
	{
		return 0;
	}

	const int32_t screen_width = GetSystemMetrics(SM_CXSCREEN);
	const int32_t screen_height = GetSystemMetrics(SM_CYSCREEN);

	constexpr int32_t client_width = 1280;
	constexpr int32_t client_height = 720;

	RECT window_rect
	{
		(screen_width - client_width) / 2,
		(screen_height - client_height) / 2,
		(screen_width + client_width) / 2,
		(screen_height + client_height) / 2
	};

	DWORD window_style = WS_OVERLAPPEDWINDOW ^ WS_SIZEBOX;
	DWORD window_style_ex = WS_EX_ACCEPTFILES;

	AdjustWindowRectEx(&window_rect, window_style_ex, FALSE, window_style);

	const int32_t window_width = window_rect.right - window_rect.left;
	const int32_t window_height = window_rect.bottom - window_rect.top;

	HWND hWnd = CreateWindowEx(
		window_style_ex,
		wcx.lpszClassName,
		wcx.lpszClassName,
		window_style,
		window_rect.left,
		window_rect.top,
		window_width,
		window_height,
		nullptr,
		nullptr,
		wcx.hInstance,
		nullptr
	);
	if(hWnd == nullptr)
	{
		return 0;
	}

	RendererDX12 renderer;
	if(!renderer.initialize(client_width, client_height, hWnd))
	{
		return 0;
	}

	ShowWindow(hWnd, nShowCmd);

	MSG msg {};
	while(true)
	{
		if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		renderer.render();
	}

	application::finalize();

	return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	return 0;
}
