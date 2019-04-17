#include "DEngine/Components/MeshRenderer.hpp"

#include <vector>
#include <string>
#include <cassert>

#include "DRenderer/Renderer.hpp"

#include "DMath/LinearTransform3D.hpp"

#include "DEngine/Scene.hpp"
#include "DEngine/SceneObject.hpp"

namespace Engine
{
	namespace Components
	{
		MeshRenderer::MeshRenderer(SceneObject& owningSceneObject) :
			ParentType(owningSceneObject),
			scale{ 1, 1, 1 },
			rotation()
		{
		}

		MeshRenderer::~MeshRenderer()
		{
		}

		size_t MeshRenderer::GetMesh() const { return mesh; }

		void MeshRenderer::SetMesh(size_t newMesh)
		{
			if (GetMesh() == newMesh)
				return;

			mesh = newMesh;
		}

		Math::Matrix<4, 3> MeshRenderer::GetModel_Reduced(Space space) const
		{
			using namespace Math::LinTran3D;
			Math::Matrix<4, 3> localModel = Multiply_Reduced(Scale_Reduced(scale), Rotate_Reduced(rotation));
			AddTranslation(localModel, positionOffset);

			if (space == Space::Local)
				return localModel;
			else
				return Multiply_Reduced(GetSceneObject().GetModel_Reduced(Space::World), localModel);
		}

		Math::Matrix4x4 MeshRenderer::GetModel(Space space) const
		{
			const auto& model = GetModel_Reduced(space);
			return Math::LinTran3D::AsMat4(model);
		}
	}
}




