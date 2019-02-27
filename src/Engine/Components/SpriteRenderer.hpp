#pragma once

#include "RenderComponent.hpp"

#include "../Asset.hpp"

#include "../Renderer/Renderer.hpp"

#include "Math/Vector/Vector.hpp"
#include "Math/Matrix/Matrix.hpp"

#include "../Enum.hpp"

namespace Engine
{
	class SpriteRenderer : public RenderComponent
	{
	public:
		using ParentType = RenderComponent;

		SpriteRenderer(SceneObject& owningObject, size_t indexInSceneObject, size_t indexInScene);
		~SpriteRenderer();

		void SetSprite(Asset::Sprite newTexture);
		[[nodiscard]] Asset::Sprite GetSprite() const;

		Math::Matrix<3, 2> GetModel2D_Reduced(Space space) const;
		Math::Matrix4x4 GetModel(Space space) const;

		explicit operator Renderer::AssetID() const;

		Math::Vector2D position;
		float rotation;
		Math::Vector2D scale;

	private:
		Asset::Sprite sprite;
	};
}