#include "renderer_dx12.h"

using namespace Microsoft::WRL;

RendererDX12::~RendererDX12()
{
	if(mhFenceEvent)
	{
		CloseHandle(mhFenceEvent);
	}
}

bool RendererDX12::initialize(uint32_t width, uint32_t height, HWND hWnd)
{
	mWidth = width;
	mHeight = height;

	if(!enableDebugLayer())
	{
		return false;
	}

	if(!createFactory())
	{
		return false;
	}

	if(!createDevice())
	{
		return false;
	}

	if(!createCommandAllocator())
	{
		return false;
	}

	if(!createGraphicsCommandList())
	{
		return false;
	}

	if(!createCommandQueue())
	{
		return false;
	}

	if(!createSwapChain(hWnd))
	{
		return false;
	}

	if(!createRTVDescriptorHeap())
	{
		return false;
	}

	if(!createRTVs())
	{
		return false;
	}

	if(!createFence())
	{
		return false;
	}

	return true;
}

bool RendererDX12::render()
{
	auto back_buffer_index = mpSwapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER resource_barrier;
	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resource_barrier.Transition.pResource = mpBackBuffers[back_buffer_index].Get();
	resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	auto rtv_handle = mpRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtv_handle.ptr += back_buffer_index * mRTVDescriptorSize;
	mpGraphicsCommandList->ResourceBarrier(1, &resource_barrier);

	mpGraphicsCommandList->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

	float clear_color[]{ 1.0f, 1.0f, 0.0f, 1.0f };
	mpGraphicsCommandList->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	mpGraphicsCommandList->ResourceBarrier(1, &resource_barrier);

	HRESULT hr = mpGraphicsCommandList->Close();
	if(FAILED(hr))
	{
		return false;
	}

	ID3D12CommandList * pp_command_lists[]{ mpGraphicsCommandList.Get() };
	mpCommandQueue->ExecuteCommandLists(1, pp_command_lists);
	mpCommandQueue->Signal(mpFence.Get(), ++mFenceValue);

	if(mpFence->GetCompletedValue() < mFenceValue)
	{
		mpFence->SetEventOnCompletion(mFenceValue, mhFenceEvent);
		WaitForSingleObject(mhFenceEvent, INFINITE);
	}

	mpCommandAllocator->Reset();
	mpGraphicsCommandList->Reset(mpCommandAllocator.Get(), nullptr);

	mpSwapChain->Present(1, 0);

	return true;
}

bool RendererDX12::enableDebugLayer()
{
#ifdef _DEBUG
	ComPtr<ID3D12Debug> p_debug;
	if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&p_debug))))
	{
		p_debug->EnableDebugLayer();
	}
#endif

	return true;
}

bool RendererDX12::createFactory()
{
	uint32_t flags = 0;
#ifdef _DEBUG
	flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	HRESULT hr = CreateDXGIFactory2(flags, IID_PPV_ARGS(&mpFactory));
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createDevice()
{
	HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mpDevice));
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createCommandAllocator()
{
	HRESULT hr = mpDevice->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(&mpCommandAllocator)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createGraphicsCommandList()
{
	HRESULT hr = mpDevice->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		mpCommandAllocator.Get(),
		nullptr,
		IID_PPV_ARGS(&mpGraphicsCommandList)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createCommandQueue()
{
	D3D12_COMMAND_QUEUE_DESC command_queue_desc;
	command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	command_queue_desc.NodeMask = 0;

	HRESULT hr = mpDevice->CreateCommandQueue(
		&command_queue_desc,
		IID_PPV_ARGS(&mpCommandQueue)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createSwapChain(HWND hWnd)
{
	DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
	swap_chain_desc.Width = mWidth;
	swap_chain_desc.Height = mHeight;
	swap_chain_desc.Format = mSwapChainBufferFormat;
	swap_chain_desc.Stereo = FALSE;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.SampleDesc.Quality = 0;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.BufferCount = SwapChainBufferCount;
	swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain1> p_swap_chain;
	HRESULT hr = mpFactory->CreateSwapChainForHwnd(
		mpCommandQueue.Get(),
		hWnd,
		&swap_chain_desc,
		nullptr,
		nullptr,
		&p_swap_chain
	);
	if(FAILED(hr))
	{
		return false;
	}

	if(FAILED(p_swap_chain.As(&mpSwapChain)))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createRTVDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptor_heap_desc.NumDescriptors = SwapChainBufferCount;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptor_heap_desc.NodeMask = 0;

	HRESULT hr = mpDevice->CreateDescriptorHeap(
		&descriptor_heap_desc,
		IID_PPV_ARGS(&mpRTVDescriptorHeap)
	);
	if(FAILED(hr))
	{
		return false;
	}

	mRTVDescriptorSize = mpDevice->GetDescriptorHandleIncrementSize(
		descriptor_heap_desc.Type
	);

	return true;
}

bool RendererDX12::createRTVs()
{
	HRESULT hr = S_OK;

	auto handle = mpRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	for(uint32_t i = 0; i < SwapChainBufferCount; ++i)
	{
		hr = mpSwapChain->GetBuffer(i, IID_PPV_ARGS(&mpBackBuffers[i]));
		if(FAILED(hr))
		{
			return false;
		}

		mpDevice->CreateRenderTargetView(
			mpBackBuffers[i].Get(),
			nullptr,
			handle
		);
		handle.ptr += mRTVDescriptorSize;
	}

	return true;
}

bool RendererDX12::createFence()
{
	HRESULT hr = mpDevice->CreateFence(
		mFenceValue,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(&mpFence)
	);
	if(FAILED(hr))
	{
		return false;
	}

	mhFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

	return true;
}
