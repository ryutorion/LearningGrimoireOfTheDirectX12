#pragma once
#ifndef PMD_RENDERER_H_INCLUDED
#define PMD_RENDERER_H_INCLUDED

#include <d3d12.h>
#include <wrl/client.h>

class RendererDX12;

class PMDRenderer
{
public:
	bool initialize(RendererDX12 & renderer);
	void setup(RendererDX12 & renderer);
private:
	bool createRootSignature(RendererDX12 & renderer);
	bool createGraphicsPipelineState(RendererDX12 & renderer);
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mpRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mpGraphicsPipelineState;
};

#endif // PMD_RENDERER_H_INCLUDED
