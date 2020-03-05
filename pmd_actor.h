#pragma once
#ifndef PMD_ACTOR_H_INCLUDED
#define PMD_ACTOR_H_INCLUDED

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>
#include <string>
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

class RendererDX12;

class PMDActor
{
public:
	PMDActor(const char * path_str, RendererDX12 & renderer);

	bool loadVMD(const char * path_str);

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

	bool loadBones(std::ifstream & fin, RendererDX12 & renderer);

	bool createMaterialDescriptorHeap(RendererDX12 & renderer);
	bool createMaterialResourceViews(RendererDX12 & renderer);

	struct BoneNode;
	void multiplyMatrixRecursively(const BoneNode & bone, const DirectX::XMMATRIX & parent_matrix);

	void updateMotion();

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

	std::vector<DirectX::XMMATRIX> mBoneMatrices;

	struct BoneNode
	{
		int32_t boneIndex;
		DirectX::XMFLOAT3 startPosition;
		std::vector<BoneNode *> children;
	};
	std::map<std::string, BoneNode> mNameToBoneMap;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpMaterialDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpMaterialConstantBuffer;

	struct KeyFrame
	{
		uint32_t frameNo;
		DirectX::XMVECTOR quaternion;

		KeyFrame(const uint32_t frame_no, const DirectX::XMVECTOR & q)
			: frameNo(frame_no)
			, quaternion(q)
		{}
	};

	std::unordered_map<std::string, std::vector<KeyFrame>> mNameToKeyFrameMap;
};

#endif // PMD_ACTOR_H_INCLUDED
