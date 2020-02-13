#include "renderer_dx12.h"
#include <algorithm>
#include <d3dcompiler.h>
#include <DirectXTex.h>

using namespace Microsoft::WRL;
using namespace DirectX;

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

	if(!createVertexBuffer())
	{
		return false;
	}

	if(!createIndexBuffer())
	{
		return false;
	}

	if(!createRootSignature())
	{
		return false;
	}

	if(!createGraphicsPipelineState())
	{
		return false;
	}

	mViewport.TopLeftX = 0.0f;
	mViewport.TopLeftY = 0.0f;
	mViewport.Width = static_cast<float>(mWidth);
	mViewport.Height = static_cast<float>(mHeight);
	mViewport.MinDepth = D3D12_MIN_DEPTH;
	mViewport.MaxDepth = D3D12_MAX_DEPTH;

	mScissorRect.left = 0;
	mScissorRect.top = 0;
	mScissorRect.right = mWidth;
	mScissorRect.bottom = mHeight;

	return true;
}

bool RendererDX12::render()
{
	static uint32_t frame = 0;

	auto back_buffer_index = mpSwapChain->GetCurrentBackBufferIndex();

	D3D12_RESOURCE_BARRIER resource_barrier;
	resource_barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	resource_barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	resource_barrier.Transition.pResource = mpBackBuffers[back_buffer_index].Get();
	resource_barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	resource_barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	resource_barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

	mpGraphicsCommandList->ResourceBarrier(1, &resource_barrier);
	mpGraphicsCommandList->SetPipelineState(mpGraphicsPipelineState.Get());

	auto rtv_handle = mpRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtv_handle.ptr += back_buffer_index * mRTVDescriptorSize;
	mpGraphicsCommandList->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);

	float clear_color[]
	{
		((frame >> 16) & 0xFF) / 255.0f,
		((frame >> 8) & 0xff) / 255.0f,
		((frame >> 0) & 0xff) / 255.0f,
		1.0f
	};
	mpGraphicsCommandList->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);
	++frame;

	mpGraphicsCommandList->RSSetViewports(1, &mViewport);
	mpGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);
	mpGraphicsCommandList->SetGraphicsRootSignature(mpRootSignature.Get());

	mpGraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mpGraphicsCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mpGraphicsCommandList->IASetIndexBuffer(&mIndexBufferView);

	mpGraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

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

bool RendererDX12::createVertexBuffer()
{
	XMFLOAT3 vertices[]
	{
		{ -0.4f, -0.7f, 0.0f },
		{ -0.4f,  0.7f, 0.0f },
		{  0.4f, -0.7f, 0.0f },
		{  0.4f,  0.7f, 0.0f },
	};

	D3D12_HEAP_PROPERTIES heap_properties;
	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resource_desc;
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Alignment = 0;
	resource_desc.Width = sizeof(vertices);
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = mpDevice->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mpVertexBuffer)
	);
	if(FAILED(hr))
	{
		return false;
	}

	void * p_dst = nullptr;
	mpVertexBuffer->Map(0, nullptr, &p_dst);

	memcpy(p_dst, vertices, sizeof(vertices));

	mpVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mpVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = sizeof(vertices);
	mVertexBufferView.StrideInBytes = sizeof(vertices[0]);

	return true;
}

bool RendererDX12::createIndexBuffer()
{
	uint32_t indices[]
	{
		0, 1, 2,
		2, 1, 3,
	};

	D3D12_HEAP_PROPERTIES heap_properties;
	heap_properties.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resource_desc;
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resource_desc.Alignment = 0;
	resource_desc.Width = sizeof(indices);
	resource_desc.Height = 1;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_UNKNOWN;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	HRESULT hr = mpDevice->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mpIndexBuffer)
	);
	if(FAILED(hr))
	{
		return false;
	}

	void * p_dst = nullptr;
	mpIndexBuffer->Map(0, nullptr, &p_dst);

	memcpy(p_dst, indices, sizeof(indices));

	mpIndexBuffer->Unmap(0, nullptr);

	mIndexBufferView.BufferLocation = mpIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.SizeInBytes = sizeof(indices);
	mIndexBufferView.Format = DXGI_FORMAT_R32_UINT;

	return true;
}

bool RendererDX12::createRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
	root_signature_desc.NumParameters = 0;
	root_signature_desc.pParameters = nullptr;
	root_signature_desc.NumStaticSamplers = 0;
	root_signature_desc.pStaticSamplers = nullptr;
	root_signature_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> p_root_signature_blob;
	ComPtr<ID3DBlob> p_error_blob;
	HRESULT hr = D3D12SerializeRootSignature(
		&root_signature_desc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&p_root_signature_blob,
		&p_error_blob
	);
	if(FAILED(hr))
	{
		return false;
	}

	hr = mpDevice->CreateRootSignature(
		0,
		p_root_signature_blob->GetBufferPointer(),
		p_root_signature_blob->GetBufferSize(),
		IID_PPV_ARGS(&mpRootSignature)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

static bool loadShader(LPCWSTR path, const char * entry_point, const char * target, ComPtr<ID3DBlob> & dst)
{
	uint32_t flags = 0;
#ifdef _DEBUG
	flags |= D3DCOMPILE_DEBUG;
	flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	ComPtr<ID3DBlob> p_error_blob;
	HRESULT hr = D3DCompileFromFile(
		path,
		nullptr, 
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entry_point,
		target,
		flags,
		0,
		&dst,
		&p_error_blob
	);
	if(FAILED(hr))
	{
		OutputDebugStringA(reinterpret_cast<LPCSTR>(p_error_blob->GetBufferPointer()));
		return false;
	}

	return true;
}

bool RendererDX12::createGraphicsPipelineState()
{
	HRESULT hr = S_OK;
	ComPtr<ID3DBlob> p_error_blob;

	ComPtr<ID3DBlob> p_vertex_shder_blob;
	if(!loadShader(L"BasicShader.hlsl", "BasicVS", "vs_5_0", p_vertex_shder_blob))
	{
		return false;
	}

	ComPtr<ID3DBlob> p_pixel_shder_blob;
	if(!loadShader(L"BasicShader.hlsl", "BasicPS", "ps_5_0", p_pixel_shder_blob))
	{
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pipeline_state_desc;
	graphics_pipeline_state_desc.pRootSignature = mpRootSignature.Get();

	graphics_pipeline_state_desc.VS.pShaderBytecode = p_vertex_shder_blob->GetBufferPointer();
	graphics_pipeline_state_desc.VS.BytecodeLength = p_vertex_shder_blob->GetBufferSize();

	graphics_pipeline_state_desc.PS.pShaderBytecode = p_pixel_shder_blob->GetBufferPointer();
	graphics_pipeline_state_desc.PS.BytecodeLength = p_pixel_shder_blob->GetBufferSize();

	graphics_pipeline_state_desc.DS.pShaderBytecode = nullptr;
	graphics_pipeline_state_desc.DS.BytecodeLength = 0;

	graphics_pipeline_state_desc.HS.pShaderBytecode = nullptr;
	graphics_pipeline_state_desc.HS.BytecodeLength = 0;

	graphics_pipeline_state_desc.GS.pShaderBytecode = nullptr;
	graphics_pipeline_state_desc.GS.BytecodeLength = 0;

	graphics_pipeline_state_desc.StreamOutput.pSODeclaration = nullptr;
	graphics_pipeline_state_desc.StreamOutput.NumEntries = 0;
	graphics_pipeline_state_desc.StreamOutput.pBufferStrides = nullptr;
	graphics_pipeline_state_desc.StreamOutput.NumStrides = 0;
	graphics_pipeline_state_desc.StreamOutput.RasterizedStream = 0;

	graphics_pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	graphics_pipeline_state_desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphics_pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	graphics_pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	graphics_pipeline_state_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	graphics_pipeline_state_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	graphics_pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	graphics_pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	graphics_pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;
	graphics_pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	graphics_pipeline_state_desc.RasterizerState.ForcedSampleCount = 0;
	graphics_pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	graphics_pipeline_state_desc.DepthStencilState.DepthEnable = FALSE;
	graphics_pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	graphics_pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	graphics_pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
	graphics_pipeline_state_desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	graphics_pipeline_state_desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	D3D12_INPUT_ELEMENT_DESC input_element_descs[1];
	input_element_descs[0].SemanticName = "POSITION";
	input_element_descs[0].SemanticIndex = 0;
	input_element_descs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	input_element_descs[0].InputSlot = 0;
	input_element_descs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	input_element_descs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[0].InstanceDataStepRate = 0;

	graphics_pipeline_state_desc.InputLayout.pInputElementDescs = input_element_descs;
	graphics_pipeline_state_desc.InputLayout.NumElements = 1;

	graphics_pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	graphics_pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphics_pipeline_state_desc.NumRenderTargets = 1;

	graphics_pipeline_state_desc.RTVFormats[0] = mSwapChainBufferFormat;
	graphics_pipeline_state_desc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[4] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[5] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[6] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[7] = DXGI_FORMAT_UNKNOWN;

	graphics_pipeline_state_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	graphics_pipeline_state_desc.SampleDesc.Count = 1;
	graphics_pipeline_state_desc.SampleDesc.Quality = 0;

	graphics_pipeline_state_desc.NodeMask = 0;
	graphics_pipeline_state_desc.CachedPSO.pCachedBlob = nullptr;
	graphics_pipeline_state_desc.CachedPSO.CachedBlobSizeInBytes = 0;
	graphics_pipeline_state_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	hr = mpDevice->CreateGraphicsPipelineState(
		&graphics_pipeline_state_desc,
		IID_PPV_ARGS(&mpGraphicsPipelineState)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}
