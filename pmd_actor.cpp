#include "pmd_actor.h"
#include "renderer_dx12.h"
#include "pmd.h"
#include <algorithm>
#include <d3dx12.h>

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

PMDActor::PMDActor(const char * path_str, RendererDX12 & renderer)
{
	if(!load(path_str, renderer))
	{
		return ;
	}

	if(!createTransformDescriptorHeap(renderer))
	{
		return ;
	}

	if(!createTransformConstantBuffer(renderer))
	{
		return ;
	}

	if(!createMaterialDescriptorHeap(renderer))
	{
		return ;
	}

	if(!createMaterialResourceViews(renderer))
	{
		return ;
	}
}

bool PMDActor::loadVMD(const char * path_str)
{
	ifstream fin(path_str, ios::in | ios::binary);
	if(!fin)
	{
		return false;
	}

	// ヘッダをスキップ
	fin.seekg(50, ios::beg);

	uint32_t key_frame_count = 0;
	fin.read(reinterpret_cast<char *>(&key_frame_count), sizeof(key_frame_count));

#pragma pack(push, 1)
	struct VMDKeyFrame
	{
		char boneName[15];
		uint32_t frameNo;
		XMFLOAT3 location;
		XMFLOAT4 quaternion;
		uint8_t bezier[64];
	};
#pragma pack(pop)
	vector<VMDKeyFrame> vmd_key_frames(key_frame_count);
	fin.read(
		reinterpret_cast<char *>(vmd_key_frames.data()),
		sizeof(vmd_key_frames[0]) * vmd_key_frames.size()
	);

	for(auto & vmd_key_frame : vmd_key_frames)
	{
		mNameToKeyFrameMap[vmd_key_frame.boneName].emplace_back(
			vmd_key_frame.frameNo,
			XMLoadFloat4(&vmd_key_frame.quaternion),
			XMFLOAT2(vmd_key_frame.bezier[3] / 127.0f, vmd_key_frame.bezier[7] / 127.0f),
			XMFLOAT2(vmd_key_frame.bezier[11] / 127.0f, vmd_key_frame.bezier[15] / 127.0f)
		);
	}

	mMaxFrame = 0;
	for(auto & kv : mNameToKeyFrameMap)
	{
		std::sort(
			kv.second.begin(),
			kv.second.end(),
			[](const KeyFrame & lhs, const KeyFrame & rhs)
			{
				return lhs.frameNo < rhs.frameNo;
			}
		);

		mMaxFrame = max(mMaxFrame, kv.second[kv.second.size() - 1].frameNo);
	}

	return true;
}

void PMDActor::update()
{
	updateMotion();
}

void PMDActor::draw(RendererDX12 & renderer)
{
	renderer.setVertexBuffers(0, 1, &mVertexBufferView);
	renderer.setIndexBuffer(mIndexBufferView);

	renderer.setDescriptorHeap(mpTransformDescriptorHeap);
	renderer.setGraphicsRootDescriptorTable(
		1,
		mpTransformDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	renderer.setDescriptorHeap(mpMaterialDescriptorHeap);

	auto material_handle = mpMaterialDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto material_handle_size = renderer.getDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	uint32_t index_offset = 0;
	for(auto & m : mMaterials)
	{
		renderer.setGraphicsRootDescriptorTable(2, material_handle);
		renderer.drawIndexedInstanced(m.indexCount, 1, index_offset, 0, 0);

		material_handle.ptr += material_handle_size * 5;
		index_offset += m.indexCount;
	}
}

void PMDActor::startAnimation()
{
	mStartTime = chrono::high_resolution_clock::now();
}

bool PMDActor::createTransformDescriptorHeap(RendererDX12 & renderer)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.NumDescriptors = 1;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;

	if(!renderer.createDescriptorHeap(mpTransformDescriptorHeap, descriptor_heap_desc))
	{
		return false;
	}

	return true;
}

bool PMDActor::createTransformConstantBuffer(RendererDX12 & renderer)
{
	auto buffer_size = (sizeof(XMMATRIX) * (1 + mBoneMatrices.size()) + 0xff) & ~0xff;
	if(!renderer.createBuffer(mpTransformConstantBuffer, buffer_size))
	{
		return false;
	}

	HRESULT hr = mpTransformConstantBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void **>(&mpMappedTransform)
	);
	if(FAILED(hr))
	{
		return false;
	}

	for(auto & kv : mNameToKeyFrameMap)
	{
		auto & bone = mNameToBoneMap[kv.first];
		auto & bone_position = bone.startPosition;
		mBoneMatrices[bone.boneIndex] =
			XMMatrixTranslation(-bone_position.x, -bone_position.y, -bone_position.z) *
			XMMatrixRotationQuaternion(kv.second[0].quaternion) *
			XMMatrixTranslation(bone_position.x, bone_position.y, bone_position.z);
	}

	multiplyMatrixRecursively(mNameToBoneMap["センター"], XMMatrixIdentity());

	*mpMappedTransform = XMMatrixIdentity();
	copy(mBoneMatrices.begin(), mBoneMatrices.end(), mpMappedTransform + 1);

	renderer.createConstantBufferView(
		mpTransformConstantBuffer->GetGPUVirtualAddress(),
		buffer_size,
		mpTransformDescriptorHeap->GetCPUDescriptorHandleForHeapStart()
	);

	return true;
}

bool PMDActor::load(const char * pathStr, RendererDX12 & renderer)
{
	filesystem::path root_path = filesystem::path(pathStr).parent_path();
	ifstream fin(pathStr, ios::in | ios::binary);
	if(!fin)
	{
		return false;
	}

	if(!loadHeader(fin))
	{
		return false;
	}

	if(!loadVertices(fin, renderer))
	{
		return false;
	}

	if(!loadIndices(fin, renderer))
	{
		return false;
	}

	if(!loadMaterials(fin, renderer, root_path))
	{
		return false;
	}

	if(!loadBones(fin, renderer))
	{
		return false;
	}

	return true;
}

bool PMDActor::loadHeader(std::ifstream & fin)
{
#pragma pack(push, 1)
	struct PMDHeader
	{
		char signature[3];
		float version;
		char modelName[20];
		char comment[256];
	};
#pragma pack(pop)

	PMDHeader header;
	fin.read(reinterpret_cast<char *>(&header), sizeof(header));

	if(strncmp(header.signature, "Pmd", 3) != 0)
	{
		return false;
	}

	return true;
}

bool PMDActor::loadVertices(std::ifstream & fin, RendererDX12 & renderer)
{
	uint32_t vertex_count = 0;
	fin.read(reinterpret_cast<char *>(&vertex_count), sizeof(vertex_count));

#pragma pack(push, 1)
	struct PMDVertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		XMFLOAT2 uv;
		uint16_t boneNo[2];
		uint8_t boneWeight;
		uint8_t edgeFlag;
	};
#pragma pack(pop)

	vector<PMDVertex> pmd_vertices(vertex_count);
	fin.read(
		reinterpret_cast<char *>(pmd_vertices.data()),
		sizeof(pmd_vertices[0]) * pmd_vertices.size()
	);

	using Vertex = pmd::Vertex;

	vector<Vertex> vertices(vertex_count);
	for(uint32_t i = 0; i < vertex_count; ++i)
	{
		const PMDVertex & src = pmd_vertices[i];
		Vertex & dst = vertices[i];

		dst.position = XMVectorSet(src.position.x, src.position.y, src.position.z, 1.0f);
		dst.normal = XMLoadFloat3(&src.normal);
		dst.uv = src.uv;
		dst.bones[0] = src.boneNo[0];
		dst.bones[1] = src.boneNo[1];
		dst.weight = src.boneWeight;
		dst.edge = src.edgeFlag;
	}

	auto buffer_size = sizeof(vertices[0]) * vertices.size();
	if(!renderer.createBuffer(mpVertexBuffer, buffer_size))
	{
		return false;
	}

	void * p_dst = nullptr;
	HRESULT hr = mpVertexBuffer->Map(0, nullptr, &p_dst);
	if(FAILED(hr))
	{
		return false;
	}

	memcpy(p_dst, vertices.data(), buffer_size);

	mpVertexBuffer->Unmap(0, nullptr);

	mVertexBufferView.BufferLocation = mpVertexBuffer->GetGPUVirtualAddress();
	mVertexBufferView.SizeInBytes = buffer_size;
	mVertexBufferView.StrideInBytes = sizeof(vertices[0]);

	return true;
}

bool PMDActor::loadIndices(std::ifstream & fin, RendererDX12 & renderer)
{
	uint32_t index_count = 0;
	fin.read(reinterpret_cast<char *>(&index_count), sizeof(index_count));

	vector<uint16_t> indices(index_count);
	fin.read(
		reinterpret_cast<char *>(indices.data()),
		sizeof(indices[0]) * indices.size()
	);

	auto buffer_size = sizeof(indices[0]) * indices.size();
	if(!renderer.createBuffer(mpIndexBuffer, buffer_size))
	{
		return false;
	}

	void * p_dst = nullptr;
	HRESULT hr = mpIndexBuffer->Map(0, nullptr, &p_dst);
	if(FAILED(hr))
	{
		return false;
	}

	memcpy(p_dst, indices.data(), buffer_size);

	mpIndexBuffer->Unmap(0, nullptr);

	mIndexBufferView.BufferLocation = mpIndexBuffer->GetGPUVirtualAddress();
	mIndexBufferView.SizeInBytes = buffer_size;
	mIndexBufferView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

bool PMDActor::loadTexture(
	Material & material,
	const std::filesystem::path & texture_path,
	RendererDX12 & renderer
)
{
	ComPtr<ID3D12Resource> p_texture;
	if(!renderer.loadTexture(p_texture, texture_path))
	{
		return false;
	}

	auto extension = texture_path.extension();
	if(extension == ".bmp" || extension == ".png")
	{
		material.pTexture = p_texture;
	}
	else if(extension == ".sph")
	{
		material.pMultipleSphereMap = p_texture;
	}
	else if(extension == ".spa")
	{
		material.pAdditiveSphereMap = p_texture;
	}

	return true;
}

bool PMDActor::loadMaterials(
	std::ifstream & fin,
	RendererDX12 & renderer,
	const std::filesystem::path & root_path
)
{
	uint32_t material_count = 0;
	fin.read(reinterpret_cast<char *>(&material_count), sizeof(material_count));

#include <pshpack1.h>
	struct PMDMaterial
	{
		XMFLOAT3 diffuse;
		float diffuseAlpha;
		float specularity;
		XMFLOAT3 specular;
		XMFLOAT3 ambient;
		unsigned char toonIndex;
		unsigned char edgeFlag;
		unsigned int indexCount;
		char textureFilePath[20];
	};
#include <poppack.h>

	vector<PMDMaterial> pmd_materials(material_count);
	fin.read(
		reinterpret_cast<char *>(pmd_materials.data()),
		sizeof(pmd_materials[0]) * pmd_materials.size()
	);

	mMaterials.resize(material_count);
	for(uint32_t i = 0; i < material_count; ++i)
	{
		auto & src = pmd_materials[i];
		auto & dst = mMaterials[i];

		dst.indexCount = src.indexCount;

		dst.constantBuffer.diffuse = XMVectorSet(
			src.diffuse.x,
			src.diffuse.y,
			src.diffuse.z,
			src.diffuseAlpha
		);

		dst.constantBuffer.specular = XMVectorSet(
			src.specular.x,
			src.specular.y,
			src.specular.z,
			src.specularity
		);

		dst.constantBuffer.ambient = XMVectorSet(
			src.ambient.x,
			src.ambient.y,
			src.ambient.z,
			1.0f
		);

		dst.pTexture = renderer.getNullWhite();
		dst.pMultipleSphereMap = renderer.getNullWhite();
		dst.pAdditiveSphereMap = renderer.getNullBlack();

		unsigned char toon_index = src.toonIndex + 1;
		char toon_path[256];
		snprintf(toon_path, sizeof(toon_path), "toon/toon%02d.bmp", toon_index);
		if(!renderer.loadTexture(dst.pToon, toon_path))
		{
			dst.pToon = renderer.getNullGradation();
		}

		if(src.textureFilePath[0] == '\0')
		{
			continue;
		}

		auto asterisk = find(begin(src.textureFilePath), end(src.textureFilePath), '*');
		if(asterisk != end(src.textureFilePath))
		{
			*asterisk = '\0';
			loadTexture(dst, root_path / (asterisk + 1), renderer);
		}

		loadTexture(dst, root_path / src.textureFilePath, renderer);

	}

	return true;
}

bool PMDActor::loadBones(std::ifstream & fin, RendererDX12 & renderer)
{
	uint16_t bone_count = 0;
	fin.read(reinterpret_cast<char *>(&bone_count), sizeof(bone_count));

#pragma pack(push, 1)
	struct PMDBone
	{
		char boneName[20];
		uint16_t parentNo;
		uint16_t nextNo;
		uint8_t type;
		uint16_t ikBoneNo;
		XMFLOAT3 pos;
	};
#pragma pack(pop)

	vector<PMDBone> pmd_bones(bone_count);
	fin.read(reinterpret_cast<char *>(pmd_bones.data()), sizeof(pmd_bones[0]) * pmd_bones.size());

	vector<string> bone_names(bone_count);
	for(size_t i = 0; i < pmd_bones.size(); ++i)
	{
		auto & pmd_bone = pmd_bones[i];
		bone_names[i] = pmd_bone.boneName;

		auto & bone = mNameToBoneMap[pmd_bone.boneName];
		bone.boneIndex = i;
		bone.startPosition = pmd_bone.pos;
	}

	for(auto & pmd_bone : pmd_bones)
	{
		if(pmd_bone.parentNo >= pmd_bones.size())
		{
			continue;
		}

		auto parent_name = bone_names[pmd_bone.parentNo];
		mNameToBoneMap[parent_name].children.emplace_back(&mNameToBoneMap[pmd_bone.boneName]);
	}

	mBoneMatrices.resize(bone_count);
	fill(mBoneMatrices.begin(), mBoneMatrices.end(), XMMatrixIdentity());

	return true;
}

bool PMDActor::createMaterialDescriptorHeap(RendererDX12 & renderer)
{
	D3D12_DESCRIPTOR_HEAP_DESC descriptor_heap_desc;
	descriptor_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descriptor_heap_desc.NumDescriptors = mMaterials.size() * 5;
	descriptor_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descriptor_heap_desc.NodeMask = 0;

	if(!renderer.createDescriptorHeap(mpMaterialDescriptorHeap, descriptor_heap_desc))
	{
		return false;
	}

	return true;
}

bool PMDActor::createMaterialResourceViews(RendererDX12 & renderer)
{
	auto material_buffer_size = (sizeof(MaterialConstantBuffer) + 0xff) & ~0xff;
	if(!renderer.createBuffer(mpMaterialConstantBuffer, material_buffer_size * mMaterials.size()))
	{
		return false;
	}

	uint8_t * p_material_constant = nullptr;
	HRESULT hr = mpMaterialConstantBuffer->Map(
		0,
		nullptr,
		reinterpret_cast<void **>(&p_material_constant)
	);
	if(FAILED(hr))
	{
		return false;
	}

	D3D12_GPU_VIRTUAL_ADDRESS buffer_location = mpMaterialConstantBuffer->GetGPUVirtualAddress();

	auto handle = mpMaterialDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto handle_size = renderer.getDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
	);

	for(auto & m : mMaterials)
	{
		*reinterpret_cast<MaterialConstantBuffer *>(p_material_constant) = m.constantBuffer;
		p_material_constant += material_buffer_size;

		renderer.createConstantBufferView(buffer_location, material_buffer_size, handle);
		buffer_location += material_buffer_size;
		handle.ptr += handle_size;

		renderer.createTextureResourceView(m.pTexture, handle);
		handle.ptr += handle_size;

		renderer.createTextureResourceView(m.pMultipleSphereMap, handle);
		handle.ptr += handle_size;

		renderer.createTextureResourceView(m.pAdditiveSphereMap, handle);
		handle.ptr += handle_size;

		renderer.createTextureResourceView(m.pToon, handle);
		handle.ptr += handle_size;
	}

	mpMaterialConstantBuffer->Unmap(0, nullptr);

	return true;
}

void PMDActor::multiplyMatrixRecursively(
	const BoneNode & bone,
	const DirectX::XMMATRIX & parent_matrix
)
{
	auto & local_matrix = mBoneMatrices[bone.boneIndex];
	local_matrix *= parent_matrix;

	for(const auto & child : bone.children)
	{
		multiplyMatrixRecursively(*child, local_matrix);
	}
}

static float getYFromXOnBezier(float x, const XMFLOAT2 & a, const XMFLOAT2 & b, int32_t n)
{
	if(a.x == a.y && b.x == b.y)
	{
		return x;
	}

	float t = x;
	const float k0 = 1.0f + 3.0f * a.x - 3.0f * b.x;
	const float k1 = 3.0f * b.x - 6.0f * a.x;
	const float k2 = 3.0f * a.x;

	constexpr float epsilon = 0.0005f;

	for(int32_t i = 0; i < n; ++i)
	{
		auto ft = k0 * t + k1;
		ft = ft * t + k2;
		ft = ft * t - x;

		if(-epsilon <= ft && ft <= epsilon)
		{
			break;
		}

		t -= ft * 0.5f;

	}

	auto r = 1.0f - t;
	return t * t * t + 3.0f * t * t * r * b.y + 3.0f * t * r * r * a.y;
}

void PMDActor::updateMotion()
{
	auto elapsed_milliseconds = chrono::duration_cast<chrono::milliseconds>(
		chrono::high_resolution_clock::now() - mStartTime
	).count();
	auto frame_per_sec = 30.0f;
	auto frame = static_cast<uint64_t>(
		floor((elapsed_milliseconds / 1000.0) / (1.0 / frame_per_sec))
	);

	frame %= mMaxFrame;

	fill(mBoneMatrices.begin(), mBoneMatrices.end(), XMMatrixIdentity());

	for(auto & kv : mNameToKeyFrameMap)
	{
		auto it_bone = mNameToBoneMap.find(kv.first);
		if(it_bone == mNameToBoneMap.end())
		{
			continue;
		}

		auto & bone = it_bone->second;
		auto & motions = kv.second;

		auto it = find_if(
			motions.rbegin(),
			motions.rend(),
			[frame](const KeyFrame & key_frame)
			{
				return key_frame.frameNo <= frame;
			}
		);

		if(it == motions.rend())
		{
			continue ;
		}
		auto & bone_position = bone.startPosition;

		XMVECTOR rotation = it->quaternion;
		auto next = it.base();
		if(next != motions.end())
		{
			auto t = 
				static_cast<float>(frame - it->frameNo) /
				static_cast<float>(next->frameNo - it->frameNo);

			t = getYFromXOnBezier(t, next->p1, next->p2, 12);

			rotation = XMQuaternionSlerp(it->quaternion, next->quaternion, t);
		}

		mBoneMatrices[bone.boneIndex] =
			XMMatrixTranslation(-bone_position.x, -bone_position.y, -bone_position.z) *
			XMMatrixRotationQuaternion(rotation) *
			XMMatrixTranslation(bone_position.x, bone_position.y, bone_position.z);
	}

	multiplyMatrixRecursively(mNameToBoneMap["センター"], XMMatrixIdentity());

	copy(mBoneMatrices.begin(), mBoneMatrices.end(), mpMappedTransform + 1);
}
