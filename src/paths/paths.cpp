#pragma once

#include "paths.hpp"

#include <filesystem>
#include <logger/logger.hpp>
#include <rom/rom.hpp>

namespace big::paths
{
	std::filesystem::path get_main_module_folder()
	{
		constexpr size_t max_str_size = MAX_PATH * 4;
		wchar_t module_file_path[max_str_size];
		const auto path_size              = GetModuleFileNameW(nullptr, module_file_path, max_str_size);
		std::filesystem::path module_path = std::wstring(module_file_path, path_size);
		return module_path.parent_path();
	}

	std::filesystem::path get_project_root_folder()
	{
		std::filesystem::path root_folder{};

		constexpr auto root_folder_arg_name = L"rom_modding_root_folder";
		constexpr auto folder_name          = "ReturnOfModding";

		const auto env_root_folder = _wgetenv(root_folder_arg_name);
		if (env_root_folder)
		{
			root_folder  = env_root_folder;
			root_folder /= folder_name;
			LOG(INFO) << "Root folder set through env variable: "
			          << reinterpret_cast<const char*>(root_folder.u8string().c_str());
			if (!std::filesystem::exists(root_folder))
			{
				std::filesystem::create_directories(root_folder);
			}
		}
		else
		{
			try
			{
				auto* args  = GetCommandLineW();
				int argc    = 0;
				auto** argv = CommandLineToArgvW(args, &argc);

				if (!argv)
				{
					throw std::runtime_error("CommandLineToArgvW failed.");
				}

				auto options = rom::get_rom_cxx_options();

				const auto result = options.parse(argc, argv);

				if (result.count(root_folder_arg_name))
				{
					root_folder  = result[root_folder_arg_name].as<std::wstring>();
					root_folder /= folder_name;
					LOG(INFO) << "Root folder set through command line args: "
					          << reinterpret_cast<const char*>(root_folder.u8string().c_str());
					if (!std::filesystem::exists(root_folder))
					{
						std::filesystem::create_directories(root_folder);
					}
				}

				LocalFree(argv);
			}
			catch (const std::exception& e)
			{
				LOG(WARNING) << "Failed parsing cmd line args. Reason: " << e.what();
			}
		}

		if (root_folder.empty() || !std::filesystem::exists(root_folder))
		{
			root_folder = get_main_module_folder() / folder_name;
			LOG(INFO) << "Root folder set through default (game folder): "
			          << reinterpret_cast<const char*>(root_folder.u8string().c_str());
			if (!std::filesystem::exists(root_folder))
			{
				std::filesystem::create_directories(root_folder);
			}
		}

		return root_folder;
	}

	static std::filesystem::path dump_file_path;

	void init_dump_file_path()
	{
		dump_file_path = g_file_manager
		                     .get_project_file(std::format("{}_crash{}.dmp", rom::g_project_name, rom::get_instance_id_string()))
		                     .get_path();

		try
		{
			if (std::filesystem::exists(dump_file_path))
			{
				std::filesystem::remove(dump_file_path);
			}
		}
		catch (const std::exception& e)
		{
			LOG(ERROR) << e.what();
		}
	}

	const std::filesystem::path& remove_and_get_dump_file_path()
	{
		try
		{
			if (std::filesystem::exists(dump_file_path))
			{
				std::filesystem::remove(dump_file_path);
			}
		}
		catch (const std::exception& e)
		{
			LOG(ERROR) << e.what();
		}

		return dump_file_path;
	}
} // namespace big::paths
