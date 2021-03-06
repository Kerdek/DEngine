#pragma once

#include <DEngine/FixedWidthTypes.hpp>

#include <DEngine/Gui/Utility.hpp>

namespace DEngine::Gui
{
	struct SizeHint
	{
		Extent preferred{};
		bool expandX = false;
		bool expandY = false;
	};
}