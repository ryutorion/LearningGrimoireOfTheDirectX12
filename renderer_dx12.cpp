#include "renderer_dx12.h"
#include <algorithm>
#include <fstream>
#include <regex>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>

using namespace std;
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

	if(!createDSVDescriptorHeap())
	{
		return false;
	}

	if(!createDSV())
	{
		return false;
	}

	if(!createFence())
	{
		return false;
	}

	if(!createNullWhite())
	{
		return false;
	}

	if(!createNullBlack())
	{
		return false;
	}

	if(!createNullGradation())
	{
		return false;
	}

	if(!loadModel())
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

	if(!createSceneDescriptorHeap())
	{
		return false;
	}

	if(!createSceneConstantBuffer())
	{
		return false;
	}

	return true;
}

bool RendererDX12::render()
{
	static float angle = 0.0f;

	*reinterpret_cast<SceneData *>(mpMappedSceneConstantBuffer) = mSceneData;

	beginDraw();

	mpGraphicsCommandList->SetPipelineState(mpGraphicsPipelineState.Get());

	mpGraphicsCommandList->SetGraphicsRootSignature(mpRootSignature.Get());

	mpGraphicsCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mpGraphicsCommandList->SetDescriptorHeaps(1, mpSceneDescriptorHeap.GetAddressOf());
	mpGraphicsCommandList->SetGraphicsRootDescriptorTable(0, mpSceneDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

	mpPMDActor->draw(*this);

	endDraw();

	mpSwapChain->Present(1, 0);

	return true;
}

bool RendererDX12::createDescriptorHeap(
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> & p_descriptor_heap,
	const D3D12_DESCRIPTOR_HEAP_DESC & descriptor_heap_desc
)
{
	HRESULT hr = mpDevice->CreateDescriptorHeap(
		&descriptor_heap_desc,
		IID_PPV_ARGS(&p_descriptor_heap)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createBuffer(Microsoft::WRL::ComPtr<ID3D12Resource> & p_dst, size_t buffer_size)
{
	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(buffer_size),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&p_dst)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

void RendererDX12::beginDraw()
{
	auto back_buffer_index = mpSwapChain->GetCurrentBackBufferIndex();

	mpGraphicsCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			mpBackBuffers[back_buffer_index].Get(),
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET
		)
	);

	auto rtv_handle = mpRTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtv_handle.ptr += back_buffer_index * mRTVDescriptorSize;

	auto dsv_handle = mpDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	mpGraphicsCommandList->OMSetRenderTargets(1, &rtv_handle, FALSE, &dsv_handle);

	float clear_color[] { 1.0f, 1.0f, 1.0f, 1.0f };
	mpGraphicsCommandList->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

	mpGraphicsCommandList->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	mpGraphicsCommandList->RSSetViewports(1, &mViewport);
	mpGraphicsCommandList->RSSetScissorRects(1, &mScissorRect);
}

void RendererDX12::setVertexBuffers(
	const uint32_t start_slot,
	const uint32_t view_count,
	const D3D12_VERTEX_BUFFER_VIEW * p_view_array
)
{
	mpGraphicsCommandList->IASetVertexBuffers(start_slot, view_count, p_view_array);
}

void RendererDX12::setIndexBuffer(const D3D12_INDEX_BUFFER_VIEW & index_buffer_view)
{
	mpGraphicsCommandList->IASetIndexBuffer(&index_buffer_view);
}

void RendererDX12::endDraw()
{
	auto back_buffer_index = mpSwapChain->GetCurrentBackBufferIndex();

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
		return ;
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
}

bool RendererDX12::enableDebugLayer()
{
#ifdef _DEBUG
	OutputDebugStringA("enableDebugLayer\n");
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
	OutputDebugStringA("createFactory\n");
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
	OutputDebugStringA("createDevice\n");

	HRESULT hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&mpDevice));
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createCommandAllocator()
{
	OutputDebugStringA("createCommandAllocator\n");

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
	OutputDebugStringA("createGraphicsCommandList\n");

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
	OutputDebugStringA("createCommandQueue\n");

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
	OutputDebugStringA("createSwapChain\n");

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
	OutputDebugStringA("createRTVDescriptorHeap\n");

	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descriptor_heap_desc.NumDescriptors = SwapChainBufferCount;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptor_heap_desc.NodeMask = 0;

	if(!createDescriptorHeap(mpRTVDescriptorHeap, descriptor_heap_desc))
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

		render_target_view_desc.Format = mpBackBuffers[i]->GetDesc().Format;

		mpDevice->CreateRenderTargetView(
			mpBackBuffers[i].Get(),
			&render_target_view_desc,
			handle
		);
		handle.ptr += mRTVDescriptorSize;
	}

	return true;
}

bool RendererDX12::createDSVDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	descriptor_heap_desc.NumDescriptors = 1;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	descriptor_heap_desc.NodeMask = 0;

	if(!createDescriptorHeap(mpDSVDescriptorHeap, descriptor_heap_desc))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createDSV()
{
	D3D12_HEAP_PROPERTIES heap_properties;
	heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_properties.CreationNodeMask = 0;
	heap_properties.VisibleNodeMask = 0;


	D3D12_RESOURCE_DESC resource_desc;
	resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resource_desc.Alignment = 0;
	resource_desc.Width = mWidth;
	resource_desc.Height = mHeight;
	resource_desc.DepthOrArraySize = 1;
	resource_desc.MipLevels = 1;
	resource_desc.Format = DXGI_FORMAT_D32_FLOAT;
	resource_desc.SampleDesc.Count = 1;
	resource_desc.SampleDesc.Quality = 0;
	resource_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resource_desc.Flags= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clear_value;
	clear_value.Format = resource_desc.Format;
	clear_value.DepthStencil.Depth = 1.0f;

	HRESULT hr = mpDevice->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clear_value,
		IID_PPV_ARGS(&mpDepthBuffer)
	);
	if(FAILED(hr))
	{
		return false;
	}

	D3D12_DEPTH_STENCIL_VIEW_DESC depth_stencil_view_desc;
	depth_stencil_view_desc.Format = resource_desc.Format;
	depth_stencil_view_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	depth_stencil_view_desc.Flags = D3D12_DSV_FLAG_NONE;
	depth_stencil_view_desc.Texture2D.MipSlice = 0;

	mpDevice->CreateDepthStencilView(
		mpDepthBuffer.Get(),
		&depth_stencil_view_desc,
		mpDSVDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

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

bool RendererDX12::createTexture(uint32_t width, uint32_t height, Microsoft::WRL::ComPtr<ID3D12Resource> & p_texture)
{
	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(
			D3D12_CPU_PAGE_PROPERTY_WRITE_BACK,
			D3D12_MEMORY_POOL_L0
		),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&p_texture)
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::loadTexture(
	ComPtr<ID3D12Resource> & p_texture,
	const filesystem::path & texture_path
)
{
	OutputDebugStringA("Load Texture : ");
	OutputDebugStringW(texture_path.c_str());

	auto cache = mTextureCache.find(texture_path);
	if(cache != mTextureCache.end())
	{
		p_texture = cache->second;

		OutputDebugStringA(" succeeded. (cache)\n");

		return true;
	}

	TexMetadata meta_data;
	ScratchImage scratch_image;

	HRESULT hr = LoadFromWICFile(
		texture_path.wstring().c_str(),
		WIC_FLAGS_NONE,
		&meta_data,
		scratch_image
	);
	if(FAILED(hr))
	{
		OutputDebugStringA(" failed.\n");
		return false;
	}

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

	ComPtr<ID3D12Resource> p_tmp_texture;
	hr = mpDevice->CreateCommittedResource(
		&heap_properties,
		D3D12_HEAP_FLAG_NONE,
		&resource_desc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&p_tmp_texture)
	);
	if(FAILED(hr))
	{
		OutputDebugStringA(" failed.\n");
		return false;
	}

	auto * p = new XMMATRIX;

	auto p_image = scratch_image.GetImage(0, 0, 0);
	hr = p_tmp_texture->WriteToSubresource(
		0,
		nullptr,
		p_image->pixels,
		p_image->rowPitch,
		p_image->slicePitch
	);
	if(FAILED(hr))
	{
		return false;
	}

	OutputDebugStringA(" succeeded.\n");

	p_texture = p_tmp_texture;
	mTextureCache[texture_path] = p_tmp_texture;

	return true;
}

void RendererDX12::createConstantBufferView(
	const D3D12_GPU_VIRTUAL_ADDRESS buffer_location,
	uint32_t size_in_bytes,
	const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle
)
{
	D3D12_CONSTANT_BUFFER_VIEW_DESC constant_buffer_view_desc;
	constant_buffer_view_desc.BufferLocation = buffer_location;
	constant_buffer_view_desc.SizeInBytes = size_in_bytes;

	mpDevice->CreateConstantBufferView(&constant_buffer_view_desc, cpu_descriptor_handle);
}

void RendererDX12::createTextureResourceView(Microsoft::WRL::ComPtr<ID3D12Resource> & p_texture, const D3D12_CPU_DESCRIPTOR_HANDLE cpu_desciptor_handle)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC shader_resource_view_desc;
	shader_resource_view_desc.Format = p_texture->GetDesc().Format;
	shader_resource_view_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	shader_resource_view_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	shader_resource_view_desc.Texture2D.MostDetailedMip = 0;
	shader_resource_view_desc.Texture2D.MipLevels = 1;
	shader_resource_view_desc.Texture2D.PlaneSlice = 0;
	shader_resource_view_desc.Texture2D.ResourceMinLODClamp = 0.0f;

	mpDevice->CreateShaderResourceView(
		p_texture.Get(),
		&shader_resource_view_desc,
		cpu_desciptor_handle
	);
}

uint32_t RendererDX12::getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type)
{
	return mpDevice->GetDescriptorHandleIncrementSize(descriptor_heap_type);
}

void RendererDX12::setDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> & p_descriptor_heap)
{
	mpGraphicsCommandList->SetDescriptorHeaps(1, p_descriptor_heap.GetAddressOf());
}

void RendererDX12::setGraphicsRootDescriptorTable(const uint32_t root_parameter_index, D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor)
{
	mpGraphicsCommandList->SetGraphicsRootDescriptorTable(root_parameter_index, base_descriptor);
}

void RendererDX12::drawIndexedInstanced(
	uint32_t index_count_per_instance,
	uint32_t instance_count,
	uint32_t start_index_location,
	int32_t base_vertex_location,
	uint32_t start_instance_location
)
{
	mpGraphicsCommandList->DrawIndexedInstanced(
		index_count_per_instance,
		instance_count,
		start_index_location,
		base_vertex_location,
		start_index_location
	);
}

bool RendererDX12::loadModel()
{
	mpPMDActor.reset(new PMDActor("model/miku.metal.pmd", *this));

	return true;
}

bool RendererDX12::createRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE descriptor_ranges[4];
	descriptor_ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descriptor_ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	descriptor_ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descriptor_ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

	CD3DX12_ROOT_PARAMETER root_parameters[3];
	root_parameters[0].InitAsDescriptorTable(
		1,
		&descriptor_ranges[0],
		D3D12_SHADER_VISIBILITY_VERTEX
	);
	root_parameters[1].InitAsDescriptorTable(
		1,
		&descriptor_ranges[1],
		D3D12_SHADER_VISIBILITY_VERTEX
	);
	root_parameters[2].InitAsDescriptorTable(
		2,
		&descriptor_ranges[2],
		D3D12_SHADER_VISIBILITY_PIXEL
	);

	CD3DX12_STATIC_SAMPLER_DESC static_sampler_descs[2];
	static_sampler_descs[0].Init(0);
	static_sampler_descs[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	static_sampler_descs[1].Init(
		1,
		D3D12_FILTER_ANISOTROPIC,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP
	);
	static_sampler_descs[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC root_signature_desc;
	root_signature_desc.NumParameters = _countof(root_parameters);
	root_signature_desc.pParameters = root_parameters;
	root_signature_desc.NumStaticSamplers = _countof(static_sampler_descs);
	root_signature_desc.pStaticSamplers = static_sampler_descs;
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
		OutputDebugStringA(reinterpret_cast<LPCSTR>(p_error_blob->GetBufferPointer()));
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
	graphics_pipeline_state_desc.BlendState.RenderTarget[0].BlendEnable = FALSE;
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
	graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	graphics_pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	graphics_pipeline_state_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	graphics_pipeline_state_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	graphics_pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	graphics_pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	graphics_pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;
	graphics_pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	graphics_pipeline_state_desc.RasterizerState.ForcedSampleCount = 0;
	graphics_pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	graphics_pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;
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

	D3D12_INPUT_ELEMENT_DESC input_element_descs[5];
	input_element_descs[0].SemanticName = "POSITION";
	input_element_descs[0].SemanticIndex = 0;
	input_element_descs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	input_element_descs[0].InputSlot = 0;
	input_element_descs[0].AlignedByteOffset = offsetof(Vertex, position);
	input_element_descs[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[0].InstanceDataStepRate = 0;

	input_element_descs[1].SemanticName = "NORMAL";
	input_element_descs[1].SemanticIndex = 0;
	input_element_descs[1].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	input_element_descs[1].InputSlot = 0;
	input_element_descs[1].AlignedByteOffset = offsetof(Vertex, normal);
	input_element_descs[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[1].InstanceDataStepRate = 0;

	input_element_descs[2].SemanticName = "TEXCOORD";
	input_element_descs[2].SemanticIndex = 0;
	input_element_descs[2].Format = DXGI_FORMAT_R32G32_FLOAT;
	input_element_descs[2].InputSlot = 0;
	input_element_descs[2].AlignedByteOffset = offsetof(Vertex, uv);
	input_element_descs[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[2].InstanceDataStepRate = 0;

	input_element_descs[3].SemanticName = "BONES";
	input_element_descs[3].SemanticIndex = 0;
	input_element_descs[3].Format = DXGI_FORMAT_R16G16_UINT;
	input_element_descs[3].InputSlot = 0;
	input_element_descs[3].AlignedByteOffset = offsetof(Vertex, bones);
	input_element_descs[3].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[3].InstanceDataStepRate = 0;

	input_element_descs[4].SemanticName = "WEIGHT";
	input_element_descs[4].SemanticIndex = 0;
	input_element_descs[4].Format = DXGI_FORMAT_R8G8_UINT;
	input_element_descs[4].InputSlot = 0;
	input_element_descs[4].AlignedByteOffset = offsetof(Vertex, weight);
	input_element_descs[4].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	input_element_descs[4].InstanceDataStepRate = 0;

	graphics_pipeline_state_desc.InputLayout.pInputElementDescs = input_element_descs;
	graphics_pipeline_state_desc.InputLayout.NumElements = _countof(input_element_descs);

	graphics_pipeline_state_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	graphics_pipeline_state_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphics_pipeline_state_desc.NumRenderTargets = 1;

	graphics_pipeline_state_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	graphics_pipeline_state_desc.RTVFormats[1] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[2] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[3] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[4] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[5] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[6] = DXGI_FORMAT_UNKNOWN;
	graphics_pipeline_state_desc.RTVFormats[7] = DXGI_FORMAT_UNKNOWN;

	graphics_pipeline_state_desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

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

bool RendererDX12::createSceneDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.NumDescriptors = 1;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;

	if(!createDescriptorHeap(mpSceneDescriptorHeap, descriptor_heap_desc))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createSceneConstantBuffer()
{
	XMFLOAT3 eye(0.0f, 16.0f, -6.0f);
	XMFLOAT3 at(0.0f, 16.0f, 0.0f);
	XMFLOAT3 up(0.0f, 1.0f, 0.0f);
	mSceneData.view = XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&at),
		XMLoadFloat3(&up)
	);
	mSceneData.view = XMMatrixTranspose(mSceneData.view);

	mSceneData.projection = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,
		static_cast<float>(mWidth) / static_cast<float>(mHeight),
		1.0f,
		100.0f
	);
	mSceneData.projection = XMMatrixTranspose(mSceneData.projection);
	mSceneData.eye = XMVectorSet(eye.x, eye.y, eye.z, 0.0f);

	HRESULT hr = mpDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer((sizeof(mSceneData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mpSceneConstantBuffer)
	);
	if(FAILED(hr))
	{
		return false;
	}

	hr = mpSceneConstantBuffer->Map(0, nullptr, &mpMappedSceneConstantBuffer);
	if(FAILED(hr))
	{
		return false;
	}

	*reinterpret_cast<SceneData *>(mpMappedSceneConstantBuffer) = mSceneData;

	createConstantBufferView(
		mpSceneConstantBuffer->GetGPUVirtualAddress(),
		mpSceneConstantBuffer->GetDesc().Width,
		mpSceneDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return true;
}

bool RendererDX12::createNullWhite()
{
	if(!createTexture(4, 4, mpNullWhite))
	{
		return false;
	}

	std::array<uint8_t, 4 * 4 * 4> buffer;
	buffer.fill(0xff);

	HRESULT hr = mpNullWhite->WriteToSubresource(0, nullptr, buffer.data(), 4 * 4, buffer.size());
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createNullBlack()
{
	if(!createTexture(4, 4, mpNullBlack))
	{
		return false;
	}
	std::array<uint8_t, 4 * 4 * 4> buffer;
	buffer.fill(0x00);

	HRESULT hr = mpNullBlack->WriteToSubresource(0, nullptr, buffer.data(), 4 * 4, buffer.size());
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}

bool RendererDX12::createNullGradation()
{
	if(!createTexture(4, 256, mpNullGradation))
	{
		return false;
	}

	array<uint32_t, 4 * 256> buffer;
	uint32_t c = 0xff;
	for(auto it = buffer.begin(); it != buffer.end(); it += 4)
	{
		auto col = (c << 24) | (c << 16) | (c << 8) | c;
		fill(it, it + 4, col);
		--c;
	}

	HRESULT hr = mpNullGradation->WriteToSubresource(
		0,
		nullptr,
		buffer.data(),
		4 * sizeof(buffer[0]),
		sizeof(buffer[0]) * buffer.size()
	);
	if(FAILED(hr))
	{
		return false;
	}

	return true;
}
