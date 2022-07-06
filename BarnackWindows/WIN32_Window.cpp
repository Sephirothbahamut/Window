#include <stdexcept>
#include <string>
#include <iostream>
#include <optional>
#include <vector>

#include <utils/window.h>
#include <windowsx.h>

#include <utils/math/vec2.h>
#include <utils/graphics/color.h>
static std::string GetScanCodeName(uint16_t scanCode)
	{
	const bool isExtendedKey = (scanCode >> 8) != 0;

	// Some extended keys doesn't work properly with GetKeyNameTextW API
	if (isExtendedKey)
		{
		const uint16_t vkCode = LOWORD(MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK_EX));
		switch (vkCode)
			{
			case VK_BROWSER_BACK:
				return "Browser Back";
			case VK_BROWSER_FORWARD:
				return "Browser Forward";
			case VK_BROWSER_REFRESH:
				return "Browser Refresh";
			case VK_BROWSER_STOP:
				return "Browser Stop";
			case VK_BROWSER_SEARCH:
				return "Browser Search";
			case VK_BROWSER_FAVORITES:
				return "Browser Favorites";
			case VK_BROWSER_HOME:
				return "Browser Home";
			case VK_VOLUME_MUTE:
				return "Volume Mute";
			case VK_VOLUME_DOWN:
				return "Volume Down";
			case VK_VOLUME_UP:
				return "Volume Up";
			case VK_MEDIA_NEXT_TRACK:
				return "Next Track";
			case VK_MEDIA_PREV_TRACK:
				return "Previous Track";
			case VK_MEDIA_STOP:
				return "Media Stop";
			case VK_MEDIA_PLAY_PAUSE:
				return "Media Play/Pause";
			case VK_LAUNCH_MAIL:
				return "Launch Mail";
			case VK_LAUNCH_MEDIA_SELECT:
				return "Launch Media Selector";
			case VK_LAUNCH_APP1:
				return "Launch App 1";
			case VK_LAUNCH_APP2:
				return "Launch App 2";
			}
		}

	const LONG lParam = MAKELONG(0, (isExtendedKey ? KF_EXTENDED : 0) | (scanCode & 0xff));
	wchar_t name[128] = {};
	size_t nameSize = ::GetKeyNameTextW(lParam, name, sizeof(name));

	std::string ret;
	std::transform(name, name + nameSize, std::back_inserter(ret), [](wchar_t c) {
		return (char)c;
		});

	return ret;
	}
/*std::string VirtualKeyCodeToString(UCHAR virtualKey)
	{
	UINT scanCode = MapVirtualKey(virtualKey, MAPVK_VK_TO_VSC);

	CHAR szName[128];
	int result = 0;
	switch (virtualKey)
		{
		case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
		case VK_RCONTROL: case VK_RMENU:
		case VK_LWIN: case VK_RWIN: case VK_APPS:
		case VK_PRIOR: case VK_NEXT:
		case VK_END: case VK_HOME:
		case VK_INSERT: case VK_DELETE:
		case VK_DIVIDE:
		case VK_NUMLOCK:
			scanCode |= KF_EXTENDED;
		default:
			result = GetKeyNameTextA(scanCode << 16, szName, 128);
		}
	if (result == 0)
		throw std::system_error(std::error_code(GetLastError(), std::system_category()),
			"WinAPI Error occured.");
	return szName;
	}*/

std::system_error last_error(const std::string& message)
	{
	return std::system_error(std::error_code(::GetLastError(), std::system_category()), message);
	}

namespace details
	{
	// we cannot just use WS_POPUP style
	// WS_THICKFRAME: without this the window cannot be resized and so aero snap, de-maximizing and minimizing won't work
	// WS_SYSMENU: enables the context menu with the move, close, maximize, minize... commands (shift + right-click on the task bar item)
	// WS_CAPTION: enables aero minimize animation/transition
	// WS_MAXIMIZEBOX, WS_MINIMIZEBOX: enable minimize/maximize
	enum class Style : DWORD 
		{
		windowed         = WS_OVERLAPPEDWINDOW | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX,
		aero_borderless  = WS_POPUP            | WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
		basic_borderless = WS_POPUP            | WS_THICKFRAME              | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX
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

template <typename T = void>
class Window
	{
	private: //Setup stuff
		inline static const wchar_t* window_class;
		void set_window_ptr() { SetWindowLongPtr(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&user_data)); }
		inline static Window* get_window_ptr(HWND handle) { auto tmp = reinterpret_cast<user_data_t*>(GetWindowLongPtr(handle, GWLP_USERDATA)); return tmp ? tmp->window : nullptr; }

		inline static LRESULT __stdcall window_procedure(HWND handle, UINT msg, WPARAM wparam, LPARAM lparam) noexcept
			{
			if (msg == WM_NCCREATE)
				{
				auto userdata = reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams;
				// store window instance pointer in window user data
				::SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(userdata));
				return ::DefWindowProc(handle, msg, wparam, lparam);
				}
			if (Window* window_ptr{get_window_ptr(handle)}) { return window_ptr->procedure(msg, wparam, lparam); }
			else { return ::DefWindowProc(handle, msg, wparam, lparam); }
			}

		struct user_data_t { Window* window; T* userdata; };
		user_data_t user_data{this, nullptr};

	public:
		inline static void init()
			{
			auto hBrush        {CreateSolidBrush(RGB(0, 0, 0))};
			WNDCLASSEXW wcx   
				{
				.cbSize        {sizeof(wcx)                      },
				.style         {CS_HREDRAW | CS_VREDRAW          },
				.lpfnWndProc   {window_procedure                 },
				.hInstance     {nullptr                          },
				.hCursor       {::LoadCursorW(nullptr, IDC_ARROW)},
				.hbrBackground {hBrush                           },
				.lpszClassName {L"window_class"                  },
				};

			const ATOM result{::RegisterClassExW(&wcx)};
			if (!result) { throw last_error("failed to register window class"); }
			window_class = wcx.lpszClassName;
			}

		struct initializer_t
			{
			std::wstring title{L"Untitled Window"};

			std::optional<utils::math::vec2i> position{std::nullopt};
			utils::math::vec2u size{800u, 600u};
			utils::graphics::color background_color{utils::graphics::color::black};
			bool glass{false};
			bool shadow{true};
			};

		Window(const initializer_t& initializer) : background_color{initializer.background_color}
			{
			int x{initializer.position ? initializer.position.value().x : CW_USEDEFAULT};
			int y{initializer.position ? initializer.position.value().y : CW_USEDEFAULT};

			DWORD style{static_cast<DWORD>(details::select_borderless_style())};

			handle = ::CreateWindowExW
				(
				0, window_class, initializer.title.c_str(),
				style, x, y,
				initializer.size.x, initializer.size.y, nullptr, nullptr, nullptr, this
				);
			if (!handle) { throw last_error("failed to create window"); }

			if (initializer.background_color.a < 255)
				{
				if (initializer.glass && utils::window::make_glass_CompositionAttribute(handle)) 
					{
					std::cout << "lol";
					}
				else 
					{ 
					utils::window::make_transparent_Layered(handle, static_cast<BYTE>(initializer.background_color.a)); 
					}
				}
			details::set_shadow(handle, initializer.shadow);

			// redraw frame
			::SetWindowPos(handle, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
			::ShowWindow(handle, SW_SHOW);

			//mouse
			RAWINPUTDEVICE rid_mouse
				{
				.usUsagePage{0x01},
				.usUsage    {0x02/*Mouse*/},
				.dwFlags    {RIDEV_INPUTSINK},
				.hwndTarget {handle},
				};
			if (!RegisterRawInputDevices(&rid_mouse, 1, sizeof(rid_mouse))) { throw last_error("failed to register raw input devices"); }

			//controller
			RAWINPUTDEVICE rid_controller
				{
				.usUsagePage{0x01},
				.usUsage    {0x05/*gamepad*/},
				.dwFlags    {RIDEV_INPUTSINK},
				.hwndTarget {handle},
				};
			if (!RegisterRawInputDevices(&rid_controller, 1, sizeof(rid_controller))) { throw last_error("failed to register raw input devices"); }

			//keyboard
			RAWINPUTDEVICE rid_keyboard
				{
				.usUsagePage{0x01},
				.usUsage    {0x06/*Keyboard*/},
				.dwFlags    {RIDEV_INPUTSINK},
				.hwndTarget {handle},
				};
			if (!RegisterRawInputDevices(&rid_keyboard, 1, sizeof(rid_keyboard))) { throw last_error("failed to register raw input devices"); }

			}
		Window(const Window& copy) = delete;
		Window& operator=(const Window& copy) = delete;
		Window(Window&& move) noexcept  
			{
			handle = move.handle; 
			move.handle = nullptr;
			set_window_ptr();
			}
		Window& operator=(Window&& move) noexcept
			{
			handle = move.handle;
			move.handle = nullptr;
			set_window_ptr();
			}
		~Window() noexcept { if (handle) { ::DestroyWindow(handle); } }

		void set_user_data(T* data) noexcept { user_data.userdata = data; }
		T* get_user_data() noexcept { return user_data.userdata; }


#pragma region Styling

		void set_transparency(bool transparent, bool glass)
			{
			if (transparent && glass)
				{
				if (!utils::window::make_glass_CompositionAttribute(handle))
					{
					utils::window::make_transparent_Layered(handle);
					}
				}
			else if (transparent)
				{
				utils::window::make_transparent_Layered(handle);
				}
			//	utils::window::make_glass_DWM_BlurBehind(handle);
			//	utils::window::make_glass_DWM_margin(handle);
			}
		void set_shadow(bool enabled) { details::set_shadow(handle, enabled); }


#pragma endregion Styling

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
		void set_size    (const utils::math::vec2u& size)     noexcept 
			{
			// SetWindowPos wants the total size of the window (including title bar and borders),
			// so we have to compute it
			RECT rectangle = {0, 0, static_cast<long>(size.x), static_cast<long>(size.y)};
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
		void      set_width (unsigned x) noexcept { set_size({x, get_height()}); }
		void      set_height(unsigned y) noexcept { set_size({get_width(), y }); }

		__declspec(property(get = get_x,        put = set_x))        int                x;
		__declspec(property(get = get_y,        put = set_y))        int                y;
		__declspec(property(get = get_width,    put = set_width))    unsigned           width;
		__declspec(property(get = get_height,   put = set_height))   unsigned           height;
		__declspec(property(get = get_position, put = set_position)) utils::math::vec2i position;
		__declspec(property(get = get_size,     put = set_size))     utils::math::vec2u size;
#pragma endregion

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

#pragma region Callbacks
		void (*callback_resizing)(Window&, utils::math::vec2u) { nullptr };
		void (*callback_resized)(Window&, utils::math::vec2u) { nullptr };

#pragma endregion

	protected:
		inline bool is_in_drag_region(utils::math::vec2u position) const noexcept { return true; }

	private:
		inline bool is_in_drag_region_wrapper(RECT& window_rect, POINT position) const noexcept
			{
			utils::math::vec2u relative_position{static_cast<unsigned>(position.x - window_rect.left), static_cast<unsigned>(position.y - window_rect.top)};
			return is_in_drag_region(relative_position);
			}
		HWND handle = nullptr;
		utils::graphics::color background_color{utils::graphics::color::black};

		LRESULT hit_test(POINT cursor) const noexcept
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

			enum region_mask
				{
				client = 0b0000,
				left = 0b0001,
				right = 0b0010,
				top = 0b0100,
				bottom = 0b1000,
				};

			if (resizable) 
				{
				const auto result =
					left   * (cursor.x <  (window.left   + border.x)) |
					right  * (cursor.x >= (window.right  - border.x)) |
					top    * (cursor.y <  (window.top    + border.y)) |
					bottom * (cursor.y >= (window.bottom - border.y));
				
				switch (result)
					{
					case left:           return HTLEFT       ;
					case right:          return HTRIGHT      ;
					case top:            return HTTOP        ;
					case bottom:         return HTBOTTOM     ;
					case top    | left:  return HTTOPLEFT    ;
					case top    | right: return HTTOPRIGHT   ;
					case bottom | left:  return HTBOTTOMLEFT ;
					case bottom | right: return HTBOTTOMRIGHT;
					case client:         return is_in_drag_region_wrapper(window, cursor) ? HTCAPTION : HTCLIENT;
					default: return HTNOWHERE;
					}
				}
			else
				{
				if (cursor.x >= window.left && cursor.x <= window.right && cursor.y >= window.top && cursor.y <= window.bottom)
					{
					return is_in_drag_region_wrapper(window, cursor) ? HTCAPTION : HTCLIENT;
					}
				else { return HTNOWHERE; }
				}
			}

		LRESULT procedure(UINT msg, WPARAM wparam, LPARAM lparam) noexcept
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
						details::adjust_maximized_client_rect(handle, params.rgrc[0]);
						return 0;
						}
					break;
					}
				case WM_NCHITTEST:
					{
					// When we have no border or title bar, we need to perform our
					// own hit testing to allow resizing and moving.

					return hit_test(POINT{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)});
					}
				case WM_NCACTIVATE:
					{
					if (!details::composition_enabled())
						{
						// Prevents window frame reappearing on window activation
						// in "basic" theme, where no aero shadow is present.
						return 1;
						}
					break;
					}

				case WM_CLOSE:
					{
					::DestroyWindow(handle);
					return 0;
					}

				case WM_DESTROY:
					{
					PostQuitMessage(0);
					return 0;
					}

				case WM_ERASEBKGND:
					{
					HDC hdc = HDC(wparam);
					RECT rect;
					GetClientRect(handle, &rect);
					SetMapMode(hdc, MM_ANISOTROPIC);
					SetWindowExtEx(hdc, 100, 100, NULL);
					SetViewportExtEx(hdc, rect.right, rect.bottom, NULL);

					HBRUSH brush{CreateSolidBrush(RGB(background_color.r, background_color.g, background_color.b))};
					FillRect(hdc, &rect, brush);
					
					//break;
					return 1L;
					}

#pragma region Events
				case WM_SIZING:
					{
					if (callback_resizing) { (*callback_resizing)(*this, size); return TRUE; }
					break;
					}
				case WM_SIZE:
					{
					if (callback_resized) { (*callback_resized)(*this, size); return TRUE; }
					break;
					}

				case WM_INPUT:
					{

					UINT dataSize;
					GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, NULL, &dataSize, sizeof(RAWINPUTHEADER)); //Need to populate data size first
					//std::cout << GET_RAWINPUT_CODE_WPARAM(wparam) << " code thing\n";
					if (dataSize > 0)
						{
						std::vector<BYTE> rawdata(dataSize);

						if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, rawdata.data(), &dataSize, sizeof(RAWINPUTHEADER)) == dataSize)
							{
							RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(rawdata.data());
							switch (raw->header.dwType)
								{
								case RIM_TYPEMOUSE:
									{
									// https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawmouse
									//TODO wheel
									std::cout << "Mouse handle " << raw->header.hDevice << "\n";
									const auto& rawmouse{raw->data.mouse};

									if ((rawmouse.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE)
										{
										bool isVirtualDesktop = (rawmouse.usFlags & MOUSE_VIRTUAL_DESKTOP) == MOUSE_VIRTUAL_DESKTOP;

										int width  = GetSystemMetrics(isVirtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
										int height = GetSystemMetrics(isVirtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);

										int absoluteX = int((rawmouse.lLastX / 65535.0f) * width);
										int absoluteY = int((rawmouse.lLastY / 65535.0f) * height);

										std::cout << "absolute: " << absoluteX << ", " << absoluteY << "\n";
										}
									else if (rawmouse.lLastX != 0 || rawmouse.lLastY != 0)
										{
										int relativeX = rawmouse.lLastX;
										int relativeY = rawmouse.lLastY;

										std::cout << "relative: " << relativeX << ", " << relativeY << "\n";
										}

									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN) { std::cout << "Left    pressed \n" ; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP  ) { std::cout << "Left    released\n"; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN) { std::cout << "Right   pressed \n" ; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP  ) { std::cout << "Right   released\n"; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN) { std::cout << "Middle  pressed \n" ; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP  ) { std::cout << "Middle  released\n"; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) { std::cout << "Forward pressed \n" ; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP  ) { std::cout << "Forward released\n"; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) { std::cout << "Back    pressed \n" ; }
									if (rawmouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP  ) { std::cout << "Back    released\n"; }
									}
									break;

								case RIM_TYPEKEYBOARD:
									{
									// https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawkeyboard
									std::cout << "Keyboard handle " << raw->header.hDevice << "\n";
									const auto& rawkb{raw->data.keyboard};

									if (rawkb.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || rawkb.VKey == 0xff/*VK__none_*/)
										{
										break;
										}

									try
										{// https://stackoverflow.com/questions/38584015/using-getkeynametext-with-special-keys
										uint16_t scanCode = rawkb.MakeCode;
										scanCode |= (rawkb.Flags & RI_KEY_E0) ? 0xe000 : 0;
										scanCode |= (rawkb.Flags & RI_KEY_E1) ? 0xe100 : 0;

										constexpr uint16_t c_BreakScanCode  {0xe11d}; // emitted on Ctrl+NumLock
										constexpr uint16_t c_NumLockScanCode{0xe045};
										constexpr uint16_t c_PauseScanCode{0x0045};
										// These are special for historical reasons
										// https://en.wikipedia.org/wiki/Break_key#Modern_keyboards
										// Without it GetKeyNameTextW API will fail for these keys
										if (scanCode == c_BreakScanCode) { scanCode = c_PauseScanCode; }
										else if (scanCode == c_PauseScanCode) { scanCode = c_NumLockScanCode; }

										std::cout << GetScanCodeName(scanCode) << " ";

										if ((rawkb.Flags & RI_KEY_BREAK) == RI_KEY_BREAK) { std::cout << "released\n"; }
										else if ((rawkb.Flags & RI_KEY_MAKE ) == RI_KEY_MAKE ) { std::cout << "pressed \n"; }
										//else if ((rawkb.Flags & RI_KEY_E0   ) == RI_KEY_E0   ) { std::cout << "E0 whatever it means\n"; }
										//else if ((rawkb.Flags & RI_KEY_E1   ) == RI_KEY_E1   ) { std::cout << "E1 whatever it means\n"; }
										}
									catch (const std::system_error& error) { std::cout << error.what() << "\n"; }
									}
									break;
								//case RIM_TYPEMOUSE:
									//std::cout << "Mouse handle " << raw->header.hDevice << ": " << raw->data.mouse.lLastX << ", " << raw->data.mouse.lLastY << std::endl;
									//break;
								}
							}
						}
					}

				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					{
					switch (wparam)
						{
						case VK_F8:;
						}
					break;
					}
#pragma endregion
				}
			return DefWindowProc(handle, msg, wparam, lparam);
			}
	};



int main()
	{
	Window<void>::init();

	using namespace utils::graphics;
	color bg{color::black};
	bg.float_a = .5f;
	bg.v /= 10.f;

	try
		{
		Window<void> window{Window<void>::initializer_t
			{
			.title{L"Test window"},
			.background_color{bg},
			.glass{true}
			}};

		window.callback_resizing = [] (Window<void>& window, utils::math::vec2u size)
			{
			std::cout << "Resizing: " << size.x << ", " << size.y << "\n";
			};

		while (window.poll_event())
			{
			}
		}
	catch (const std::system_error& e) { ::MessageBoxA(nullptr, e.what(), "Unhandled Exception", MB_OK | MB_ICONERROR); }
	}/**/