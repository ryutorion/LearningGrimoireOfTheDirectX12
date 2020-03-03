#ifndef RENDERER_DX12_H_INCLUDED
#define RENDERER_DX12_H_INCLUDED

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include "pmd_actor.h"

class RendererDX12
{
public:
	~RendererDX12();

	bool initialize(uint32_t width, uint32_t height, HWND hWnd);
	bool render();

	bool createDescriptorHeap(
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> & p_descriptor_heap,
		const D3D12_DESCRIPTOR_HEAP_DESC & descriptor_heap_desc
	);

	bool createBuffer(
		Microsoft::WRL::ComPtr<ID3D12Resource> & p_dst,
		size_t buffer_size
	);

	bool loadTexture(
		Microsoft::WRL::ComPtr<ID3D12Resource> & p_texture,
		const std::filesystem::path & texture_path
	);

	void createConstantBufferView(
		const D3D12_GPU_VIRTUAL_ADDRESS buffer_location,
		uint32_t size_in_bytes,
		const D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle
	);

	void createTextureResourceView(
		Microsoft::WRL::ComPtr<ID3D12Resource> & p_texture,
		const D3D12_CPU_DESCRIPTOR_HANDLE cpu_desciptor_handle
	);

	uint32_t getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type);

	void beginDraw();
	void setVertexBuffers(
		const uint32_t start_slot,
		const uint32_t view_count,
		const D3D12_VERTEX_BUFFER_VIEW * p_view_array
	);
	void setIndexBuffer(const D3D12_INDEX_BUFFER_VIEW & index_buffer_view);
	void setDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> & p_descriptor_heap);
	void setGraphicsRootDescriptorTable(
		const uint32_t root_parameter_index,
		D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor
	);
	void drawIndexedInstanced(
		uint32_t index_count_per_instance,
		uint32_t instance_count,
		uint32_t start_index_location,
		int32_t base_vertex_location,
		uint32_t start_instance_location
	);
	void endDraw();

	const Microsoft::WRL::ComPtr<ID3D12Resource> & getNullWhite() const { return mpNullWhite; }
	const Microsoft::WRL::ComPtr<ID3D12Resource> & getNullBlack() const { return mpNullBlack; }
	const Microsoft::WRL::ComPtr<ID3D12Resource> & getNullGradation() const { return mpNullGradation; }

private:
	bool enableDebugLayer();
	bool createFactory();
	bool createDevice();
	bool createCommandAllocator();
	bool createGraphicsCommandList();
	bool createCommandQueue();
	bool createSwapChain(HWND hWnd);
	bool createRTVDescriptorHeap();
	bool createRTVs();
	bool createDSVDescriptorHeap();
	bool createDSV();
	bool createFence();
	bool createTexture(
		uint32_t width,
		uint32_t height,
		Microsoft::WRL::ComPtr<ID3D12Resource> & p_texture
	);
	bool createNullWhite();
	bool createNullBlack();
	bool createNullGradation();

	bool loadModel();
	bool createRootSignature();
	bool createGraphicsPipelineState();
	bool createSceneDescriptorHeap();
	bool createSceneConstantBuffer();
private:
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory2> mpFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter> mpAdapter;

	Microsoft::WRL::ComPtr<ID3D12Device> mpDevice;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mpCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mpGraphicsCommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mpCommandQueue;

	DXGI_FORMAT mSwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr uint32_t SwapChainBufferCount = 2;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> mpSwapChain;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpRTVDescriptorHeap;
	uint32_t mRTVDescriptorSize = 0;

	std::array<
		Microsoft::WRL::ComPtr<ID3D12Resource>,
		SwapChainBufferCount
	> mpBackBuffers;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpDepthBuffer;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpDSVDescriptorHeap;

	uint64_t mFenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> mpFence;
	HANDLE mhFenceEvent = nullptr;

	struct Vertex
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR normal;
		DirectX::XMFLOAT2 uv;
		uint16_t bones[2];
		uint8_t weight;
		uint8_t edge;
	};

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mpRootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mpGraphicsPipelineState;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpSceneDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpSceneConstantBuffer;
	void * mpMappedSceneConstantBuffer = nullptr;

	struct SceneData
	{
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX projection;
		DirectX::XMVECTOR eye;
	};
	SceneData mSceneData;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpNullWhite;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpNullBlack;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpNullGradation;

	std::unique_ptr<PMDActor> mpPMDActor;

	std::map<
		std::filesystem::path,
		Microsoft::WRL::ComPtr<ID3D12Resource>
	> mTextureCache;
};

#endif // RENDERER_DX12_H_INCLUDED