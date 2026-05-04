#include "rom.hpp"

#include "logger/logger.hpp"
#include "paths/paths.hpp"

#include <string/string_conversions.hpp>

#include <fstream>

namespace rom
{
	struct store_reason_to_file
	{
		std::filesystem::path m_file_path;
		bool m_file_exists = false;

		store_reason_to_file()
		{
			const auto main_module_folder = big::paths::get_main_module_folder();

			constexpr auto file_name = "ReturnOfModdingFirstEnabledReason.txt";

			m_file_path   = main_module_folder / file_name;
			m_file_exists = std::filesystem::exists(m_file_path);

			std::ifstream file_stream(m_file_path);
			if (!file_stream.is_open())
			{
				return;
			}

			uint16_t first_enabled_reason = 0;
			file_stream >> first_enabled_reason;
			g_first_enabled_reason = (enabled_reason)first_enabled_reason;

			file_stream.close();
		}

		~store_reason_to_file()
		{
			if (m_file_exists)
			{
				return;
			}

			std::ofstream file_stream(m_file_path);
			if (!file_stream.is_open())
			{
				return;
			}

			file_stream << (uint16_t)g_enabled_reason << std::endl;
			file_stream.close();
		}
	};

	// Check if the mod manager profile has a newer mod loader DLL and copy
	// it to the game directory. Auto-detects the DLL filename from the
	// module that contains this function. Only called when
	// rom_modding_root_folder is provided via command line arguments.
	static void try_update_dll_from_profile(const std::wstring& profile_root)
	{
		try
		{
			if (profile_root.empty())
			{
				return;
			}

			HMODULE our_module = nullptr;
			GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			    reinterpret_cast<LPCWSTR>(&try_update_dll_from_profile),
			    &our_module);
			if (!our_module)
			{
				return;
			}

			wchar_t dll_path_buf[MAX_PATH * 4];
			const auto dll_path_len = GetModuleFileNameW(our_module, dll_path_buf, sizeof(dll_path_buf) / sizeof(dll_path_buf[0]));
			if (dll_path_len == 0)
			{
				return;
			}

			const std::filesystem::path game_dll(std::wstring(dll_path_buf, dll_path_len));
			const auto dll_name    = game_dll.filename();
			const auto game_dir    = game_dll.parent_path();
			const auto profile_dll = std::filesystem::path(profile_root) / dll_name;

			if (!std::filesystem::exists(profile_dll) || !std::filesystem::exists(game_dll))
			{
				return;
			}

			const auto profile_size = std::filesystem::file_size(profile_dll);
			const auto game_size    = std::filesystem::file_size(game_dll);

			if (profile_size == game_size)
			{
				return;
			}

			const auto game_dll_old = game_dir / (dll_name.string() + ".old");

			// Rename the loaded DLL, then copy the new one into place.
			std::error_code ec;
			std::filesystem::remove(game_dll_old, ec);
			std::filesystem::rename(game_dll, game_dll_old, ec);
			if (ec)
			{
				LOG(WARNING) << "[ROM] Failed to rename " << dll_name << " for update: " << ec.message();
				return;
			}

			std::filesystem::copy_file(profile_dll, game_dll, std::filesystem::copy_options::overwrite_existing, ec);
			if (ec)
			{
				LOG(WARNING) << "[ROM] Failed to copy updated " << dll_name << " from profile: " << ec.message();
				std::filesystem::rename(game_dll_old, game_dll);
				return;
			}

			LOG(INFO) << "[ROM] Updated " << dll_name << " from profile"
			          << " (size " << game_size << " -> " << profile_size << ")."
			          << " The update will take effect on next launch.";
		}
		catch (const std::exception& e)
		{
			LOG(WARNING) << "[ROM] DLL update check failed: " << e.what();
		}
	}

	bool is_rom_enabled()
	{
		const store_reason_to_file _store_reason_to_file{};

		constexpr auto rom_enabled_arg_name = L"rom_enabled";
		constexpr auto root_folder_arg_name = L"rom_modding_root_folder";

		const auto rom_enabled_value = _wgetenv(rom_enabled_arg_name);
		if (rom_enabled_value && wcslen(rom_enabled_value))
		{
			if (wcsstr(rom_enabled_value, L"true"))
			{
				LOG(INFO) << "ReturnOfModding enabled because " << big::string_conversions::utf16_to_utf8(rom_enabled_value);

				g_enabled_reason = enabled_reason::ENABLED_BY_ENV_VAR;
				return true;
			}
			else
			{
				LOG(INFO) << "ReturnOfModding disabled because " << big::string_conversions::utf16_to_utf8(rom_enabled_value);

				g_enabled_reason = enabled_reason::DISABLED_BY_ENV_VAR;
				return false;
			}
		}
		else
		{
			try
			{
				int argc  = 0;
				auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
				if (!argv)
				{
					throw std::runtime_error("CommandLineToArgvW failed.");
				}

				bool rom_enabled     = true;
				bool rom_enabled_set = false;

				bool has_rom_enabled_arg_name = false;
				std::wstring rom_enabled_value_str;

				bool has_root_folder_arg_name = false;
				std::wstring root_folder;

				for (int i = 0; i < argc - 1; i++)
				{
					if (wcsstr(argv[i], rom_enabled_arg_name))
					{
						has_rom_enabled_arg_name = true;
						rom_enabled_value_str    = argv[i + 1];
					}
					else if (wcsstr(argv[i], root_folder_arg_name))
					{
						has_root_folder_arg_name = true;
						root_folder              = argv[i + 1];
					}
				}

				if (has_rom_enabled_arg_name || has_root_folder_arg_name)
				{
					rom_enabled_set = true;

					// If launched with command line args, check if the profile
					// has a newer mod loader DLL and update the game copy.
					if (has_root_folder_arg_name && root_folder.size())
					{
						try_update_dll_from_profile(root_folder);
					}

					if (has_rom_enabled_arg_name)
					{
						LOG(INFO) << big::string_conversions::utf16_to_utf8(rom_enabled_value_str);
						if (rom_enabled_value_str.contains(L"true"))
						{
							LOG(INFO) << "ReturnOfModding enabled from command line";
							g_enabled_reason = enabled_reason::ENABLED_BY_CMD_LINE;
						}
						else
						{
							LOG(INFO) << "ReturnOfModding disabled from command line";
							g_enabled_reason = enabled_reason::DISABLED_BY_CMD_LINE;
							rom_enabled      = false;
						}
					}
					else if (has_root_folder_arg_name)
					{
						if (root_folder.size())
						{
							LOG(INFO) << "ReturnOfModding enabled from command line through custom root_folder";
							g_enabled_reason = enabled_reason::ENABLED_BY_CMD_LINE;
						}
					}
				}

				LocalFree(argv);

				if (rom_enabled_set)
				{
					return rom_enabled;
				}
			}
			catch (const std::exception& e)
			{
				LOG(WARNING) << "Failed parsing cmd line args " << e.what();
			}
		}

		if (g_first_enabled_reason == enabled_reason::ENABLED_BY_ENV_VAR || g_first_enabled_reason == enabled_reason::ENABLED_BY_CMD_LINE)
		{
			LOG(INFO)
			    << "ReturnOfModding disabled because first enabled launch was explicit through env var or cmd line";
			g_enabled_reason = enabled_reason::DISABLED_BECAUSE_FIRST_ENABLED_LAUNCH_WAS_EXPLICIT_THROUGH_ENV_VAR_OR_CMD_LINE;
			return false;
		}
		else
		{
			LOG(INFO) << "ReturnOfModding enabled";
			g_enabled_reason = enabled_reason::ENABLED_BY_DEFAULT;
		}

		return true;
	}

	static int32_t g_instance_id = -1;

	int32_t get_instance_id()
	{
		if (g_instance_id != -1)
		{
			return g_instance_id;
		}

		for (int32_t i = 0; i < 100; ++i)
		{
			const std::string mutex_instance_name = std::format("Global\\ReturnOfModdingInstanceID{}", i);
			const auto mutex_handle               = CreateMutexA(NULL, FALSE, mutex_instance_name.c_str());
			if (mutex_handle && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				LOG(DEBUG) << "Returned instance id: " << i;
				g_instance_id = i;
				return i;
			}
		}

		LOG(WARNING) << "Failed getting proper instance id from mutex.";
		g_instance_id = 0;
		return g_instance_id;
	}

	static bool g_init_once_instance_id_string = true;

	std::string& get_instance_id_string()
	{
		static std::string instance_id_str;

		if (g_init_once_instance_id_string)
		{
			const auto instance_id = rom::get_instance_id();
			if (instance_id)
			{
				instance_id_str = std::format("{}", instance_id);
			}
			else
			{
				instance_id_str = "";
			}

			g_init_once_instance_id_string = false;
		}

		return instance_id_str;
	}
} // namespace rom
