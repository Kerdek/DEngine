#pragma once

namespace DEngine::Math
{
	constexpr Vector<2, f32> Vector<3, f32>::AsVec2() const
	{
		return Vector<2, f32>{ x, y };
	}

	constexpr Vector<4, f32> Vector<3, f32>::AsVec4(f32 const& wValue) const
	{
		return Vector<4, f32>{ x, y, z, wValue };
	}

	constexpr f32 Vector<3, f32>::MagnitudeSqrd() const
	{
		return (x * x) + (y * y) + (z * z);
	}

	constexpr f32* Vector<3, f32>::Data()
	{
		return &x;
	}

	constexpr f32 const* Vector<3, f32>::Data() const
	{
		return &x;
	}

	constexpr Vector<3, f32> Vector<3, f32>::Cross(Vector<3, f32> const& lhs, Vector<3, f32> const& rhs)
	{
		return { lhs.y * rhs.z - lhs.z * rhs.y, lhs.z * rhs.x - lhs.x * rhs.z, lhs.x * rhs.y - lhs.y * rhs.x };
	}

	constexpr f32 Vector<3, f32>::Dot(Vector<3, f32> const& lhs, Vector<3, f32> const& rhs)
	{
		return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
	}

	constexpr Vector<3, f32> Vector<3, f32>::SingleValue(f32 const& input)
	{
		return { input, input, input };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Zero()
	{
		return Vector<3, f32>{ 0.f, 0.f, 0.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::One()
	{
		return { 1.f, 1.f, 1.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Up()
	{
		return { 0.f, 1.f, 0.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Down()
	{
		return { 0.f, -1.f, 0 };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Left()
	{
		return { -1.f, 1.f, 0.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Right()
	{
		return { 1.f, 0.f, 0.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Forward()
	{
		return { 0.f, 0.f, 1.f };
	}
	constexpr Vector<3, f32> Vector<3, f32>::Back()
	{
		return { 0.f, 0.f, -1.f };
	}

	constexpr f32& Vector<3, f32>::operator[](uSize index) noexcept
	{
		switch (index)
		{
		case 0:
			return x;
		case 1:
			return y;
		case 2:
			return z;
		default:
			DENGINE_DETAIL_UNREACHABLE();
		}
	}

	constexpr f32 Vector<3, f32>::operator[](uSize index) const noexcept
	{
		switch (index)
		{
		case 0:
			return x;
		case 1:
			return y;
		case 2:
			return z;
		default:
			DENGINE_DETAIL_UNREACHABLE();
		}
	}

	constexpr Vector<3, f32>& Vector<3, f32>::operator+=(Vector<3, f32> const& rhs)
	{
		x += rhs.x;
		y += rhs.y;
		z += rhs.z;
		return *this;
	}
	constexpr Vector<3, f32>& Vector<3, f32>::operator-=(Vector<3, f32> const& rhs)
	{
		x -= rhs.x;
		y -= rhs.y;
		z -= rhs.z;
		return *this;
	}
	constexpr Vector<3, f32>& Vector<3, f32>::operator*=(f32 const& rhs)
	{
		x *= rhs;
		y *= rhs;
		z *= rhs;
		return *this;
	}
	constexpr Vector<3, f32> Vector<3, f32>::operator+(Vector<3, f32> const& rhs) const
	{
		return Vector<3, f32>{ x + rhs.x, y + rhs.y, z + rhs.z };
	}
	constexpr Vector<3, f32> Vector<3, f32>::operator-(Vector<3, f32> const& rhs) const
	{
		return Vector<3, f32>{ x - rhs.x, y - rhs.y, z - rhs.z };
	}
	constexpr Vector<3, f32> Vector<3, f32>::operator-() const
	{
		return Vector<3, f32>{ -x, -y, -z };
	}
	constexpr bool Vector<3, f32>::operator==(Vector<3, f32> const& rhs) const
	{
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}
	constexpr bool Vector<3, f32>::operator!=(Vector<3, f32> const& rhs) const
	{
		return x != rhs.x || y != rhs.y || z != rhs.z;
	}

	constexpr Vector<3, f32> operator*(Vector<3, f32> const& lhs, f32 const& rhs)
	{
		return Vector<3, f32>{ lhs.x * rhs, lhs.y * rhs, lhs.z * rhs };
	}

	constexpr Vector<3, f32> operator*(f32 const& lhs, Vector<3, f32> const& rhs)
	{
		return Vector<3, f32>{ lhs * rhs.x, lhs * rhs.y, lhs * rhs.z };
	}
}