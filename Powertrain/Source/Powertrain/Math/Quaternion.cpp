#include "Powertrain/Math/Quaternion.hpp"

#include "Powertrain/Math/DirectXMathConversions.hpp"

namespace Powertrain
{
	Quaternion Quaternion::FromAxisAngle(const Vector3& axis, float radians)
	{
		// XMQuaternionRotationAxis asserts on a zero axis; the identity is the sensible answer
		if (axis.LengthSquared() <= Math::k_Epsilon * Math::k_Epsilon)
		{
			return Identity();
		}

		return DXMath::StoreQuaternion(DirectX::XMQuaternionRotationAxis(DXMath::Load(axis), radians));
	}

	Quaternion Quaternion::FromEulerAngles(float pitch, float yaw, float roll)
	{
		return DXMath::StoreQuaternion(DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
	}

	Quaternion Quaternion::Slerp(const Quaternion& a, const Quaternion& b, float t)
	{
		return DXMath::StoreQuaternion(DirectX::XMQuaternionSlerp(DXMath::Load(a), DXMath::Load(b), t));
	}

	Quaternion Quaternion::Inverse() const
	{
		return DXMath::StoreQuaternion(DirectX::XMQuaternionInverse(DXMath::Load(*this)));
	}

	Vector3 Quaternion::Rotate(const Vector3& vector) const
	{
		return DXMath::StoreVector3(DirectX::XMVector3Rotate(DXMath::Load(vector), DXMath::Load(*this)));
	}

	void Quaternion::ToAxisAngle(Vector3& axis, float& radians) const
	{
		DirectX::XMVECTOR l_Axis;
		DirectX::XMQuaternionToAxisAngle(&l_Axis, &radians, DXMath::Load(*this));

		// DirectXMath hands back the raw vector part; the identity has none
		axis = DXMath::StoreVector3(l_Axis).Normalized();
	}

	Quaternion operator*(const Quaternion& a, const Quaternion& b)
	{
		// XMQuaternionMultiply(a, b) is the rotation a followed by b, matching XMMatrixMultiply
		return DXMath::StoreQuaternion(DirectX::XMQuaternionMultiply(DXMath::Load(a), DXMath::Load(b)));
	}
}