#pragma once
#ifndef PMD_ACTOR_H_INCLUDED
#define PMD_ACTOR_H_INCLUDED

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class RendererDX12;

class PMDActor
{
public:
	PMDActor(const char * pathStr, RendererDX12 & renderer);

	void draw(RendererDX12 & renderer);

private:
	bool createTransformDescriptorHeap(RendererDX12 & renderer);
	bool createTransformConstantBuffer(RendererDX12 & renderer);
	bool load(const char * pathStr, RendererDX12 & renderer);
	bool loadHeader(std::ifstream & fin);
	bool loadVertices(std::ifstream & fin, RendererDX12 & renderer);
	bool loadIndices(std::ifstream & fin, RendererDX12 & renderer);

	struct Material;
	static bool loadTexture(
		Material & material,
		const std::filesystem::path & texture_path,
		RendererDX12 & renderer
	);

	bool loadMaterials(
		std::ifstream & fin,
		RendererDX12 & renderer,
		const std::filesystem::path & root_path
	);

	bool createMaterialDescriptorHeap(RendererDX12 & renderer);
	bool createMaterialResourceViews(RendererDX12 & renderer);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mpVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW mVertexBufferView;

	Microsoft::WRL::ComPtr<ID3D12Resource> mpIndexBuffer;
	D3D12_INDEX_BUFFER_VIEW mIndexBufferView;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpTransformDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpTransformConstantBuffer;
	DirectX::XMMATRIX * mpMappedTransform = nullptr;

	struct MaterialConstantBuffer
	{
		DirectX::XMVECTOR diffuse;
		DirectX::XMVECTOR specular;
		DirectX::XMVECTOR ambient;
	};

	struct Material
	{
		uint32_t indexCount;
		MaterialConstantBuffer constantBuffer;
		Microsoft::WRL::ComPtr<ID3D12Resource> pTexture;
		Microsoft::WRL::ComPtr<ID3D12Resource> pMultipleSphereMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> pAdditiveSphereMap;
		Microsoft::WRL::ComPtr<ID3D12Resource> pToon;
	};

	std::vector<Material> mMaterials;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpMaterialDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpMaterialConstantBuffer;
};

#endif // PMD_ACTOR_H_INCLUDED
