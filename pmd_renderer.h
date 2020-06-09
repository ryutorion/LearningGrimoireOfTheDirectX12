#pragma once
#ifndef PMD_RENDERER_H_INCLUDED
#define PMD_RENDERER_H_INCLUDED

#include <vector>
#include <memory>
#include <d3d12.h>
#include <wrl/client.h>
#include "pmd_actor.h"

class RendererDX12;

class PMDRenderer
{
public:
	bool initialize(RendererDX12 & renderer);
	void setup(RendererDX12 & renderer);
	void update();
	void draw(RendererDX12 & renderer);

	[[nodiscard]]
	PMDActor & addActor(const char * path_str, RendererDX12 & renderer);

	void startActorAnimation();

private:
	bool createRootSignature(RendererDX12 & renderer);
	bool createGraphicsPipelineState(RendererDX12 & renderer);
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> mpRootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> mpGraphicsPipelineState;
	std::vector<std::unique_ptr<PMDActor>> mpActors;
};

#endif // PMD_RENDERER_H_INCLUDED
