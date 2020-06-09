#pragma once
#ifndef PMD_ACTOR_H_INCLUDED
#define PMD_ACTOR_H_INCLUDED

#include <cstdint>
#include <chrono>
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

	void update();

	void draw(RendererDX12 & renderer);

	void startAnimation();

	void setPosition(float x, float y, float z);
	void setEulerAngle(float x, float y, float z);

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

	bool loadIK(std::ifstream & fin);

	bool createMaterialDescriptorHeap(RendererDX12 & renderer);
	bool createMaterialResourceViews(RendererDX12 & renderer);

	struct BoneNode;
	void multiplyMatrixRecursively(const uint32_t boneIndex, const DirectX::XMMATRIX & parent_matrix);

	struct IK;
	void solveLookAt(const IK & ik);
	void solveCosineIK(const IK & ik);
	void solveCCDIK(const IK & ik);
	void solveIK();

	void updateMotion();

private:
	DirectX::XMFLOAT3 mPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	DirectX::XMFLOAT3 mEulerAngle = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

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

	enum class BoneType : uint32_t
	{
		Rotation,
		RotAndMove,
		IK,
		Undefined,
		IKChild,
		RotationChild,
		IKDestination,
		Invisible,
	};

	struct BoneNode
	{
		BoneType boneType;
		int32_t ikParentBone;
		DirectX::XMFLOAT3 startPosition;
		std::string boneName;
		std::vector<uint32_t> children;
	};
	std::vector<BoneNode> mBones;
	std::map<std::string, uint32_t> mNameToBoneIndex;

	struct IK
	{
		uint16_t boneIndex;
		uint16_t targetIndex;
		uint16_t iterations;
		float limit;
		std::vector<uint16_t> nodeIndices;
	};
	std::vector<IK> mIKs;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mpMaterialDescriptorHeap;
	Microsoft::WRL::ComPtr<ID3D12Resource> mpMaterialConstantBuffer;

	struct KeyFrame
	{
		uint32_t frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT3 offset;
		DirectX::XMFLOAT2 p1;
		DirectX::XMFLOAT2 p2;

		KeyFrame(
			const uint32_t frame_no,
			const DirectX::XMVECTOR & q,
			const DirectX::XMFLOAT3 & ofs,
			const DirectX::XMFLOAT2 & vp1,
			const DirectX::XMFLOAT2 & vp2
		)
			: frameNo(frame_no)
			, quaternion(q)
			, offset(ofs)
			, p1(vp1)
			, p2(vp2)
		{}

		KeyFrame(const KeyFrame & key_frame)
			: frameNo(key_frame.frameNo)
			, quaternion(key_frame.quaternion)
			, offset(key_frame.offset)
			, p1(key_frame.p1)
			, p2(key_frame.p2)
		{}
	};

	struct IKEnable
	{
		uint32_t frameNo;
		std::vector<bool> enables;
	};
	std::vector<IKEnable> mIKEnables;

	std::unordered_map<std::string, std::vector<KeyFrame>> mNameToKeyFrameMap;

	std::chrono::high_resolution_clock::time_point mStartTime;

	uint32_t mMaxFrame = 0;
};

#endif // PMD_ACTOR_H_INCLUDED
