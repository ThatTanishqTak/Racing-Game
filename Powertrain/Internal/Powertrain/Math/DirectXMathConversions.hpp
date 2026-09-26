#pragma once

#include "Powertrain/Math/Matrix4.hpp"
#include "Powertrain/Math/Quaternion.hpp"
#include "Powertrain/Math/Vector3.hpp"
#include "Powertrain/Math/Vector4.hpp"

#include <DirectXMath.h>

#include <cstring>

// DirectXMath is the implementation behind the public math types. The public structs share XMFLOAT3/4 and XMFLOAT4X4's layout, so loads and stores are plain copies.
namespace Powertrain::DXMath
{
	static_assert(sizeof(Matrix4) == sizeof(DirectX::XMFLOAT4X4), "Matrix4 must match XMFLOAT4X4");

	inline DirectX::XMVECTOR Load(const Vector3& v) { return DirectX::XMVectorSet(v.X, v.Y, v.Z, 0.0f); }
	inline DirectX::XMVECTOR Load(const Vector4& v) { return DirectX::XMVectorSet(v.X, v.Y, v.Z, v.W); }
	inline DirectX::XMVECTOR Load(const Quaternion& q) { return DirectX::XMVectorSet(q.X, q.Y, q.Z, q.W); }

	inline DirectX::XMMATRIX Load(const Matrix4& m)
	{
		DirectX::XMFLOAT4X4 l_Stored;
		std::memcpy(&l_Stored, m.M, sizeof(l_Stored));

		return DirectX::XMLoadFloat4x4(&l_Stored);
	}

	inline Vector3 StoreVector3(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT3 l_Stored;
		DirectX::XMStoreFloat3(&l_Stored, v);

		return { l_Stored.x, l_Stored.y, l_Stored.z };
	}

	inline Vector4 StoreVector4(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT4 l_Stored;
		DirectX::XMStoreFloat4(&l_Stored, v);

		return { l_Stored.x, l_Stored.y, l_Stored.z, l_Stored.w };
	}

	inline Quaternion StoreQuaternion(DirectX::FXMVECTOR v)
	{
		DirectX::XMFLOAT4 l_Stored;
		DirectX::XMStoreFloat4(&l_Stored, v);

		return { l_Stored.x, l_Stored.y, l_Stored.z, l_Stored.w };
	}

	inline Matrix4 StoreMatrix4(DirectX::CXMMATRIX m)
	{
		DirectX::XMFLOAT4X4 l_Stored;
		DirectX::XMStoreFloat4x4(&l_Stored, m);

		Matrix4 l_Result;
		std::memcpy(l_Result.M, &l_Stored, sizeof(l_Result.M));

		return l_Result;
	}
}