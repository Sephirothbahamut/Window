#include <SFML/Graphics.hpp>
#include <Windows.h>
//#define GLFW_INCLUDE_NONE
//#define GLFW_INCLUDE_GL
/*#define GLFW_EXPOSE_NATIVE_WIN32
#include <gl/GL.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <string>
#include <stdexcept>


namespace utils
	{
	template <typename T, typename Deleter>
	std::unique_ptr<T, Deleter> make_unique_with_deleter(T* ptr) { return std::unique_ptr<T, Deleter>(ptr); }
	}

namespace glfw
	{
	class Window
		{
		private:
			struct glfwWindowDestructor { void operator()(GLFWwindow* ptr) { glfwDestroyWindow(ptr); } };
			

		public:GLFWwindow* inner;
			Window(const char* window_name = "Window", sf::Vector2i position = sf::Vector2i(0, 0), sf::Vector2i size = sf::Vector2i(800, 600))
				{
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
				inner = glfwCreateWindow(size.x, size.y, window_name, nullptr, nullptr);
				}
			~Window() { glfwDestroyWindow(inner); }

			bool is_open() { return inner; }
			// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Properties ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
			
			HWND get_handle() { return glfwGetWin32Window(inner); }
			
			sf::Vector2i get_position() { sf::Vector2i ret; glfwGetWindowPos(inner, &ret.x, &ret.y); return ret; }
			void         set_position(int x, int y) { glfwSetWindowPos(inner, x, y); }
			void         set_position(const sf::Vector2i& position) { set_position(position.x, position.y); }
			sf::Vector2i get_size() { sf::Vector2i ret; glfwGetWindowSize(inner, &ret.x, &ret.y); return ret; }
			void         set_size(int x, int y) { glfwSetWindowSize(inner, x, y); }
			void         set_size(const sf::Vector2i& size) { set_position(size.x, size.y); }
			int  get_x()           { return get_position().x; }
			void set_x(int x)      { set_position(x, get_y()); }
			int  get_y()           { return get_position().y; }
			void set_y(int y)      { set_position(get_x(), y); }
			int  get_width()       { return get_size().x; }
			void set_width(int x)  { set_size(x, get_height()); }
			int  get_height()      { return get_size().y; }
			void set_height(int y) { set_size(get_width(), y); }

			__declspec(property(get = get_x, put = set_x)) int x;
			__declspec(property(get = get_y, put = set_y)) int y;
			__declspec(property(get = get_width, put = set_width)) int width;
			__declspec(property(get = get_height, put = set_height)) int height;
			__declspec(property(get = get_position, put = set_position)) sf::Vector2i position;
			__declspec(property(get = get_size, put = set_size)) sf::Vector2i size;

			void set_transparent()
				{
				glfwSetWindowAttrib(inner, GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
				}

		};
	}

class Window
	{

	private:
		glfw::Window inner;
		sf::RenderWindow sfrender;

		inline const static int resize_margin = 16;
		enum class Resize { rr, up, ll, dw, ur, ul, dr, dl, NONE };

		sf::Color bg_color;
		sf::Sprite bg_image;
		bool has_bg_image;

		Resize resize = Resize::NONE;
		sf::Vector2i resize_mouse_delta;

	public:
		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Constructors & Destructors ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		Window(const char* window_name = "Window", sf::Vector2i position = sf::Vector2i(0, 0), sf::Vector2i size = sf::Vector2i(800, 600), sf::Color color = sf::Color::Black, const sf::Texture* const texture = nullptr)
			: inner(window_name, position, size), sfrender(inner.get_handle())
			{
			if (texture) { bg_image.setTexture(*texture); bg_image.setColor(bg_color); }

			glfwSetWindowUserPointer(inner.inner, this);
			glfwSetFramebufferSizeCallback(inner.inner, &framebuffer_size_callback);
			}

		inline static void framebuffer_size_callback(GLFWwindow* window, int width, int height)
			{
			// make sure the viewport matches the new window dimensions; note that width and 
			// height will be significantly larger than specified on retina displays.
			glViewport(0, 0, width, height);
			// Re-render the scene because the current frame was drawn for the old resolution
			static_cast<Window*>(glfwGetWindowUserPointer(window))->draw();
			}

		Window(const Window& copy) = delete;
		Window& operator=(const Window& copy) = delete;
		Window(Window&& move) noexcept = default;
		Window& operator=(Window && move) = default;

		bool is_open() { return inner.is_open(); }

		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Properties ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		sf::Vector2i get_position() { return inner.get_position(); }
		sf::Vector2i get_size() { return inner.get_size(); }
		int get_x() { return get_position().x; }
		int get_y() { return get_position().y; }
		int get_width() { return get_size().x; }
		int get_height() { return get_size().y; }

		__declspec(property(get = get_x)) int x;
		__declspec(property(get = get_y)) int y;
		__declspec(property(get = get_width)) int width;
		__declspec(property(get = get_height)) int height;
		__declspec(property(get = get_position)) sf::Vector2i position;
		__declspec(property(get = get_size)) sf::Vector2i size;

		bool is_resizing() { return resize != Resize::NONE; }
		// ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== Functions ===== ===== ===== ===== ===== ===== ===== ===== ===== ===== 
		bool poll_event(sf::Event& event)
			{
			glfwPollEvents();
			return true;
			}
		bool wait_event(sf::Event& event)
			{
			glfwWaitEvents();
			return true;
			}

		void draw()
			{
			clear();
			display();
			}

		void clear()
			{
			if (has_bg_image) { sfrender.clear(sf::Color::Transparent); sfrender.draw(bg_image); }
			else { sfrender.clear(bg_color); }
			}

		void display() { sfrender.display(); }
	};

#include <exception>
#include <iostream>
namespace glfw
	{
	void error_callback(int error, const char* description) noexcept 
		{
		using namespace std::literals::string_literals;
		throw std::runtime_error{"Error #"s + std::to_string(error) + ": "s + description};
		}
	class Initializer
		{
		private:
			inline static bool initialized = false;
			bool first = false;
		public:
			Initializer() 
				{
				if (!initialized)
					{
					first = true; 
					if (!glfwInit()) { throw std::exception("Failed to initialize GLFW."); };
					glfwSetErrorCallback(error_callback);
					initialized = true;
					}
				}
			Initializer(const Initializer& copy) = delete;
			Initializer(Initializer&& move) noexcept { first = move.first; move.first = false; }
			~Initializer() noexcept { if (first) { glfwTerminate(); } }
		};
	}

int main()
	{
	glfw::Initializer glfw_initializer;

	sf::Texture bg;
	bg.loadFromFile("Aegdaine.png");

	Window window("Title", {0, 0}, {800, 600}, {0, 130, 255, 80}, &bg);

	while (window.is_open())
		{
		sf::Event event;
		if (window.wait_event(event))
			{
			}
		std::cout << "asd" << "\n";

		window.clear();
		window.display();
		}
	}*/