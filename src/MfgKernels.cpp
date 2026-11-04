#include "MfgKernels.h"
#include "Log.h"
#include "Util.h"
#include "MfgPort/ModelPort.h"
#include "MfgPort/RuntimePayload.h"
#include "MfgPort/RuntimeLayout.h"
#include "MfgPort/Sha256.h"
#include "MfgPort/ShaderPort.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace
{
	// The 16 reconstruction program pairs the ported model needs from the payload (MFG-Unlocker, xess_mod.cpp)
	constexpr const char* ReconstructionPrograms[] = {
		"3518243144275597942266", "3518243145375109570477", "3519166589661394643131", "3519166588561883014920",
		"3515873413746975655382", "3515873414846487283593", "35115408591383412559519", "35115408590283900931308",
		"35118300971948070613363", "35118300970848558985152", "351747859676298538780", "351747860775810166991",
		"3517111067725881270903", "3517111066626369642692", "35110677435103722760616", "35110677436203234388827",
	};

	// The port patches the library's code and tables process wide, so it lives as long as the process does
	struct State
	{
		std::once_flag once;
		bool active = false;
		std::unique_ptr<ShaderPort> shaders;
		std::unique_ptr<ModelPort> model;
	};

	State& Get()
	{
		static auto* state = new State;
		return *state;
	}

	// The PE header identifies the build; it stays behind when a dll is patched, so the file hash is logged too
	std::pair<uint32_t, uint32_t> ImageIdentity(HMODULE module) noexcept
	{
		const auto* base = reinterpret_cast<const unsigned char*>(module);
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);

		if (!base || dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000)
			return {};

		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);

		if (nt->Signature != IMAGE_NT_SIGNATURE)
			return {};

		return { static_cast<uint32_t>(nt->FileHeader.TimeDateStamp),
			static_cast<uint32_t>(nt->OptionalHeader.SizeOfImage) };
	}

	std::string FileHash(const std::filesystem::path& path) noexcept
	{
		try
		{
			std::ifstream file(path, std::ios::binary);

			if (!file)
				return {};

			sha256_util::Hasher hasher;
			std::array<char, 1 << 20> buffer;

			while (file)
			{
				file.read(buffer.data(), buffer.size());

				if (file.gcount() > 0 && !hasher.Update(buffer.data(), static_cast<size_t>(file.gcount())))
					return {};
			}

			sha256_util::Digest digest{};

			return hasher.Finish(digest) ? sha256_util::LowerHex(digest) : std::string{};
		}
		catch (...)
		{
			return {};
		}
	}

	// next to the dll, either laid out flat or as MFG-Unlocker's build input has it
	std::filesystem::path FindPrograms()
	{
		auto root = ThisModulePath().parent_path() / L"xefg_mfg_programs";

		if (!std::filesystem::exists(root / "programs.tsv") && std::filesystem::exists(root / "programs" / "programs.tsv"))
			root /= "programs";

		return root;
	}

	void TryInstall(HMODULE module, State& state)
	{
		try
		{
			const auto root = FindPrograms();

			if (!std::filesystem::exists(root / "programs.tsv"))
			{
				Log::Info("No ported XMX MFG program set in {}, XeFG keeps its DP4a model for every generated frame",
					root.string());
				return;
			}

			// Decoder first: the model port makes XeFG ask for the ported programs right afterwards
			auto shaders = std::make_unique<ShaderPort>(module, root);

			for (const char* key : ReconstructionPrograms)
			{
				if (!shaders->hasProgram(key))
					throw std::runtime_error(std::string("missing reconstruction program ") + key);
			}

			auto model = std::make_unique<ModelPort>(module, true, true);

			state.shaders = std::move(shaders);
			state.model = std::move(model);
			state.active = true;

			Log::Info("Ported XMX MFG model installed, one optical flow field per generated batch");
		}
		catch (const std::exception& error)
		{
			const auto path = ModulePath(module);
			const auto [timeDateStamp, sizeOfImage] = ImageIdentity(module);
			const auto hash = FileHash(path);

			Log::Warn("Ported XMX MFG model not installed ({}), XeFG keeps its DP4a model; module {} build {:08X}, image {:X}{}",
				error.what(), path.string(), timeDateStamp, sizeOfImage,
				hash.empty() ? std::string{} : ", file sha256 " + hash);
		}
	}
} // namespace

bool MfgKernels::Install(HMODULE module)
{
	auto& state = Get();

	std::call_once(state.once, [module, &state] { TryInstall(module, state); });

	return state.active;
}

std::filesystem::path MfgKernels::ProgramsDirectory()
{
	return FindPrograms();
}
