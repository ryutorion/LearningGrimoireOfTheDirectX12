#ifndef PMD_H_INCLUDED
#define PMD_H_INCLUDED

#include <DirectXMath.h>

namespace pmd
{
	struct Vertex
	{
		DirectX::XMVECTOR position;
		DirectX::XMVECTOR normal;
		DirectX::XMFLOAT2 uv;
		uint16_t bones[2];
		uint8_t weight;
		uint8_t edge;
	};
}

#endif // PMD_H_INCLUDED
