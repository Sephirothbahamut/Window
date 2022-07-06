#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <Windows.h>
#pragma comment (lib, "dwmapi.lib")//without this dwmapi.h doesn't work :shrugs: no idea whatsoever where the compiler is taking this file from
//#include <WinUser.h>
#include <dwmapi.h>
//#include <Shlwapi.h>
//#include <tchar.h>

#include <iostream>

#include <Windows.h>
#include <windowsx.h>
#pragma comment (lib, "dwmapi.lib")//without this dwmapi.h doesn't work :shrugs: no idea whatsoever where the compiler is taking this file from
//#include <WinUser.h>
#include <dwmapi.h>
//#include <Shlwapi.h>
//#include <tchar.h>

#include <iostream>
/*
class Window;
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;
LRESULT hit_test(const Window& window, POINT cursor);
bool borderless = true; // is the window currently borderless
bool borderless_resize = true; // should the window allow resizing by dragging the borders while borderless
bool borderless_drag = true; // should the window allow moving my dragging the client area
bool borderless_shadow = true; // should the window display a native aero shadow while borderless

namespace
	{
	// we cannot just use WS_POPUP style
	// WS_THICKFRAME: without this the window cannot be resized and so aero snap, de-maximizing and minimizing won't work
	// WS_SYSMENU: enables the context menu with the move, close, maximize, minize... commands (shift + right-click on the task bar item)
	// WS_CAPTION: enables aero minimize animation/transition
	// WS_MAXIMIZEBOX, WS_MINIMIZEBOX: enable minimize/maximize
	enum class Style : DWORD
		{
		windowed = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		aero_borderless = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
		basic_borderless = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
		};

	auto maximized(HWND hwnd) -> bool
		{
		WINDOWPLACEMENT placement;
		if (!::GetWindowPlacement(hwnd, &placement))
			{
			return false;
			}

		return placement.showCmd == SW_MAXIMIZE;
		}

	//Adjust client rect to not spill over monitor edges when maximized.
	//rect(in/out): in: proposed window rect, out: calculated client rect
	//Does nothing if the window is not maximized.
	auto adjust_maximized_client_rect(HWND window, RECT& rect) -> void
		{
		if (!maximized(window))
			{
			return;
			}

		auto monitor = ::MonitorFromWindow(window, MONITOR_DEFAULTTONULL);
		if (!monitor)
			{
			return;
			}

		MONITORINFO monitor_info{};
		monitor_info.cbSize = sizeof(monitor_info);
		if (!::GetMonitorInfoW(monitor, &monitor_info))
			{
			return;
			}

		// when maximized, make the client area fill just the monitor (without task bar) rect,
		// not the whole window rect which extends beyond the monitor.
		rect = monitor_info.rcWork;
		}

	auto last_error(const std::string& message) -> std::system_error
		{
		return std::system_error(
			std::error_code(::GetLastError(), std::system_category()),
			message
		);
		}

	auto window_class(WNDPROC wndproc) -> const wchar_t*
		{
		static const wchar_t* window_class_name = [&]
			{
			WNDCLASSEXW wcx{};
			wcx.cbSize = sizeof(wcx);
			wcx.style = CS_HREDRAW | CS_VREDRAW;
			wcx.hInstance = nullptr;
			wcx.lpfnWndProc = wndproc;
			wcx.lpszClassName = L"BorderlessWindowClass";
			wcx.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
			wcx.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
			const ATOM result = ::RegisterClassExW(&wcx);
			if (!result)
				{
				throw last_error("failed to register window class");
				}
			return wcx.lpszClassName;
			}();
			return window_class_name;
		}

	auto composition_enabled() -> bool
		{
		BOOL composition_enabled = FALSE;
		bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
		return composition_enabled && success;
		}

	auto select_borderless_style() -> Style
		{
		return composition_enabled() ? Style::aero_borderless : Style::basic_borderless;
		}

	auto set_shadow(HWND handle, bool enabled) -> void
		{
		if (composition_enabled())
			{
			static const MARGINS shadow_state[2]{{ 0,0,0,0 },{ 1,1,1,1 }};
			::DwmExtendFrameIntoClientArea(handle, &shadow_state[enabled]);
			}
		}
	}

class Window
	{
	friend LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept;
	friend LRESULT hit_test(const Window& window, POINT cursor);
	private:
		inline static Window* global_window_ptr;
		LONG_PTR sfml_window_proc;

		inline const static int resize_margin = 16;
		enum class Resize { rr, up, ll, dw, ur, ul, dr, dl, NONE };

		sf::RenderWindow inner;
		HWND handle;
		sf::Color bg_color;
		sf::Sprite bg_image;
		bool has_bg_image;

		Resize resize = Resize::NONE;
		sf::Vector2i resize_mouse_delta;

		void _button_released(const sf::Event::MouseButtonEvent& event) { resize = Resize::NONE; }
		bool _button_pressed(const sf::Event::MouseButtonEvent& event)
			{
			if (event.x < resize_margin)
				{
				resize_mouse_delta.x = event.x;
				if (event.y < resize_margin) { resize = Resize::ul; resize_mouse_delta.y = event.y; }
				else if (event.y > height - resize_margin) { resize = Resize::dl; resize_mouse_delta.y = height - event.y; }
				else { resize = Resize::ll; resize_mouse_delta.y = 0; }
				return true;
				}
			else if (event.x > width - resize_margin)
				{
				resize_mouse_delta.x = width - event.x;
				if (event.y < resize_margin) { resize = Resize::ur; resize_mouse_delta.y = event.y; }
				else if (event.y > height - resize_margin) { resize = Resize::dr; resize_mouse_delta.y = height - event.y; }
				else { resize = Resize::rr; resize_mouse_delta.y = 0; }
				return true;
				}
			else if (event.y < resize_margin) { resize = Resize::up; resize_mouse_delta = {0, event.y}; return true; }
			else if (event.y > height - resize_margin) { resize = Resize::dw; resize_mouse_delta = {0, static_cast<int>(height) - event.y}; return true; }
			return false;
			}
		void _mouse_moved(const sf::Event::MouseMoveEvent& event)
			{
			sf::Vector2u new_size = size;
			sf::Vector2i new_pos = position;
			switch (resize)
				{
				case Resize::rr: new_size.x = event.x + resize_mouse_delta.x; break;
				case Resize::dw: new_size.y = event.y + resize_mouse_delta.y; break;
				case Resize::dr: new_size.x = event.x + resize_mouse_delta.x; new_size.y = event.y + resize_mouse_delta.y; break;

				case Resize::ll: new_pos.x = sf::Mouse::getPosition().x - resize_mouse_delta.x; new_size.x = width - (new_pos.x - x); break;
				case Resize::up: new_pos.y = sf::Mouse::getPosition().y - resize_mouse_delta.y; new_size.y = height - (new_pos.y - y); break;
				case Resize::ul: new_pos.x = sf::Mouse::getPosition().x - resize_mouse_delta.x; new_size.x = width - (new_pos.x - x); new_pos.y = sf::Mouse::getPosition().y - resize_mouse_delta.y; new_size.y = height - (new_pos.y - y); break;
				}

			new_size.x = new_size.x < size_min.x ? size_min.x : new_size.x > size_max.x ? size_max.x : new_size.x;
			new_size.y = new_size.y < size_min.y ? size_min.y : new_size.y > size_max.y ? size_max.y : new_size.y;
			std::cout << "(" << new_size.x << ", " << new_size.y << ")" << std::endl;
			set_size(new_size);
			set_position(new_pos);
			}

		void _resized(const sf::Event::SizeEvent& event)
			{
			sf::FloatRect visibleArea(0, 0, event.width, event.height);
			inner.setView(sf::View(visibleArea));
			if (has_bg_image)
				{
				bg_image.setTextureRect(sf::IntRect(inner.getPosition(), sf::Vector2i(inner.getSize())));
				}

			clear(); display();
			}

		void set_position(const sf::Vector2i& position) { inner.setPosition(position); }
		void set_position(int x, int y) { inner.setPosition({x, y}); }
		void set_size(const sf::Vector2u& size) { inner.setSize(size); }
		void set_size(unsigned width, unsigned height) { inner.setSize({width, height}); }
	public:
		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Constructors & Destructors ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		Window(sf::String title, sf::Vector2i position = sf::Vector2i(0, 0), sf::Vector2u size = sf::Vector2u(800, 600), sf::Color color = sf::Color::Black, const sf::Texture* const texture = nullptr)
			: inner(sf::VideoMode::getDesktopMode(), title, sf::Style::None, sf::ContextSettings(0, 0, 8, 1, 0, 0, false)), bg_color(color), has_bg_image(texture)
			{
			global_window_ptr = this;

			inner.setPosition(position); inner.setSize(size);

			handle = inner.getSystemHandle();



			sfml_window_proc = GetWindowLongPtr(handle, GWLP_WNDPROC);
			SetWindowLongPtr(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc));



			MARGINS margins;
			margins.cxLeftWidth = -1;
			SetWindowLong(handle, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			DwmExtendFrameIntoClientArea(handle, &margins);

			if (texture) { bg_image.setTexture(*texture); bg_image.setColor(bg_color); }
			}

		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Properties ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		sf::Vector2i get_position() { return inner.getPosition(); }
		sf::Vector2u get_size() { return inner.getSize(); }
		int get_x() { return get_position().x; }
		int get_y() { return get_position().y; }
		unsigned get_width() { return get_size().x; }
		unsigned get_height() { return get_size().y; }

		__declspec(property(get = get_x)) int x;
		__declspec(property(get = get_y)) int y;
		__declspec(property(get = get_width)) unsigned width;
		__declspec(property(get = get_height)) unsigned height;
		__declspec(property(get = get_position)) sf::Vector2i position;
		__declspec(property(get = get_size)) sf::Vector2u size;

		sf::Vector2u size_min = {32, 32};
		sf::Vector2u size_max = {sf::VideoMode::getDesktopMode().width, sf::VideoMode::getDesktopMode().height};

		bool is_resizing() { return resize != Resize::NONE; }
		bool is_open() { return inner.isOpen(); }
		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Functions ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		bool poll_event(sf::Event& event)
			{
		SKIP: //goto SKIP polls a new event; this means the outside caller will not catch events generated by resizing operations.
			bool tmp = inner.pollEvent(event);
			return tmp;
			}
		bool wait_event(sf::Event& event)
			{
		SKIP: //goto SKIP polls a new event; this means the outside caller will not catch events generated by resizing operations.
			bool tmp = inner.waitEvent(event);
			return tmp;
			}

		void draw()
			{
			clear();
			display();
			}

		void clear()
			{
			if (has_bg_image) { inner.clear(sf::Color::Transparent); inner.draw(bg_image); }
			else { inner.clear(bg_color); }
			}

		void display() { inner.display(); }
	};

LRESULT hit_test(const Window& window, POINT cursor)
	{
	// identify borders and corners to allow resizing the window.
	// Note: On Windows 10, windows behave differently and
	// allow resizing outside the visible window frame.
	// This implementation does not replicate that behavior.
	const POINT border{
		::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
		::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
		};
	RECT window_rect;
	if (!::GetWindowRect(window.handle, &window_rect))
		{
		return HTNOWHERE;
		}

	const auto drag = borderless_drag ? HTCAPTION : HTCLIENT;

	enum region_mask
		{
		client = 0b0000,
		left = 0b0001,
		right = 0b0010,
		top = 0b0100,
		bottom = 0b1000,
		};

	const auto result =
		left * (cursor.x < (window_rect.left + border.x)) |
		right * (cursor.x >= (window_rect.right - border.x)) |
		top * (cursor.y < (window_rect.top + border.y)) |
		bottom * (cursor.y >= (window_rect.bottom - border.y));

	switch (result)
		{
		case left: return borderless_resize ? HTLEFT : drag;
		case right: return borderless_resize ? HTRIGHT : drag;
		case top: return borderless_resize ? HTTOP : drag;
		case bottom: return borderless_resize ? HTBOTTOM : drag;
		case top | left: return borderless_resize ? HTTOPLEFT : drag;
		case top | right: return borderless_resize ? HTTOPRIGHT : drag;
		case bottom | left: return borderless_resize ? HTBOTTOMLEFT : drag;
		case bottom | right: return borderless_resize ? HTBOTTOMRIGHT : drag;
		case client: return drag;
		default: return HTNOWHERE;
		}
	}
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
	{
	if (msg == WM_NCCREATE)
		{
		auto userdata = reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams;
		// store window instance pointer in window user data
		::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(userdata));
		}
	if (auto window_ptr = Window::global_window_ptr)
		{
		auto& window = *window_ptr;

		switch (msg)
			{
			case WM_NCCALCSIZE:
				{
				if (wparam == TRUE) //&& window.borderless)
					{
					auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
					adjust_maximized_client_rect(hwnd, params.rgrc[0]);
					return 0;
					}
				break;
				}
			case WM_NCHITTEST:
				{
				// When we have no border or title bar, we need to perform our
				// own hit testing to allow resizing and moving.
				if (true)//window.borderless)
					{
					return hit_test(window, POINT{
						GET_X_LPARAM(lparam),
						GET_Y_LPARAM(lparam)
						});
					}
				break;
				}
			case WM_NCACTIVATE:
				{
				if (!composition_enabled())
					{
					// Prevents window frame reappearing on window activation
					// in "basic" theme, where no aero shadow is present.
					return 1;
					}
				break;
				}
			}
		return CallWindowProc(reinterpret_cast<WNDPROC>(window.sfml_window_proc), hwnd, msg, wparam, lparam);
		}
	return ::DefWindowProcW(hwnd, msg, wparam, lparam);
	}



int main()
	{
	sf::Texture bg;
	bg.loadFromFile("Aegdaine.png");

	Window window("Title", {0, 0}, {800, 600}, {255, 255, 255, 120}, &bg);

	while (window.is_open())
		{
		sf::Event event;
		if (window.wait_event(event))
			{
			//do stuff
			}

		window.clear();
		window.display();
		}

	}*/