#include "renderer_dx12.h"
#include <algorithm>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>

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

	if(!loadTexture())
	{
		return false;
	}

	if(!createConstantBuffer())
	{
		return false;
	}

	return true;
}

bool RendererDX12::render()
{
	static uint32_t frame = 0;
	static float angle = 0.0f;

	angle += 0.05f;
	mWorld = XMMatrixRotationY(angle);
	*reinterpret_cast<XMMATRIX *>(mpMappedConstantBuffer) = XMMatrixTranspose(mWorld * mView * mProjection);

	auto back_buffer_index = mpSwapChain->GetCurrentBackBufferIndex();

	mpGraphicsCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mpBackBuffers[back_buffer_index].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

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
	mpGraphicsCommandList->SetDescriptorHeaps(1, mpSRVDescriptorHeap.GetAddressOf());
	mpGraphicsCommandList->SetGraphicsRootSignature(mpRootSignature.Get());
	mpGraphicsCommandList->SetDescriptorHeaps(1, mpSRVDescriptorHeap.GetAddressOf());
	mpGraphicsCommandList->SetGraphicsRootDescriptorTable(0, mpSRVDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	mpGraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	mpGraphicsCommandList->IASetVertexBuffers(0, 1, &mVertexBufferView);
	mpGraphicsCommandList->IASetIndexBuffer(&mIndexBufferView);

	mpGraphicsCommandList->DrawIndexedInstanced(6, 1, 0, 0, 0);

	mpGraphicsCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mpBackBuffers[back_buffer_index].Get(),
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT
		)
	);

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

	D3D12_RENDER_TARGET_VIEW_DESC render_target_view_desc;
	render_target_view_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	render_target_view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	render_target_view_desc.Texture2D.MipSlice = 0;
	render_target_view_desc.Texture2D.PlaneSlice = 0;

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
			&render_target_view_desc,
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
	struct Vertex
	{
		XMFLOAT3 pos;
		XMFLOAT2 uv;
	};

	Vertex vertices[]
	{
		{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
		{ { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },
		{ {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },
	};

	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices)),
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

	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices)),
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
	D3D12_DESCRIPTOR_RANGE descriptor_ranges[2];
	descriptor_ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptor_ranges[0].NumDescriptors = 1;
	descriptor_ranges[0].BaseShaderRegister = 0;
	descriptor_ranges[0].RegisterSpace = 0;
	descriptor_ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descriptor_ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descriptor_ranges[1].NumDescriptors = 1;
	descriptor_ranges[1].BaseShaderRegister = 0;
	descriptor_ranges[1].RegisterSpace = 0;
	descriptor_ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER root_parameters[1];
	root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	root_parameters[0].DescriptorTable.NumDescriptorRanges = _countof(descriptor_ranges);
	root_parameters[0].DescriptorTable.pDescriptorRanges = descriptor_ranges;
	root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	D3D12_STATIC_SAMPLER_DESC static_samplers[1];
	static_samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	static_samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	static_samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	static_samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	static_samplers[0].MipLODBias = 0.0f;
	static_samplers[0].MaxAnisotropy = 0;
	static_samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	static_samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	static_samplers[0].MinLOD = 0.0f;
	static_samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	static_samplers[0].ShaderRegister = 0;
	static_samplers[0].RegisterSpace = 0;
	static_samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
	root_signature_desc.NumParameters = _countof(root_parameters);
	root_signature_desc.pParameters = root_parameters;
	root_signature_desc.NumStaticSamplers = _countof(static_samplers);
	root_signature_desc.pStaticSamplers = static_samplers;
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

	graphics_pipeline_state_desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	// graphics_pipeline_state_desc.BlendState.AlphaToCoverageEnable = FALSE;
	// graphics_pipeline_state_desc.BlendState.IndependentBlendEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].LogicOpEnable = FALSE;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	// graphics_pipeline_state_desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

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

	D3D12_INPUT_ELEMENT_DESC input_element_descs[2];
	input_element_descs[0].SemanticName = "POSITION";
	input_element_descs[0].SemanticIndex = 0;
	input_element_descs[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	input_element_descs[0].InputSlot = 0;
	input_element_descs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	input_element_descs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[0].InstanceDataStepRate = 0;

	input_element_descs[1].SemanticName = "TEXCOORD";
	input_element_descs[1].SemanticIndex = 0;
	input_element_descs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	input_element_descs[1].InputSlot = 0;
	input_element_descs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	input_element_descs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[1].InstanceDataStepRate = 0;


	graphics_pipeline_state_desc.InputLayout.pInputElementDescs = input_element_descs;
	graphics_pipeline_state_desc.InputLayout.NumElements = _countof(input_element_descs);

	graphics_pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	graphics_pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphics_pipeline_state_desc.NumRenderTargets = 1;

	graphics_pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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

bool RendererDX12::loadTexture()
{
	TexMetadata meta_data;
	ScratchImage scratch_image;

	HRESULT hr = LoadFromWICFile(L"img/textest.png", WIC_FLAGS_NONE, &meta_data, scratch_image);
	if(FAILED(hr))
	{
		return false;
	}

	auto image = scratch_image.GetImage(0, 0, 0);

	D3D12_HEAP_PROPERTIES heap_properties;
	heap_properties.Type = D3D12_HEAP_TYPE_CUSTOM;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resource_desc;
	resource_desc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(meta_data.dimension);
	resource_desc.Alignment = 0;
	resource_desc.Width = meta_data.width;
	resource_desc.Height = meta_data.height;
	resource_desc.DepthOrArraySize = meta_data.arraySize;
	resource_desc.MipLevels = meta_data.mipLevels;
	resource_desc.Format = meta_data.format;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = mpDevice->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&mpTexture)
	);
	if(FAILED(hr))
	{
		return false;
	}

	hr = mpTexture->WriteToSubresource(
		0,
		nullptr,
		image->pixels,
		image->rowPitch,
		image->slicePitch
	);
	if(FAILED(hr))
	{
		return false;
	}

	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.NumDescriptors = 2;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;

	hr = mpDevice->CreateDescriptorHeap(&descriptor_heap_desc, IID_PPV_ARGS(&mpSRVDescriptorHeap));
	if(FAILED(hr))
	{
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = meta_data.format;
	srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srv_desc.Texture2D.MostDetailedMip = 0;
	srv_desc.Texture2D.MipLevels = 1;
	srv_desc.Texture2D.PlaneSlice = 0;
	srv_desc.Texture2D.ResourceMinLODClamp = 0;

	mpDevice->CreateShaderResourceView(
		mpTexture.Get(),
		&srv_desc,
		mpSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return true;
}

bool RendererDX12::createConstantBuffer()
{
	mWorld = XMMatrixRotationY(XM_PIDIV4);

	XMFLOAT3 eye(0.0f, 0.0f, -5.0f);
	XMFLOAT3 at(0.0f, 0.0f, 0.0f);
	XMFLOAT3 up(0.0f, 1.0f, 0.0f);
	mView = XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&at),
		XMLoadFloat3(&up)
	);

	mProjection = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(mWidth) / static_cast<float>(mHeight),
		1.0f,
		10.0f
	);

	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(XMMATRIX) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mpConstantBuffer)
	);
	if(FAILED(hr))
	{
		return false;
	}

	hr = mpConstantBuffer->Map(0, nullptr, &mpMappedConstantBuffer);
	if(FAILED(hr))
	{
		return false;
	}

	*reinterpret_cast<XMMATRIX *>(mpMappedConstantBuffer) = XMMatrixTranspose(mWorld * mView * mProjection);

	auto handle = mpSRVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += mpDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc;
	constant_buffer_view_desc.BufferLocation = mpConstantBuffer->GetGPUVirtualAddress();
	constant_buffer_view_desc.SizeInBytes = mpConstantBuffer->GetDesc().Width;

	mpDevice->CreateConstantBufferView(&constant_buffer_view_desc, handle);

	return true;
}
