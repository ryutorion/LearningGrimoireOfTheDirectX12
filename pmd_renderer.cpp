#include "pmd_renderer.h"
#include "renderer_dx12.h"
#include <d3dx12.h>
#include "pmd.h"

using namespace Microsoft::WRL;

bool PMDRenderer::initialize(RendererDX12 & renderer)
{
	if(!createRootSignature(renderer))
	{
		return false;
	}

	if(!createGraphicsPipelineState(renderer))
	{
		return false;
	}

	return true;
}

void PMDRenderer::setup(RendererDX12 & renderer)
{
	renderer.setPipelineState(mpGraphicsPipelineState);
	renderer.setGraphicsRootSignature(mpRootSignature);
}

bool PMDRenderer::createRootSignature(RendererDX12 & renderer)
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

	if(!renderer.createRootSignature(
		mpRootSignature,
		p_root_signature_blob->GetBufferPointer(),
		p_root_signature_blob->GetBufferSize()
	))
	{
		return false;
	}

	return true;
}

bool PMDRenderer::createGraphicsPipelineState(RendererDX12 & renderer)
{
	ComPtr<ID3DBlob> p_vertex_shader_blob;
	if(!RendererDX12::loadShader(L"BasicShader.hlsl", "BasicVS", "vs_5_0", p_vertex_shader_blob))
	{
		return false;
	}

	ComPtr<ID3DBlob> p_pixel_shader_blob;
	if(!RendererDX12::loadShader(L"BasicShader.hlsl", "BasicPS", "ps_5_0", p_pixel_shader_blob))
	{
		return false;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphics_pipeline_state_desc;
	graphics_pipeline_state_desc.pRootSignature = mpRootSignature.Get();

	graphics_pipeline_state_desc.VS = CD3DX12_SHADER_BYTECODE(p_vertex_shader_blob.Get());
	graphics_pipeline_state_desc.PS = CD3DX12_SHADER_BYTECODE(p_pixel_shader_blob.Get());
	graphics_pipeline_state_desc.DS = CD3DX12_SHADER_BYTECODE();
	graphics_pipeline_state_desc.HS = CD3DX12_SHADER_BYTECODE();
	graphics_pipeline_state_desc.GS = CD3DX12_SHADER_BYTECODE();

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

	graphics_pipeline_state_desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	// graphics_pipeline_state_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	graphics_pipeline_state_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	// graphics_pipeline_state_desc.RasterizerState.FrontCounterClockwise = FALSE;
	// graphics_pipeline_state_desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	// graphics_pipeline_state_desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	// graphics_pipeline_state_desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	// graphics_pipeline_state_desc.RasterizerState.DepthClipEnable = TRUE;
	// graphics_pipeline_state_desc.RasterizerState.MultisampleEnable = FALSE;
	// graphics_pipeline_state_desc.RasterizerState.AntialiasedLineEnable = FALSE;
	// graphics_pipeline_state_desc.RasterizerState.ForcedSampleCount = 0;
	// graphics_pipeline_state_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	graphics_pipeline_state_desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	// graphics_pipeline_state_desc.DepthStencilState.DepthEnable = TRUE;
	// graphics_pipeline_state_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// graphics_pipeline_state_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	// graphics_pipeline_state_desc.DepthStencilState.StencilEnable = FALSE;
	// graphics_pipeline_state_desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	// graphics_pipeline_state_desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	// graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	// graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	// graphics_pipeline_state_desc.DepthStencilState.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	using Vertex = pmd::Vertex;

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
	input_element_descs[4].Format = DXGI_FORMAT_R8_UINT;
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

	if(!renderer.createGraphicsPipelineState(
		mpGraphicsPipelineState,
		graphics_pipeline_state_desc
	))
	{
		return false;
	}

	return true;
}
