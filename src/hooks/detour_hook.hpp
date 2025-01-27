#pragma once

#include <polyhook2/Detour/x64Detour.hpp>

namespace big
{
	class detour_hook
	{
	public:
		explicit detour_hook();
		explicit detour_hook(const std::string& name, void* detour);
		explicit detour_hook(const std::string& name, void* target, void* detour);
		~detour_hook() noexcept;

		detour_hook(detour_hook&& that);
		//detour_hook& operator=(detour_hook&& that) = delete;
		//detour_hook(detour_hook const&)            = delete;
		//detour_hook& operator=(detour_hook const&) = delete;

		void set_instance(const std::string& name, void* detour);
		void set_instance(const std::string& name, void* target, void* detour);

		void set_target_and_create_hook(void* target);

		void enable();
		void disable();

		template<typename T>
		T get_original()
		{
			return static_cast<T>(m_original);
		}

		void** get_original_ptr()
		{
			return &m_original;
		}

		void fix_hook_address();

		void set_is_follow_call_on_fn_address(bool value);

	private:
		void create_hook();

		std::string m_name;
		void* m_original;
		void* m_target;
		void* m_detour;

		std::unique_ptr<PLH::x64Detour> m_detour_object;
	};
} // namespace big
