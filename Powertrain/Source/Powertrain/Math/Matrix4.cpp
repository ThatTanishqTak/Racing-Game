#include "Powertrain/Math/Matrix4.hpp"

#include "Powertrain/Core/CoreLog.hpp"
#include "Powertrain/Math/DirectXMathConversions.hpp"

#include <cmath>

namespace Powertrain
{
	Matrix4 Matrix4::RotationX(float radians)
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixRotationX(radians));
	}

	Matrix4 Matrix4::RotationY(float radians)
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixRotationY(radians));
	}

	Matrix4 Matrix4::RotationZ(float radians)
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixRotationZ(radians));
	}

	Matrix4 Matrix4::FromQuaternion(const Quaternion& rotation)
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixRotationQuaternion(DXMath::Load(rotation)));
	}

	Matrix4 Matrix4::TRS(const Vector3& translation, const Quaternion& rotation, const Vector3& scale)
	{
		// Rotation about the origin, so the rotation origin argument is zero
		return DXMath::StoreMatrix4(DirectX::XMMatrixAffineTransformation(DXMath::Load(scale), DirectX::XMVectorZero(), DXMath::Load(rotation), DXMath::Load(translation)));
	}

	Matrix4 Matrix4::LookAt(const Vector3& eye, const Vector3& target, const Vector3& up)
	{
		if ((target - eye).LengthSquared() <= Math::k_Epsilon * Math::k_Epsilon)
		{
			PT_CORE_ASSERT(false, "Matrix4::LookAt needs distinct eye and target");

			return Identity();
		}

		return DXMath::StoreMatrix4(DirectX::XMMatrixLookAtRH(DXMath::Load(eye), DXMath::Load(target), DXMath::Load(up)));
	}

	Matrix4 Matrix4::Perspective(float verticalFovRadians, float aspectRatio, float nearPlane)
	{
		PT_CORE_ASSERT(verticalFovRadians > 0.0f && verticalFovRadians < Math::k_Pi, "Field of view {} out of range", verticalFovRadians);
		PT_CORE_ASSERT(aspectRatio > 0.0f, "Aspect ratio must be positive");
		PT_CORE_ASSERT(nearPlane > 0.0f, "Near plane must be positive");

		const float l_YScale = 1.0f / std::tan(verticalFovRadians * 0.5f);
		const float l_XScale = l_YScale / aspectRatio;

		Matrix4 l_Result;
		l_Result.M[0][0] = l_XScale;
		l_Result.M[1][1] = l_YScale;
		l_Result.M[2][2] = 0.0f;
		l_Result.M[2][3] = -1.0f;
		l_Result.M[3][2] = nearPlane;
		l_Result.M[3][3] = 0.0f;

		return l_Result;
	}

	Matrix4 Matrix4::Orthographic(float width, float height, float nearPlane, float farPlane)
	{
		PT_CORE_ASSERT(width > 0.0f && height > 0.0f, "Orthographic size {}x{} must be positive", width, height);
		PT_CORE_ASSERT(farPlane > nearPlane, "Orthographic far plane {} must be beyond the near plane {}", farPlane, nearPlane);

		// View space looks down -Z, so z_view = -nearPlane maps to depth 1 and z_view = -farPlane to depth 0
		const float l_DepthScale = 1.0f / (farPlane - nearPlane);

		Matrix4 l_Result;
		l_Result.M[0][0] = 2.0f / width;
		l_Result.M[1][1] = 2.0f / height;
		l_Result.M[2][2] = l_DepthScale;
		l_Result.M[3][2] = farPlane * l_DepthScale;

		return l_Result;
	}

	Matrix4 Matrix4::Transposed() const
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixTranspose(DXMath::Load(*this)));
	}

	Matrix4 Matrix4::Inverse() const
	{
		DirectX::XMVECTOR l_Determinant;
		const DirectX::XMMATRIX l_Inverse = DirectX::XMMatrixInverse(&l_Determinant, DXMath::Load(*this));

		// A singular matrix comes back as infinities; the identity is at least finite
		if (DirectX::XMVector4Equal(l_Determinant, DirectX::XMVectorZero()) || DirectX::XMMatrixIsInfinite(l_Inverse) || DirectX::XMMatrixIsNaN(l_Inverse))
		{
			return Identity();
		}

		return DXMath::StoreMatrix4(l_Inverse);
	}

	float Matrix4::Determinant() const
	{
		return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(DXMath::Load(*this)));
	}

	bool Matrix4::Decompose(Vector3& scale, Quaternion& rotation, Vector3& translation) const
	{
		DirectX::XMVECTOR l_Scale;
		DirectX::XMVECTOR l_Rotation;
		DirectX::XMVECTOR l_Translation;
		if (!DirectX::XMMatrixDecompose(&l_Scale, &l_Rotation, &l_Translation, DXMath::Load(*this)))
		{
			return false;
		}

		scale = DXMath::StoreVector3(l_Scale);
		rotation = DXMath::StoreQuaternion(l_Rotation);
		translation = DXMath::StoreVector3(l_Translation);

		return true;
	}

	Matrix4& Matrix4::operator*=(const Matrix4& other)
	{
		*this = *this * other;

		return *this;
	}

	Matrix4 operator*(const Matrix4& a, const Matrix4& b)
	{
		return DXMath::StoreMatrix4(DirectX::XMMatrixMultiply(DXMath::Load(a), DXMath::Load(b)));
	}
}