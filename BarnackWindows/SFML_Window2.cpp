#include <stdexcept>
#include <string>
#include <iostream>
#include <optional>

#include <utils/window.h>
#include <windowsx.h>

#include <utils/math/vec2.h>
#include <utils/graphics/color.h>

#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
/*
std::system_error last_error(const std::string& message)
	{
	return std::system_error(std::error_code(::GetLastError(), std::system_category()), message);
	}

namespace
	{
	// we cannot just use WS_POPUP style
	// WS_THICKFRAME: without this the window cannot be resized and so aero snap, de-maximizing and minimizing won't work
	// WS_SYSMENU: enables the context menu with the move, close, maximize, minize... commands (shift + right-click on the task bar item)
	// WS_CAPTION: enables aero minimize animation/transition
	// WS_MAXIMIZEBOX, WS_MINIMIZEBOX: enable minimize/maximize
	enum class Style : DWORD
		{
		windowed         = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		aero_borderless  = WS_POPUP | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
		basic_borderless = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
		};

	bool composition_enabled() noexcept
		{
		BOOL composition_enabled = FALSE;
		bool success = ::DwmIsCompositionEnabled(&composition_enabled) == S_OK;
		return composition_enabled && success;
		}
	inline Style select_borderless_style() noexcept
		{
		return composition_enabled() ? Style::aero_borderless : Style::basic_borderless;
		}

	inline void set_shadow(HWND handle, bool enabled)
		{
		if (composition_enabled())
			{
			static const MARGINS shadow_state[2]{{ 0,0,0,0 },{ 1,1,1,1 }};
			::DwmExtendFrameIntoClientArea(handle, &shadow_state[enabled]);
			}
		}

	auto maximized(HWND hwnd) -> bool
		{
		WINDOWPLACEMENT placement;
		if (!::GetWindowPlacement(hwnd, &placement))
			{
			return false;
			}

		return placement.showCmd == SW_MAXIMIZE;
		}

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

	}

class Win32SFML_Window : public sf::RenderWindow
	{
	using Window = Win32SFML_Window;
	private: //Setup stuff
		inline static const wchar_t* window_class;
		void set_window_ptr() { SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this)); }
		inline static Window* get_window_ptr(HWND handle) { return reinterpret_cast<Window*>(GetWindowLongPtr(handle, GWLP_USERDATA)); }

		inline static LRESULT __stdcall window_procedure(HWND handle, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
			{
			auto ret{CallWindowProc(reinterpret_cast<WNDPROC>(SFML_event_callback), handle, msg, wparam, lparam)};

			return get_window_ptr(handle)->procedure(msg, wparam, lparam, ret);
			}

		inline static LONG_PTR SFML_event_callback;

	public:

		struct initializer_t
			{
			std::wstring title{L"Untitled Window"};

			std::optional<utils::math::vec2i> position{std::nullopt};
			utils::math::vec2u size{800u, 600u};
			utils::graphics::color background_color{utils::graphics::color::black};
			};

		Win32SFML_Window(const initializer_t& initializer) : background_color{initializer.background_color},
			sf::RenderWindow{sf::VideoMode{initializer.size.x, initializer.size.y}, initializer.title, sf::Style::None}
			{
			int x{initializer.position ? initializer.position.value().x : CW_USEDEFAULT};
			int y{initializer.position ? initializer.position.value().y : CW_USEDEFAULT};

			set_window_ptr();
			handle = getSystemHandle();

			SFML_event_callback = SetWindowLongPtr(handle, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(window_procedure));

			//SetWindowLongPtr(handle, GWL_STYLE, static_cast<DWORD>(select_borderless_style()));

			//set_shadow(handle, true);

			//utils::window::make_glass_CompositionAttribute(handle);
			//	utils::window::make_glass_DWM_BlurBehind(handle);
			//	utils::window::make_glass_DWM_margin(handle);
			//	utils::window::make_transparent_Layered(handle);

			// redraw frame
			//::SetWindowPos(handle, nullptr, x, y, 0, 0, SWP_FRAMECHANGED | SWP_NOSIZE);
			//::ShowWindow(handle, SW_SHOW);
			}
		Win32SFML_Window(const Window& copy) = delete;
		Win32SFML_Window& operator=(const Window& copy) = delete;
		Win32SFML_Window(Window&& move) noexcept
			{
			handle = move.handle;
			move.handle = nullptr;
			set_window_ptr();
			}
		Win32SFML_Window& operator=(Window&& move) noexcept
			{
			handle = move.handle;
			move.handle = nullptr;
			set_window_ptr();
			}
		~Win32SFML_Window() noexcept { if (handle) { ::DestroyWindow(handle); } }

		std::optional<int> poll_event() const
			{
			MSG msg;
			BOOL ret = GetMessage(&msg, handle, 0, 0);

			if (ret < 0) { throw last_error("Error polling window events"); }
			else if (ret > 0)
				{
				::TranslateMessage(&msg);
				::DispatchMessage(&msg);
				return {1};
				}
			else { return std::nullopt; }
			}

#pragma region Properties
		bool resizable = true;
		utils::math::vec2u size_min{32, 32};

		utils::math::vec2i get_position() const noexcept
			{
			RECT rect;
			GetWindowRect(handle, &rect);
			return {rect.left, rect.top};
			}
		utils::math::vec2u get_size() const noexcept
			{
			RECT rect;
			GetClientRect(handle, &rect);
			return {static_cast<unsigned>(rect.right), static_cast<unsigned>(rect.bottom)};
			}
		void set_position(const utils::math::vec2i& position) noexcept { SetWindowPos(handle, NULL, position.x, position.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }
		void set_size(const utils::math::vec2u& size)     noexcept
			{
			// SetWindowPos wants the total size of the window (including title bar and borders),
			// so we have to compute it
			RECT rectangle ={0, 0, static_cast<long>(size.x), static_cast<long>(size.y)};
			AdjustWindowRect(&rectangle, GetWindowLongPtr(handle, GWL_STYLE), false);
			int width  = rectangle.right - rectangle.left;
			int height = rectangle.bottom - rectangle.top;

			SetWindowPos(handle, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
			}

		int       get_x()      const noexcept { return get_position().x; }
		int       get_y()      const noexcept { return get_position().y; }
		unsigned  get_width()  const noexcept { return get_size().x; }
		unsigned  get_height() const noexcept { return get_size().y; }

		void      set_x(int x)           noexcept { set_position({x, get_y()}); }
		void      set_y(int y)           noexcept { set_position({get_x(), y}); }
		void      set_width(unsigned x) noexcept { set_size({x, get_height()}); }
		void      set_height(unsigned y) noexcept { set_size({get_width(), y}); }

		__declspec(property(get = get_x, put = set_x))        int                x;
		__declspec(property(get = get_y, put = set_y))        int                y;
		__declspec(property(get = get_width, put = set_width))    unsigned           width;
		__declspec(property(get = get_height, put = set_height))   unsigned           height;
		__declspec(property(get = get_position, put = set_position)) utils::math::vec2i position;
		__declspec(property(get = get_size, put = set_size))     utils::math::vec2u size;
#pragma endregion

#pragma region Callbacks
		void (*callback_resizing)(Window&, utils::math::vec2u) { nullptr };
		void (*callback_resized)(Window&, utils::math::vec2u) { nullptr };

#pragma endregion

	private:
		HWND handle = nullptr;
		utils::graphics::color background_color{utils::graphics::color::black};

		inline static LRESULT hit_test(POINT cursor, HWND handle) noexcept
			{
			// identify borders and corners to allow resizing the window.
			// Note: On Windows 10, windows behave differently and
			// allow resizing outside the visible window frame.
			// This implementation does not replicate that behavior.
			const POINT border{
				::GetSystemMetrics(SM_CXFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER),
				::GetSystemMetrics(SM_CYFRAME) + ::GetSystemMetrics(SM_CXPADDEDBORDER)
				};
			RECT window;
			if (!::GetWindowRect(handle, &window))
				{
				return HTNOWHERE;
				}

			const auto drag = HTCAPTION; //HCAPTION drags, HCLIENT doesn't when has defaulttitle bar

			enum region_mask
				{
				client = 0b0000,
				left   = 0b0001,
				right  = 0b0010,
				top    = 0b0100,
				bottom = 0b1000,
				};

			const auto result =
				left * (cursor.x < (window.left + border.x)) |
				right * (cursor.x >= (window.right - border.x)) |
				top * (cursor.y < (window.top + border.y)) |
				bottom * (cursor.y >= (window.bottom - border.y));

			switch (result)
				{
				case left:           return HTLEFT       ;
				case right:          return HTRIGHT      ;
				case top:            return HTTOP        ;
				case bottom:         return HTBOTTOM     ;
				case top | left:     return HTTOPLEFT    ;
				case top | right:    return HTTOPRIGHT   ;
				case bottom | left:  return HTBOTTOMLEFT ;
				case bottom | right: return HTBOTTOMRIGHT;
				case client: return drag;
				default: return HTNOWHERE;
				}
			}

		LRESULT procedure(UINT msg, WPARAM wparam, LPARAM lparam, LRESULT sfml_lresult) noexcept
			{

			switch (msg)
				{
				case WM_GETMINMAXINFO:
					{
					LPMINMAXINFO lpMMI = (LPMINMAXINFO)lparam;
					lpMMI->ptMinTrackSize.x = size_min.x;
					lpMMI->ptMinTrackSize.y = size_min.y;
					break;
					}
				case WM_NCCALCSIZE:
					{
					if (wparam == TRUE)
						{
						auto& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
						adjust_maximized_client_rect(handle, params.rgrc[0]);
						sfml_lresult = 0;
						}
					break;
					}
				case WM_NCHITTEST:
					{
					// When we have no border or title bar, we need to perform our
					// own hit testing to allow resizing and moving.
					std::cout << hit_test(POINT{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}, getSystemHandle()) << std::endl;
					return hit_test(POINT{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}, getSystemHandle());
					}
				case WM_NCACTIVATE:
					{
					if (!composition_enabled())
						{
						// Prevents window frame reappearing on window activation
						// in "basic" theme, where no aero shadow is present.
						sfml_lresult = 1;
						}
					break;
					}

				//case WM_ERASEBKGND:
				//	{
				//	HDC hdc = HDC(wparam);
				//	RECT rect;
				//	GetClientRect(handle, &rect);
				//	SetMapMode(hdc, MM_ANISOTROPIC);
				//	SetWindowExtEx(hdc, 100, 100, NULL);
				//	SetViewportExtEx(hdc, rect.right, rect.bottom, NULL);
				//
				//	HBRUSH brush{CreateSolidBrush(RGB(background_color.r, background_color.g, background_color.b))};
				//	FillRect(hdc, &rect, brush);
				//
				//	//break;
				//	return 1L;
				//	}

#pragma region Events
				case WM_SIZING:
					{
					std::cout << "ah";
					if (callback_resizing) { (*callback_resizing)(*this, size); return TRUE; }
					break;
					}
				case WM_SIZE:
					{
					if (callback_resized) { (*callback_resized)(*this, size); return TRUE; }
					break;
					}


				//case WM_KEYDOWN:
				//case WM_SYSKEYDOWN:
				//	{
				//	switch (wparam)
				//		{
				//		case VK_F8:;
				//		}
				//	break;
				//	}
#pragma endregion
				}
			return sfml_lresult;
			}
	};

int main()
	{
	using namespace utils::graphics;
	color bg{color::blue};
	bg.v /= 10.f;


	try
		{
		Win32SFML_Window window{Win32SFML_Window::initializer_t
			{
			.title{L"Test window"},
			.background_color{bg}
			}};

		window.callback_resizing = [] (Win32SFML_Window& window, utils::math::vec2u size)
			{
			std::cout << "uh";
			sf::FloatRect visibleArea{0, 0, (float)size.x, (float)size.y};
			window.setView(sf::View(visibleArea));

			window.clear();

			sf::RectangleShape a({64, 32});
			sf::RectangleShape b({64, 32});
			b.setPosition({window.getSize().x - 64.f, window.getSize().y - 32.f});
			a.setFillColor(sf::Color::Red);
			b.setFillColor(sf::Color::Blue);

			window.draw(a);
			window.draw(b);

			window.display();
			};

		while (window.isOpen())
			{
			sf::Event event;
			if (window.waitEvent(event))
				{
				switch (event.type)
					{
					case sf::Event::MouseMoved: std::cout << "SFML mouse" << std::endl;
						break;
					}
				}


			window.clear(sf::Color{0, 0, 255, 100});

			sf::RectangleShape a({64, 32});
			sf::RectangleShape b({64, 32});
			b.setPosition({window.getSize().x - 64.f, window.getSize().y - 32.f});
			a.setFillColor(sf::Color::Red);
			b.setFillColor(sf::Color::Blue);

			window.draw(a);
			window.draw(b);

			window.display();
			}
		}
	catch (const std::system_error& e) { ::MessageBoxA(nullptr, e.what(), "Unhandled Exception", MB_OK | MB_ICONERROR); }
	}/**/