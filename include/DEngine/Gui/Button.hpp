#pragma once

#include <DEngine/Gui/Widget.hpp>
#include <DEngine/Gui/Text.hpp>

#include <functional>

namespace DEngine::Gui
{
	class Button : public Widget
	{
	public:
		using ParentType = Widget;

		Button();
		virtual ~Button() {}

		enum class Type
		{
			Push,
			Toggle
		};
		Type type = Type::Push;
		
		enum class State
		{
			Normal,
			Hovered,
			Pressed,
			Toggled,
		};
		State state = State::Normal;
		Math::Vec4 normalColor{ 0.25f, 0.25f, 0.25f, 1.f };
		Math::Vec4 normalTextColor = Math::Vec4::One();
		Math::Vec4 toggledColor{ 0.4f, 0.4f, 0.4f, 1.f };
		Math::Vec4 toggledTextColor = Math::Vec4::One();
		Math::Vec4 hoverColor{ 0.6f, 0.6f, 0.6f, 1.f };
		Math::Vec4 hoverTextColor = Math::Vec4::One();
		Math::Vec4 pressedColor{ 1.f, 1.f, 1.f, 1.f };
		Math::Vec4 pressedTextColor = Math::Vec4::Zero();
		Text textWidget{};

		void* pUserData{};
		std::function<void(Button& btn)> activatePfn = nullptr;

		void SetToggled(bool toggled);
		bool GetToggled() const;

		virtual void CursorMove(
			Test& test,
			Rect widgetRect,
			Rect visibleRect,
			CursorMoveEvent event) override;

		virtual void CursorClick(
			Context& ctx,
			Rect widgetRect,
			Rect visibleRect,
			Math::Vec2Int cursorPos,
			CursorClickEvent event) override;

		virtual void TouchEvent(
			Context& ctx,
			Rect widgetRect,
			Rect visibleRect,
			Gui::TouchEvent event) override;

		[[nodiscard]] virtual Gui::SizeHint SizeHint(
			Context const& ctx) const override;

		[[nodiscard]] virtual Gui::SizeHint SizeHint_Tick(
			Context const& ctx) override;

		virtual void Render(
			Context const& ctx,
			Extent framebufferExtent,
			Rect widgetRect,
			Rect visibleRect,
			DrawInfo& drawInfo) const override;

	private:
		bool toggled = false;
		
		void SetState(State newState);
		void Activate();
	};
}