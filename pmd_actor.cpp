#include "pmd_actor.h"
#include "renderer_dx12.h"
#include "pmd.h"
#include <algorithm>
#include <cassert>
#include <sstream>
#include <d3dx12.h>

using namespace std;
using namespace DirectX;
using namespace Microsoft::WRL;

static constexpr float epsilon = 0.0005f;

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
			vmd_key_frame.location,
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

#if 0
	uint32_t morph_count = 0;
	fin.read(reinterpret_cast<char *>(&morph_count), sizeof(morph_count));

#pragma pack(push, 1)
	struct VMDMorph
	{
		char name[15];
		uint32_t frameNo;
		float weight;
	};
#pragma pack(pop)

	vector<VMDMorph> morphs(morph_count);
	fin.read(reinterpret_cast<char *>(morphs.data()), sizeof(morphs[0]) * morphs.size());

	uint32_t camera_count = 0;
	fin.read(reinterpret_cast<char *>(&camera_count), sizeof(camera_count));

#pragma pack(push, 1)
	struct VMDCamera
	{
		uint32_t frameNo;
		float distance;
		XMFLOAT3 position;
		XMFLOAT3 eulerAngle;
		uint8_t interpolation[24];
		uint32_t fov;
		uint8_t perspective;
	};
#pragma pack(pop)

	vector<VMDCamera> cameras(camera_count);
	fin.read(reinterpret_cast<char *>(cameras.data()), sizeof(cameras[0]) * cameras.size());

	uint32_t light_count = 0;
	struct VMDLight
	{
		uint32_t frameNo;
		XMFLOAT3 rgb;
		XMFLOAT3 position;
	};

	vector<VMDLight> lights(light_count);
	fin.read(reinterpret_cast<char *>(lights.data()), sizeof(lights[0]) * lights.size());

	uint32_t self_shadow_count = 0;
	fin.read(reinterpret_cast<char *>(&self_shadow_count), sizeof(self_shadow_count));

#pragma pack(push, 1)
	struct VMDSelfShadow
	{
		uint32_t frameNo;
		uint8_t mode;
		float distance;
	};
#pragma pack(pop)
	
	vector<VMDSelfShadow> self_shadows(self_shadow_count);
	fin.read(reinterpret_cast<char *>(self_shadows.data()), sizeof(self_shadows[0]) * self_shadows.size());

	uint32_t ik_switch_count = 0;
	fin.read(reinterpret_cast<char *>(&ik_switch_count), sizeof(ik_switch_count));

	mIKEnables.resize(ik_switch_count);
	for(auto & ik_enable : mIKEnables)
	{
		ik_enable.enables.resize(mBones.size());

		fin.read(reinterpret_cast<char *>(&ik_enable.frameNo), sizeof(ik_enable.frameNo));

		// visibleフラグをスキップ
		fin.seekg(1, ios::cur);

		uint32_t ik_bone_count = 0;
		fin.read(reinterpret_cast<char *>(&ik_bone_count), sizeof(ik_bone_count));

		for(uint32_t i = 0; i < ik_bone_count; ++i)
		{
			char ik_bone_name[20];
			fin.read(ik_bone_name, sizeof(ik_bone_name));

			uint8_t enable = 0;
			fin.read(reinterpret_cast<char *>(&enable), sizeof(enable));

			auto it = mNameToBoneIndex.find(ik_bone_name);
			if(it != mNameToBoneIndex.end())
			{
				ik_enable.enables[it->second] = enable;
			}
		}
	}
#endif

	return true;
}

void PMDActor::update()
{
	*mpMappedTransform = XMMatrixTranspose(
		XMMatrixRotationRollPitchYaw(mEulerAngle.x, mEulerAngle.y, mEulerAngle.z) *
		XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z)
	);
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

void PMDActor::setPosition(float x, float y, float z)
{
	mPosition = XMFLOAT3(x, y, z);
}

void PMDActor::setEulerAngle(float x, float y, float z)
{
	mEulerAngle = XMFLOAT3(x, y, z);
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
		auto boneIndex = mNameToBoneIndex[kv.first];
		auto & bone = mBones[boneIndex];
		auto & bone_position = bone.startPosition;
		mBoneMatrices[boneIndex] =
			XMMatrixTranslation(-bone_position.x, -bone_position.y, -bone_position.z) *
			XMMatrixRotationQuaternion(kv.second[0].quaternion) *
			XMMatrixTranslation(bone_position.x, bone_position.y, bone_position.z);
	}

	multiplyMatrixRecursively(mNameToBoneIndex["センター"], XMMatrixIdentity());

	*mpMappedTransform = XMMatrixTranspose(
		XMMatrixRotationRollPitchYaw(mEulerAngle.x, mEulerAngle.y, mEulerAngle.z) *
		XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z)
	);
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

	if(!loadIK(fin))
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

	mBones.resize(bone_count);
	for(uint32_t i = 0; i < pmd_bones.size(); ++i)
	{
		auto & pmd_bone = pmd_bones[i];
		auto & bone = mBones[i];

		bone.boneType = static_cast<BoneType>(pmd_bone.type);
		bone.boneName = pmd_bone.boneName;
		bone.startPosition = pmd_bone.pos;

		mNameToBoneIndex[bone.boneName] = i;
	}

	for(auto & pmd_bone : pmd_bones)
	{
		if(pmd_bone.parentNo >= pmd_bones.size())
		{
			continue;
		}

		auto boneIndex = mNameToBoneIndex[pmd_bone.boneName];
		mBones[pmd_bone.parentNo].children.emplace_back(boneIndex);
	}

	mBoneMatrices.resize(bone_count);
	fill(mBoneMatrices.begin(), mBoneMatrices.end(), XMMatrixIdentity());

	return true;
}

bool PMDActor::loadIK(std::ifstream & fin)
{
	uint16_t ik_count = 0;
	fin.read(reinterpret_cast<char *>(&ik_count), sizeof(ik_count));

	mIKs.resize(ik_count);
	for(auto & ik : mIKs)
	{
		fin.read(reinterpret_cast<char *>(&ik.boneIndex), sizeof(ik.boneIndex));
		fin.read(reinterpret_cast<char *>(&ik.targetIndex), sizeof(ik.targetIndex));

		uint8_t chain_length = 0;
		fin.read(reinterpret_cast<char *>(&chain_length), sizeof(chain_length));
		fin.read(reinterpret_cast<char *>(&ik.iterations), sizeof(ik.iterations));
		fin.read(reinterpret_cast<char *>(&ik.limit), sizeof(ik.limit));

		if(chain_length == 0)
		{
			continue;
		}

		ik.nodeIndices.resize(chain_length);
		fin.read(
			reinterpret_cast<char *>(ik.nodeIndices.data()),
			sizeof(ik.nodeIndices[0]) * ik.nodeIndices.size()
		);

#if defined(_DEBUG)
		ostringstream oss;
		oss << "IKボーン番号:" << ik.boneIndex << "(" << mBones[ik.boneIndex].boneName << ")" << endl;
		for(auto & node : ik.nodeIndices)
		{
			oss << "\tノードボーン:" << node << "(" << mBones[node].boneName << ")" << endl;
		}

		OutputDebugStringA(oss.str().c_str());
#endif
	}

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
	const uint32_t boneIndex,
	const DirectX::XMMATRIX & parent_matrix
)
{
	auto & local_matrix = mBoneMatrices[boneIndex];
	local_matrix *= parent_matrix;

	for(const auto child : mBones[boneIndex].children)
	{
		multiplyMatrixRecursively(child, local_matrix);
	}
}

static XMMATRIX lookAt(const XMVECTOR & dir, const XMVECTOR & up, const XMVECTOR & right)
{
	XMVECTOR z = dir;
	XMVECTOR y = XMVector3Normalize(up);
	XMVECTOR x = XMVector3Normalize(XMVector3Cross(y, z));
	y = XMVector3Cross(x, z);

	if(abs(XMVectorGetX(XMVector3Dot(y, z))) == 1.0f)
	{
		x = XMVector3Normalize(right);
		y = XMVector3Normalize(XMVector3Cross(z, x));
		x = XMVector3Cross(y, z);
	}

	XMMATRIX result = XMMatrixIdentity();
	result.r[0] = x;
	result.r[1] = y;
	result.r[2] = z;

	return result;
}

static XMMATRIX lookAt(const XMVECTOR & eye, const XMVECTOR & at, const XMVECTOR & up, const XMVECTOR & right)
{
	return XMMatrixTranspose(lookAt(eye, up, right)) * lookAt(at, up, right);
}

void PMDActor::solveLookAt(const IK & ik)
{
	auto & root_bone = mBones[ik.nodeIndices[0]];
	auto & target_bone = mBones[ik.targetIndex];

	auto opos1 = XMLoadFloat3(&root_bone.startPosition);
	auto tpos1 = XMLoadFloat3(&target_bone.startPosition);

	auto opos2 = XMVector3Transform(opos1, mBoneMatrices[ik.nodeIndices[0]]);
	auto tpos2 = XMVector3Transform(tpos1, mBoneMatrices[ik.boneIndex]);

	auto originVec = XMVector3Normalize(tpos1 - opos1);
	auto targetVec = XMVector3Normalize(tpos2 - opos2);

	mBoneMatrices[ik.nodeIndices[0]] =
		XMMatrixTranslationFromVector(-opos2) *
		lookAt(originVec, targetVec, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)) *
		XMMatrixTranslationFromVector(opos2);
}

void PMDActor::solveCosineIK(const IK & ik)
{
	auto & ik_bone = mBones[ik.boneIndex];
	auto ik_position = XMVector3Transform(XMLoadFloat3(&ik_bone.startPosition), mBoneMatrices[ik.boneIndex]);

	auto target_bone = mBones[ik.targetIndex];
	XMVECTOR positions[]
	{
		XMLoadFloat3(&mBones[ik.nodeIndices[1]].startPosition),
		XMLoadFloat3(&mBones[ik.nodeIndices[0]].startPosition),
		XMLoadFloat3(&target_bone.startPosition),
	};

	float edge_lengths[]
	{
		XMVectorGetX(XMVector3Length(positions[1] - positions[0])),
		XMVectorGetX(XMVector3Length(positions[2] - positions[1])),
	};

	positions[0] = XMVector3Transform(positions[0], mBoneMatrices[ik.nodeIndices[1]]);
	positions[2] = XMVector3Transform(positions[2], mBoneMatrices[ik.boneIndex]);

	auto linearVec = positions[2] - positions[0];
	float A = XMVectorGetX(XMVector3Length(linearVec));
	float B = edge_lengths[0];
	float C = edge_lengths[1];

	linearVec = XMVector3Normalize(linearVec);

	float theta1 = acos((A * A + B * B - C * C) / (2.0f * A * B));
	float theta2 = acos((B * B + C * C - A * A) / (2.0f * B * C));

	XMVECTOR axis;
	if(mBones[ik.nodeIndices[0]].boneName.find("ひざ") != string::npos)
	{
		axis = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	}
	else
	{
		auto vm = XMVector3Normalize(positions[2] - positions[0]);
		auto vt = XMVector3Normalize(ik_position - positions[0]);
		axis = XMVector3Cross(vt, vm);
	}

	auto r0 =
		XMMatrixTranslationFromVector(-positions[0]) *
		XMMatrixRotationAxis(axis, theta1) *
		XMMatrixTranslationFromVector(positions[0]);

	auto r1 =
		XMMatrixTranslationFromVector(-positions[1]) *
		XMMatrixRotationAxis(axis, theta2 - XM_PI) *
		XMMatrixTranslationFromVector(positions[1]);

	mBoneMatrices[ik.nodeIndices[1]] *= r0;
	mBoneMatrices[ik.nodeIndices[0]] = r1 * mBoneMatrices[ik.nodeIndices[1]];
	mBoneMatrices[ik.targetIndex] = mBoneMatrices[ik.nodeIndices[0]];
}

void PMDActor::solveCCDIK(const IK & ik)
{
	auto & ik_bone = mBones[ik.boneIndex];
	auto ik_position = XMLoadFloat3(&ik_bone.startPosition);

	auto & parent_matrix = mBoneMatrices[ik_bone.ikParentBone];
	XMVECTOR determinant;
	auto inverse_parent_matrix = XMMatrixInverse(&determinant, parent_matrix);
	auto target_next_position = XMVector3Transform(ik_position, mBoneMatrices[ik.boneIndex] * inverse_parent_matrix);

	vector<XMVECTOR> bone_positions;
	auto target_position = XMLoadFloat3(&mBones[ik.targetIndex].startPosition);
	for(auto node_index : ik.nodeIndices)
	{
		bone_positions.push_back(XMLoadFloat3(&mBones[node_index].startPosition));
	}

	vector<XMMATRIX> matrices(bone_positions.size(), XMMatrixIdentity());

	auto ik_limit = ik.limit * XM_PI;
	for(auto c = 0; c < ik.iterations; ++c)
	{
		if(XMVectorGetX(XMVector3Length(target_position - target_next_position)) <= epsilon)
		{
			break;
		}

		for(int bone_index = 0; bone_index < bone_positions.size(); ++bone_index)
		{
			const auto & position = bone_positions[bone_index];
			auto vec_to_target = XMVector3Normalize(target_position - position);
			auto vec_to_next_target = XMVector3Normalize(target_next_position - position);

			if(XMVectorGetX(XMVector3Length(vec_to_target - vec_to_next_target)) <= epsilon)
			{
				continue;
			}

			auto cross = XMVector3Normalize(XMVector3Cross(vec_to_target, vec_to_next_target));
			float angle = min(XMVectorGetX(XMVector3AngleBetweenVectors(vec_to_target, vec_to_next_target)), ik_limit);
			XMMATRIX m =
				XMMatrixTranslationFromVector(-position) *
				XMMatrixRotationAxis(cross, angle) *
				XMMatrixTranslationFromVector(position);

			matrices[bone_index] *= m;

			for(auto index = bone_index - 1; index >= 0; --index)
			{
				bone_positions[index] = XMVector3Transform(bone_positions[index], m);
			}

			target_position = XMVector3Transform(target_position, m);
			if(XMVectorGetX(XMVector3Length(target_position - target_next_position)) <= epsilon)
			{
				break;
			}
		}
	}

	for(auto i = 0; i < matrices.size(); ++i)
	{
		mBoneMatrices[ik.nodeIndices[i]] = matrices[i];
	}

	multiplyMatrixRecursively(ik.nodeIndices.back(), parent_matrix);
}

void PMDActor::solveIK()
{
	for(const auto & ik : mIKs)
	{
		switch(ik.nodeIndices.size())
		{
		case 0:
			continue;
		case 1:
			solveLookAt(ik);
			break;
		case 2:
			solveCosineIK(ik);
			break;
		default:
			solveCCDIK(ik);
			break;
		}
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

	// frame = frame_value;

	fill(mBoneMatrices.begin(), mBoneMatrices.end(), XMMatrixIdentity());

	for(auto & kv : mNameToKeyFrameMap)
	{
		auto it_bone = mNameToBoneIndex.find(kv.first);
		if(it_bone == mNameToBoneIndex.end())
		{
			continue;
		}

		auto boneIndex = it_bone->second;
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
		auto & bone_position = mBones[boneIndex].startPosition;

		XMVECTOR rotation = it->quaternion;
		XMVECTOR translation = XMLoadFloat3(&it->offset);

		auto next = it.base();
		if(next != motions.end())
		{
			auto t = 
				static_cast<float>(frame - it->frameNo) /
				static_cast<float>(next->frameNo - it->frameNo);

			t = getYFromXOnBezier(t, next->p1, next->p2, 12);

			rotation = XMQuaternionSlerp(it->quaternion, next->quaternion, t);
			translation = XMVectorLerp(translation, XMLoadFloat3(&next->offset), t);
		}

		mBoneMatrices[boneIndex] =
			XMMatrixTranslation(-bone_position.x, -bone_position.y, -bone_position.z) *
			XMMatrixRotationQuaternion(rotation) *
			XMMatrixTranslation(bone_position.x, bone_position.y, bone_position.z) *
			XMMatrixTranslationFromVector(translation);
	}

	multiplyMatrixRecursively(mNameToBoneIndex["センター"], XMMatrixIdentity());

	// solveIK();

	copy(mBoneMatrices.begin(), mBoneMatrices.end(), mpMappedTransform + 1);
}
