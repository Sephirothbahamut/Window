/*#include <SFML/System.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>

#include <Windows.h>
#pragma comment (lib, "dwmapi.lib")//without this dwmapi.h doesn't work :shrugs: no idea whatsoever where the compiler is taking this file from
//#include <WinUser.h>
#include <dwmapi.h>
//#include <Shlwapi.h>
//#include <tchar.h>

#include <iostream>

class Window
	{
	private:
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
			inner.setPosition(position); inner.setSize(size);

			handle = inner.getSystemHandle();
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
			if (tmp)
				{
				//if clicking and within resizing borders, start resize
				if (event.type == sf::Event::MouseButtonPressed && _button_pressed(event.mouseButton))
					{
					goto SKIP;
					}
				else if (is_resizing())
					{
					switch (event.type)
						{
						case sf::Event::MouseButtonReleased:
							_button_released(event.mouseButton);
							break;
						case sf::Event::MouseMoved:
							_mouse_moved(event.mouseMove);
							break;
						case sf::Event::MouseLeft:
							_mouse_moved(event.mouseMove);
							break;
						case sf::Event::Resized:
							_resized(event.size);
							break;
						default:
							goto PASS;
						}
					goto SKIP;
					}
				}
		PASS:
			return tmp;
			}
		bool wait_event(sf::Event& event)
			{
		SKIP: //goto SKIP polls a new event; this means the outside caller will not catch events generated by resizing operations.
			bool tmp = inner.waitEvent(event);
			if (tmp)
				{
				//if clicking and within resizing borders, start resize
				if (event.type == sf::Event::MouseButtonPressed && _button_pressed(event.mouseButton))
					{
					goto SKIP;
					}
				else if (is_resizing())
					{
					switch (event.type)
						{
						case sf::Event::MouseButtonReleased:
							_button_released(event.mouseButton);
							break;
						case sf::Event::MouseMoved:
							_mouse_moved(event.mouseMove);
							break;
						case sf::Event::MouseLeft:
							_mouse_moved(event.mouseMove);
							break;
						case sf::Event::Resized:
							_resized(event.size);
							break;
						default:
							goto PASS;
						}
					goto SKIP;
					}
				}
		PASS:
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