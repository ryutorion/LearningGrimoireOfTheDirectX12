#ifndef RENDERER_DX12_H_INCLUDED
#define RENDERER_DX12_H_INCLUDED

#include <array>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class RendererDX12
{
public:
	~RendererDX12();

	bool initialize(uint32_t width, uint32_t height, HWND hWnd);
	bool render();
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
	bool createFence();
	bool createVertexBuffer();
	bool createIndexBuffer();
	bool createRootSignature();
	bool createGraphicsPipelineState();
	bool loadTexture();
private:
	uint32_t mWidth = 0;
	uint32_t mHeight = 0;

	Microsoft::WRL::ComPtr<IDXGIFactory7> mpFactory;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> mpAdapter;

	Microsoft::WRL::ComPtr<ID3D12Device6> mpDevice;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mpCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList5> mpGraphicsCommandList;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> mpCommandQueue;

	DXGI_FORMAT mSwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr uint32_t SwapChainBufferCount = 2;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> mpSwapChain;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpRTVDescriptorHeap;
	uint32_t mRTVDescriptorSize = 0;

	std::array<
		Microsoft::WRL::ComPtr<ID3D12Resource>,
		SwapChainBufferCount
	> mpBackBuffers;

	uint64_t mFenceValue = 0;
	Microsoft::WRL::ComPtr<ID3D12Fence> mpFence;
	HANDLE mhFenceEvent = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> mpRootSignature;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> mpGraphicsPipelineState;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpTexture;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpSRVDescriptorHeap;
};

#endif // RENDERER_DX12_H_INCLUDED