#include <fstream>
#include <filesystem>
#include <array>
#include <vector>
#include <string>
#include <optional>
#include <iostream>
#include <set>
#include <map>
#include <codecvt>
#include <sstream>

#include <windows.h>

#include <unicode/uchar.h>

struct FText
{
	std::wstring ns;
	std::wstring key;
	std::wstring s;
};

inline bool test_signature(std::string_view const& signature, std::vector<char> const& buffer, size_t index)
{
	return std::string_view(buffer.data() + index, buffer.size() - index).starts_with(signature);
}

bool good_ch(wchar_t ch)
{
	if (U_MASK(u_charType(ch)) & (~U_GC_C_MASK | U_GC_CF_MASK | U_GC_CS_MASK)) // all categories but controls + format controls and surrogate controls
		return true;
	if (0x09 <= ch && ch <= 0x0d || 0x1c <= ch && ch <= 0x1f) // some ISO format controls
		return true;
	return false;
}

bool very_good_key(std::wstring const& key)
{
	if (key.size() != 32)
		return false;
	for (const auto c : key)
		if (!(L'0' <= c && c <= L'9' || L'A' <= c && c <= L'F'))
			return false;
	return true;
}

bool all_white_spaces(std::wstring const& s)
{
	for (const auto c : s)
		if (!u_isspace(c))
			return false;
	return true;
}

bool has_letter(std::wstring const& s)
{
	for (const auto c : s)
		if (u_isalpha(c))
			return true;
	return false;
}

std::optional<std::pair<FText, size_t>> try_read_blueprint_text(std::vector<char> const& buffer, size_t index)
{
	constexpr std::string_view BLUEPRINT_TEXT_SIGNATURE = "\x29\x01"; // EX_TextConst, EBlueprintTextLiteralType::LocalizedText

	if (!test_signature(BLUEPRINT_TEXT_SIGNATURE, buffer, index))
		return std::nullopt;

	index += BLUEPRINT_TEXT_SIGNATURE.size();

	const auto read_to_null = [&] () -> std::optional<std::wstring> {
		if (buffer.size() <= index)
			return std::nullopt;

		if (buffer[index] == 0x1F) // ANSI (EX_StringConst)
		{
			std::wstring s;
			for (++index; index < buffer.size(); ++index)
			{
				const auto ch = buffer[index];
				if (ch == 0)
				{
					++index;
					return s;
				}
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			return std::nullopt;
		}

		if (buffer[index] == 0x34) // UTF-16 (EX_UnicodeStringConst)
		{
			std::wstring s;
			for (++index; index < buffer.size(); index += 2)
			{
				if (buffer.size() <= index + 1)
					return std::nullopt;
				const auto ch = *reinterpret_cast<const wchar_t*>(buffer.data() + index);
				if (ch == 0)
				{
					index += 2;
					return s;
				}
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			return std::nullopt;
		}

		return std::nullopt;
	};

	const auto s = read_to_null();
	if (!s.has_value())
		return std::nullopt;
	if (s->size() == 0)
		return std::nullopt;
	const auto key = read_to_null();
	if (!key.has_value())
		return std::nullopt;
	if (key->size() == 0)
		return std::nullopt;
	if (128 < key->size())   // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	if (all_white_spaces(s.value()))
		return std::nullopt;
	const auto ns = read_to_null();
	if (!ns.has_value())
		return std::nullopt;
	if (128 < ns->size())    // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	int good_score = 0;
	if (very_good_key(key.value()))
		good_score += 10;
	if (has_letter(s.value()))
		good_score += 5;
	if (good_score < 5)
		return std::nullopt;

	return std::pair{ FText{ ns.value(), key.value(), s.value() }, index };
}

std::optional<std::pair<FText, size_t>> try_read_ftext(std::vector<char> const& buffer, size_t index)
{
	if (buffer.size() < index + 5)
		return std::nullopt;

	const auto flag = *reinterpret_cast<const int*>(buffer.data() + index);
	index += 4;

	if (0b00011111 < flag) // highest flag right now: InitializedFromString = (1<<4)
		return std::nullopt;

	if (flag & 0b00000100) // ConvertedProperty = (1 << 2) never set in cooked text
		return std::nullopt;

	if (flag & 0b00010000) // InitializedFromString = (1 << 4) never set in cooked text
		return std::nullopt;

	// Strange, but some probably localizable text using this flag
	//if (flag & 0b00000010) // ShouldGatherForLocalization: no CultureInvariant
	//	return std::nullopt;

	if (flag & 0b00000001) // ShouldGatherForLocalization: no Transient
		return std::nullopt;

	const auto history = *reinterpret_cast<const char*>(buffer.data() + index);
	index += 1;

	if (history != 0) // support only ETextHistoryType::Base right now, should we support None = -1?
		return std::nullopt;

	const auto read_string = [&] () -> std::optional<std::wstring> {
		if (buffer.size() < index + 4)
			return std::nullopt;
		auto length = static_cast<int64_t>(*reinterpret_cast<const int*>(buffer.data() + index));
		index += 4;
		if (length == 0)
			return L"";
		if (length < 0)
		{
			length = -length;
			if (buffer.size() < index + 2 * length)
				return std::nullopt;
			if (buffer[index + 2 * length - 2] != 0)
				return std::nullopt;
			if (buffer[index + 2 * length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + 2 * length - 2; i += 2)
			{
				const auto ch = *reinterpret_cast<const wchar_t*>(buffer.data() + i);
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length * 2;
			return s;
		}
		else
		{
			if (buffer.size() < index + length)
				return std::nullopt;
			if (buffer[index + length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + length - 1; ++i)
			{
				const auto ch = buffer[i];
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length;
			return s;
		}
	};

	const auto ns = read_string();
	if (!ns.has_value())
		return std::nullopt;
	if (128 < ns->size())    // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	const auto key = read_string();
	if (!key.has_value())
		return std::nullopt;
	if (key->size() == 0)
		return std::nullopt;
	if (128 < key->size())   // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	const auto s = read_string();
	if (!s.has_value())
		return std::nullopt;
	if (s->size() == 0)
		return std::nullopt;
	if (all_white_spaces(s.value()))
		return std::nullopt;

	int good_score = 0;
	if (very_good_key(key.value()))
		good_score += 10;
	if (has_letter(s.value()))
		good_score += 5;

	const auto current_index = index;
	if (good_score < 10)
	{
		const auto impostor_check = read_string();
		if (impostor_check.has_value() && 0 < impostor_check->size())
			good_score -= 5;
	}

	if (good_score < 5)
		return std::nullopt;

	return std::pair{ FText{ ns.value(), key.value(), s.value() }, current_index };
}

std::optional<std::pair<FText, size_t>> try_read_very_good_raw_text(std::vector<char> const& buffer, size_t index)
{
	const auto read_string = [&] () -> std::optional<std::wstring> {
		if (buffer.size() < index + 4)
			return std::nullopt;
		auto length = static_cast<int64_t>(*reinterpret_cast<const int*>(buffer.data() + index));
		index += 4;
		if (length == 0)
			return L"";
		if (length < 0)
		{
			length = -length;
			if (buffer.size() < index + 2 * length)
				return std::nullopt;
			if (buffer[index + 2 * length - 2] != 0)
				return std::nullopt;
			if (buffer[index + 2 * length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + 2 * length - 2; i += 2)
			{
				const auto ch = *reinterpret_cast<const wchar_t*>(buffer.data() + i);
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length * 2;
			return s;
		}
		else
		{
			if (buffer.size() < index + length)
				return std::nullopt;
			if (buffer[index + length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + length - 1; ++i)
			{
				const auto ch = buffer[i];
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length;
			return s;
		}
	};

	const auto ns = read_string();
	if (!ns.has_value())
		return std::nullopt;
	if (128 < ns->size())    // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	if (ns->size() != 0)
		return std::nullopt; // only empty namespaces supported!
	const auto key = read_string();
	if (!key.has_value())
		return std::nullopt;
	if (key->size() == 0)
		return std::nullopt;
	if (128 < key->size())   // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
	if (!very_good_key(key.value()))
		return std::nullopt; // only very good keys supported!
	const auto s = read_string();
	if (!s.has_value())
		return std::nullopt;
	if (s->size() == 0)
		return std::nullopt;
	if (all_white_spaces(s.value()))
		return std::nullopt;
	return std::pair{ FText{ ns.value(), key.value(), s.value() }, index };
}

std::optional<std::pair<std::vector<FText>, size_t>> try_read_string_table(std::vector<char> const& buffer, size_t index)
{
	if (buffer.size() < index + 12)
		return std::nullopt;

	const auto read_string = [&] () -> std::optional<std::wstring> {
		if (buffer.size() < index + 4)
			return std::nullopt;
		auto length = static_cast<int64_t>(*reinterpret_cast<const int*>(buffer.data() + index));
		index += 4;
		if (length == 0)
			return L"";
		if (length < 0)
		{
			length = -length;
			if (buffer.size() < index + 2 * length)
				return std::nullopt;
			if (buffer[index + 2 * length - 2] != 0)
				return std::nullopt;
			if (buffer[index + 2 * length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + 2 * length - 2; i += 2)
			{
				const auto ch = *reinterpret_cast<const wchar_t*>(buffer.data() + i);
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length * 2;
			return s;
		}
		else
		{
			if (buffer.size() < index + length)
				return std::nullopt;
			if (buffer[index + length - 1] != 0)
				return std::nullopt;
			std::wstring s;
			for (size_t i = index; i < index + length - 1; ++i)
			{
				const auto ch = buffer[i];
				if (ch == 0)
					return std::nullopt;
				if (!good_ch(ch))
					return std::nullopt;
				s += ch;
			}
			index += length;
			return s;
		}
	};

	const auto ns = read_string();
	if (!ns.has_value())
		return std::nullopt;
	if (128 < ns->size())    // static const int32 InlineStringSize = 128;
		return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);

	if (buffer.size() < index + 8)
		return std::nullopt;

	const auto size = *reinterpret_cast<const int*>(buffer.data() + index);
	index += 4;
	if (size < 1)
		return std::nullopt;

	std::vector<FText> table;

	// size maybe very large
	//table.reserve(size);

	int good_score = 0;

	for (size_t i = 0; i < size; ++i)
	{
		const auto key = read_string();
		if (!key.has_value())
			return std::nullopt;
		if (key->size() == 0)
			return std::nullopt;
		if (128 < key->size())   // static const int32 InlineStringSize = 128;
			return std::nullopt; // UE_CLOG(SaveNum > InlineStringSize, LogTextKey, VeryVerbose, TEXT("Key string '%s' was larger (%d) than the inline size (%d) and caused an allocation!"), OutStrBuffer.GetData(), SaveNum, InlineStringSize);
		const auto s = read_string();
		if (!s.has_value())
			return std::nullopt;
		if (s->size() == 0)
		{
			good_score -= 2;
			continue;
		}
		if (all_white_spaces(s.value()))
		{
			good_score -= 1;
			continue;
		}
		table.push_back(FText{ ns.value(), key.value(), s.value() });
		good_score += 2;
	}

	if (good_score < 0)
		return std::nullopt;

	if (buffer.size() < index + 4)
		return std::nullopt;

	const auto metadata_size = *reinterpret_cast<const int*>(buffer.data() + index);
	index += 4;
	if (metadata_size < 0)
		return std::nullopt;

	return std::pair{ std::move(table), index };
}

void file_extract(std::filesystem::path root, std::filesystem::path file, std::vector<std::string> const& raw_text_signatures, bool all_uexps, std::vector<FText> & texts)
{
	if (!(file.extension() == L".uasset" || file.extension() == L".umap" || all_uexps && file.extension() == L".uexp"))
		return;

	const auto replace_extension = [&] (std::filesystem::path const& ext) {
		auto copy = file;
		return copy.replace_extension(ext);
	};

	if (file.extension() == L".uexp")
	{
		if (std::filesystem::exists(replace_extension(L".uasset")) || std::filesystem::exists(replace_extension(L".umap")))
			return;
	}

	std::wcout << std::filesystem::relative(file, root) << std::endl;

	auto fin = std::ifstream{ file, std::ios::binary | std::ios::ate };
	if (fin.fail())
		return;

	auto buffer = std::vector<char>(fin.tellg());
	fin.seekg(0, std::ios::beg);
	fin.read(buffer.data(), buffer.size());

	bool has_blueprint = false;
	bool has_text_property = false;
	bool has_string_table = false;
	bool has_very_good_raw_text = false;

	if (file.extension() == L".uasset" || file.extension() == L".umap")
	{
		constexpr std::string_view BLUEPRINT_SIGNATURE = "BlueprintGeneratedClass";
		constexpr std::string_view TEXT_PROPERTY_SIGNATURE = "TextProperty";
		constexpr std::string_view STRING_TABLE_SIGNATURE = "StringTable";

		for (size_t i = 0; i < buffer.size(); ++i)
		{
			if (!has_blueprint && test_signature(BLUEPRINT_SIGNATURE, buffer, i))
				has_blueprint = true;
			if (!has_text_property && test_signature(TEXT_PROPERTY_SIGNATURE, buffer, i))
				has_text_property = true;
			if (!has_string_table && test_signature(STRING_TABLE_SIGNATURE, buffer, i))
				has_string_table = true;
			if (0 < raw_text_signatures.size() && !has_very_good_raw_text)
			{
				for (auto const& raw_text_signature : raw_text_signatures)
					if (test_signature(raw_text_signature, buffer, i))
					{
						has_very_good_raw_text = true;
						break;
					}
			}
			if (has_blueprint && has_text_property && has_string_table && (raw_text_signatures.size() == 0 || has_very_good_raw_text))
				break;
		}

		if (!(has_blueprint || has_text_property || has_string_table || has_very_good_raw_text))
			return;

		if (const auto uexp_file = replace_extension(L".uexp"); std::filesystem::exists(uexp_file))
		{
			fin = std::ifstream{ uexp_file, std::ios::binary | std::ios::ate };
			if (fin.fail())
				return;
			buffer = std::vector<char>(fin.tellg());
			fin.seekg(0, std::ios::beg);
			fin.read(buffer.data(), buffer.size());
		}
	}
	else
	{
		has_blueprint = true;
		has_text_property = true;
		has_string_table = true;
		if (0 < raw_text_signatures.size())
			has_very_good_raw_text = true;
	}

	for (size_t i = 0; i < buffer.size(); ++i)
	{
		if (has_blueprint)
		{
			if (const auto text = try_read_blueprint_text(buffer, i); text.has_value())
			{
				texts.push_back(text.value().first);
				i = text.value().second - 1;
				continue;
			}
		}
		if (has_text_property)
		{
			if (const auto text = try_read_ftext(buffer, i); text.has_value())
			{
				texts.push_back(text.value().first);
				i = text.value().second - 1;
				continue;
			}
		}
		if (has_string_table)
		{
			if (const auto table = try_read_string_table(buffer, i); table.has_value())
			{
				for (auto const& text : table.value().first)
					texts.push_back(text);
				i = table.value().second - 1;
				continue;
			}
		}
		if (has_very_good_raw_text)
		{
			if (const auto text = try_read_very_good_raw_text(buffer, i); text.has_value())
			{
				texts.push_back(text.value().first);
				i = text.value().second - 1;
				continue;
			}
		}
	}
}

void directory_extract(std::filesystem::path root, std::filesystem::path directory, std::vector<std::string> const& raw_text_signatures, bool all_uexps, std::vector<FText> & texts)
{
	for (auto const& entry : std::filesystem::directory_iterator(directory))
	{
		if (entry.is_directory())
			directory_extract(root, entry, raw_text_signatures, all_uexps, texts);
		else
			file_extract(root, entry, raw_text_signatures, all_uexps, texts);
	}
}

namespace crc32
{
	constexpr auto CRCTablesSB8 = std::array<unsigned int, 256>{
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	};

	unsigned int StrCrc32_Unicode(std::wstring string)
	{
		std::vector<unsigned int> buf;
		for (size_t i = 0; i < string.size(); ++i)
		{
			buf.push_back(string[i] & 0xFF);
			buf.push_back((string[i] & 0xFF00) >> 8);
		}
		auto CRC = 0xFFFFFFFF;
		for (size_t i = 0; i < buf.size() / 2; ++i)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[(CRC ^ buf[i * 2 + 0]) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[(CRC ^ buf[i * 2 + 1]) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[CRC & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[CRC & 0xFF];
		}
		return CRC ^ 0xFFFFFFFF;
	}

	unsigned int StrCrc32_ASCII(std::wstring string)
	{
		std::vector<unsigned int> buf;
		for (size_t i = 0; i < string.size(); ++i)
			buf.push_back(string[i]);
		auto CRC = 0xFFFFFFFF;
		for (size_t i = 0; i < buf.size(); ++i)
		{
			CRC = (CRC >> 8) ^ CRCTablesSB8[(CRC ^ buf[i]) & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[CRC & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[CRC & 0xFF];
			CRC = (CRC >> 8) ^ CRCTablesSB8[CRC & 0xFF];
		}
		return CRC ^ 0xFFFFFFFF;
	}

	unsigned int StrCrc32(std::wstring string)
	{
		bool bNeedUnicode = false;
		for (size_t i = 0; i < string.size(); ++i)
		{
			if (!(0x0000 <= string[i] && string[i] <= 0x00FF))
			{
				bNeedUnicode = true;
				break;
			}
		}
		if (bNeedUnicode)
			return StrCrc32_Unicode(string);
		else
			return StrCrc32_ASCII(string);
	}
}

void print_help()
{
	std::wcout
		<< L"Extract localizable texts to locres or txt file:" << std::endl
		<< L"UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.locres file> [-old] [-raw-text-signatures=<signature1>,<signature2>,...] [-all-uexps]" << std::endl
		<< L"UE4TextExtractor.exe <path to folder with extracted from pak files> <path to texts.txt file> [-raw-text-signatures=<signature1>,<signature2>,...] [-all-uexps]" << std::endl
		<< LR"(Example: UE4TextExtractor.exe "C:\MyGame\Content\Paks\unpacked" "C:\MyGame\Content\Paks\texts.locres")" << std::endl
		<< std::endl

		<< L"Use -raw-text-signatures=<signature1>,<signature2>,... modifier for parsing localizable text by custom signatures. See also: https://github.com/VD42/UE4TextExtractor/blob/master/RAW_TEXT_SIGNATURES.md." << std::endl
		<< L"Use -all-uexps modifier for additionaly parsing uexp files without matching uasset or umap files." << std::endl
		<< std::endl

		<< L"Convert locres to txt or backward:" << std::endl
		<< L"UE4TextExtractor.exe <path to texts.txt file> <path to texts.locres file> [-old]" << std::endl
		<< L"UE4TextExtractor.exe <path to texts.locres file> <path to texts.txt file>" << std::endl
		<< LR"(Example: UE4TextExtractor.exe "C:\MyGame\Content\Paks\texts.txt" "C:\MyGame\Content\Paks\texts.locres")" << std::endl
		<< std::endl

		<< L"Use -old modifier for old-version locres file generation." << std::endl
	;
}

std::wstring replace_all(std::wstring s, std::wstring const& from, std::wstring const& to)
{
	size_t pos = 0;
	while ((pos = s.find(from, pos)) != std::wstring::npos)
	{
		s.replace(pos, from.length(), to);
		pos += from.length();
	}
	return s;
}

std::wstring escape_key(std::wstring key)
{
	key = replace_all(key, L"\r", L"&#x000013;");
	key = replace_all(key, L"\n", L"&#x000010;");
	key = replace_all(key, L"[", L"&#x000091;");
	key = replace_all(key, L"]", L"&#x000093;");
	key = replace_all(key, L"{", L"&#x000123;");
	key = replace_all(key, L"}", L"&#x000125;");
	return key;
}

std::wstring unescape_key(std::wstring key)
{
	key = replace_all(key, L"&#x000013;", L"\r");
	key = replace_all(key, L"&#x000010;", L"\n");
	key = replace_all(key, L"&#x000091;", L"[");
	key = replace_all(key, L"&#x000093;", L"]");
	key = replace_all(key, L"&#x000123;", L"{");
	key = replace_all(key, L"&#x000125;", L"}");
	return key;
}

struct FEntry
{
	std::wstring key;
	uint32_t hash;
	std::wstring s;
};

using locres_vector = std::vector<std::pair<std::wstring, std::vector<FEntry>>>;

static const auto magic = std::vector<unsigned char>{
	0x0E, 0x14, 0x74, 0x75, 0x67, 0x4A, 0x03, 0xFC, 0x4A, 0x15, 0x90, 0x9D, 0xC3, 0x37, 0x7F, 0x1B
};

void write_to_txt_file(locres_vector const& lv, std::filesystem::path file)
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	auto fout = std::ofstream{ file, std::ios::binary | std::ios::out };
	for (auto const& ns : lv)
	{
		const auto escaped_ns = converter.to_bytes(escape_key(ns.first));
		fout << "=>{" << escaped_ns << "}" << '\r' << '\n' << '\r' << '\n';
		for (auto const& text : ns.second)
		{
			const auto escaped_key = converter.to_bytes(escape_key(text.key));
			const auto s = converter.to_bytes(text.s);
			fout << "=>[" << escaped_key << "][" << text.hash << "]" << '\r' << '\n' << s << '\r' << '\n' << '\r' << '\n';
		}
	}
	fout << "=>{[END]}" << '\r' << '\n' << std::flush;
}

void write_to_locres_file(bool old, locres_vector const& lv, std::filesystem::path file)
{
	auto fout = std::ofstream{ file, std::ios::binary | std::ios::out };

	std::streampos strings_array_offset_placeholder_offset;

	if (!old)
	{
		fout.write(reinterpret_cast<const char*>(magic.data()), magic.size());

		const uint8_t version = 1;
		fout.write(reinterpret_cast<const char*>(&version), sizeof(uint8_t));

		const int64_t strings_array_offset_placeholder = 0;
		strings_array_offset_placeholder_offset = fout.tellp();
		fout.write(reinterpret_cast<const char*>(&strings_array_offset_placeholder), sizeof(int64_t));
	}

	const uint32_t namespace_count = static_cast<const uint32_t>(lv.size());
	fout.write(reinterpret_cast<const char*>(&namespace_count), sizeof(uint32_t));

	std::vector<std::wstring> strings;
	std::map<std::wstring, int32_t> strings_map;

	const auto write_string = [&] (std::wstring s) {
		if (s.length() == 0)
		{
			const int32_t length = 0;
			fout.write(reinterpret_cast<const char*>(&length), sizeof(int32_t));
			return;
		}
		bool need_unicode = false;
		for (auto const& c : s)
			if (!(0x00 <= c && c <= 0x7F))
			{
				need_unicode = true;
				break;
			}
		if (need_unicode)
		{
			const int32_t length = -static_cast<int32_t>(s.length()) - 1;
			fout.write(reinterpret_cast<const char*>(&length), sizeof(int32_t));
			fout.write(reinterpret_cast<const char*>(s.c_str()), s.length() * 2);
			const uint16_t zero = 0;
			fout.write(reinterpret_cast<const char*>(&zero), sizeof(uint16_t));
		}
		else
		{
			const int32_t length = static_cast<int32_t>(s.length()) + 1;
			fout.write(reinterpret_cast<const char*>(&length), sizeof(int32_t));
			for (auto const& c : s)
				fout.write(reinterpret_cast<const char*>(&c), sizeof(char));
			const uint8_t zero = 0;
			fout.write(reinterpret_cast<const char*>(&zero), sizeof(uint8_t));
		}
	};

	for (auto const& ns : lv)
	{
		write_string(ns.first);
		const uint32_t key_count = static_cast<const uint32_t>(ns.second.size());
		fout.write(reinterpret_cast<const char*>(&key_count), sizeof(uint32_t));
		for (auto const& text : ns.second)
		{
			write_string(text.key);
			fout.write(reinterpret_cast<const char*>(&text.hash), sizeof(uint32_t));
			if (!old)
			{
				int32_t index = 0;
				if (const auto it = strings_map.find(text.s); it == strings_map.end())
				{
					index = static_cast<int32_t>(strings.size());
					strings_map.emplace(text.s, index);
					strings.push_back(text.s);
				}
				else
				{
					index = it->second;
				}
				fout.write(reinterpret_cast<const char*>(&index), sizeof(int32_t));
			}
			else
			{
				write_string(text.s);
			}
		}
	}

	if (!old)
	{
		const int64_t strings_array_offset = fout.tellp();
		const uint32_t strings_array_count = static_cast<uint32_t>(strings.size());
		fout.write(reinterpret_cast<const char*>(&strings_array_count), sizeof(uint32_t));
		for (auto const& s : strings)
			write_string(s);
		fout.seekp(strings_array_offset_placeholder_offset);
		fout.write(reinterpret_cast<const char*>(&strings_array_offset), sizeof(int64_t));
	}
}

int wmain(int argc, wchar_t ** argv)
{
	std::locale::global(std::locale{ std::locale::classic(), "en_US.UTF-8", std::locale::ctype });

	SetConsoleOutputCP(CP_UTF8);
	SetConsoleCP(CP_UTF8);

	std::vector<std::wstring_view> args;
	for (size_t i = 0; i < argc; ++i)
		args.emplace_back(argv[i]);

	if (args.size() < 3)
	{
		print_help();
		return 1;
	}

	constexpr std::wstring_view old_argument = L"-old";
	constexpr std::wstring_view raw_text_signatures_argument = L"-raw-text-signatures=";
	constexpr std::wstring_view all_uexps_argument = L"-all-uexps";

	const auto path_left = std::filesystem::path(args[1]);
	const auto path_right = std::filesystem::path(args[2]);
	bool old = false;
	bool all_uexps = false;
	std::vector<std::string> raw_text_signatures;

	for (size_t i = 3; i < args.size(); ++i)
	{
		if (args[i] == old_argument)
		{
			old = true;
			continue;
		}
		if (args[i] == all_uexps_argument)
		{
			all_uexps = true;
			continue;
		}
		if (args[i].starts_with(raw_text_signatures_argument))
		{
			const auto wtos = [] (std::wstring_view const& s) {
				std::string result;
				for (auto c : s)
					result += static_cast<char>(c);
				return result;
			};
			auto raw_text_signatures_value = args[i];
			raw_text_signatures_value.remove_prefix(raw_text_signatures_argument.size());
			size_t pos = -1;
			while ((pos = raw_text_signatures_value.find(L",")) != std::wstring_view::npos)
			{
				raw_text_signatures.push_back(wtos(raw_text_signatures_value.substr(0, pos)));
				raw_text_signatures_value.remove_prefix(pos + 1);
			}
			if (0 < raw_text_signatures_value.size())
				raw_text_signatures.push_back(wtos(raw_text_signatures_value));
			continue;
		}
	}

	if (std::filesystem::is_directory(path_left))
	{
		std::vector<FText> texts;
		directory_extract(path_left, path_left, raw_text_signatures, all_uexps, texts);
		std::set<std::wstring> namespaces;
		for (auto const& text : texts)
			namespaces.insert(text.ns);
		locres_vector lv;
		lv.reserve(namespaces.size());
		for (auto const& ns : namespaces)
		{
			lv.emplace_back();
			lv.back().first = ns;
			std::set<std::wstring> unique_check;
			for (auto const& text : texts)
			{
				if (text.ns != ns)
					continue;
				if (unique_check.find(text.key) != unique_check.end())
					continue;
				unique_check.insert(text.key);
				lv.back().second.push_back(FEntry{ text.key, crc32::StrCrc32(text.s), text.s });
			}
		}

		if (path_right.extension() == L".txt")
		{
			write_to_txt_file(lv, path_right);
			return 0;
		}
		else if (path_right.extension() == L".locres")
		{
			write_to_locres_file(old, lv, path_right);
			return 0;
		}
		else
		{
			print_help();
			return 1;
		}
	}
	else if (path_left.extension() == L".locres" && path_right.extension() == L".txt")
	{
		auto fin = std::ifstream{ path_left, std::ios::binary | std::ios::ate };
		auto buffer = std::vector<char>(fin.tellg());
		fin.seekg(0, std::ios::beg);
		fin.read(buffer.data(), buffer.size());

		uint8_t version = 0;
		size_t index = 0;

		if (magic.size() <= buffer.size())
		{
			bool found = true;
			for (size_t i = 0; i < magic.size(); ++i)
				if (static_cast<unsigned char>(buffer[i]) != magic[i])
				{
					found = false;
					break;
				}
			if (found)
			{
				index += magic.size();
				version = *reinterpret_cast<const uint8_t*>(buffer.data() + index);
				index += sizeof(uint8_t);
			}
		}

		if (!(0 <= version && version <= 3))
		{
			std::wcout << L"ERROR: LocRes format too new!";
			return 1;
		}

		const auto read_string = [&] () -> std::wstring {
			auto length = static_cast<int64_t>(*reinterpret_cast<const int*>(buffer.data() + index));
			index += 4;
			if (length == 0)
				return L"";
			if (length < 0)
			{
				length = -length;
				std::wstring s;
				for (size_t i = index; i < index + 2 * length - 2; i += 2)
				{
					const auto ch = *reinterpret_cast<const wchar_t*>(buffer.data() + i);
					s += ch;
				}
				index += length * 2;
				return s;
			}
			else
			{
				std::wstring s;
				for (size_t i = index; i < index + length - 1; ++i)
				{
					const auto ch = buffer[i];
					s += ch;
				}
				index += length;
				return s;
			}
		};

		std::vector<std::wstring> strings;

		if (1 <= version)
		{
			const auto strings_array_offset = *reinterpret_cast<const int64_t*>(buffer.data() + index);
			index += sizeof(int64_t);

			const auto restore_index_point = index;

			index = strings_array_offset;

			const auto strings_array_count = *reinterpret_cast<const uint32_t*>(buffer.data() + index);
			index += sizeof(uint32_t);

			strings.reserve(strings_array_count);

			for (size_t i = 0; i < strings_array_count; ++i)
			{
				strings.push_back(read_string());
				if (2 <= version)
					index += sizeof(int32_t);
			}

			index = restore_index_point;
		}

		if (2 <= version)
			index += sizeof(uint32_t);

		const auto namespace_count = *reinterpret_cast<const uint32_t*>(buffer.data() + index);
		index += sizeof(uint32_t);

		locres_vector lv;
		lv.reserve(namespace_count);

		for (size_t i = 0; i < namespace_count; ++i)
		{
			if (version == 2 || version == 3)
				index += sizeof(uint32_t);
			const auto ns = read_string();

			const auto key_count = *reinterpret_cast<const uint32_t*>(buffer.data() + index);
			index += sizeof(uint32_t);

			lv.emplace_back();
			lv.back().first = ns;
			lv.back().second.reserve(key_count);

			for (size_t j = 0; j < key_count; ++j)
			{
				if (version == 2 || version == 3)
					index += sizeof(uint32_t);

				const auto key = read_string();

				const auto hash = *reinterpret_cast<const uint32_t*>(buffer.data() + index);
				index += sizeof(uint32_t);

				std::wstring str;
				if (1 <= version)
				{
					const auto str_index = *reinterpret_cast<const int32_t*>(buffer.data() + index);
					index += sizeof(int32_t);
					str = strings[str_index];
				}
				else
				{
					str = read_string();
				}

				lv.back().second.push_back(FEntry{ key, hash, str });
			}
		}

		write_to_txt_file(lv, path_right);

		return 0;
	}
	else if (path_left.extension() == L".txt" && path_right.extension() == L".locres")
	{
		locres_vector lv;

		auto fin = std::ifstream{ path_left, std::ios::binary | std::ios::ate };
		auto buffer = std::vector<char>(fin.tellg());
		fin.seekg(0, std::ios::beg);
		fin.read(buffer.data(), buffer.size());

		std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
		std::wstring lines = converter.from_bytes(buffer.data());
		auto stream = std::wstringstream{ lines };

		std::wstring line;
		int mode = 0;
		while (std::getline(stream, line))
		{
			if (5 < line.length() && line.substr(0, 3) == L"=>[")
			{
				if (mode == 1)
					lv.back().second.back().s = lv.back().second.back().s.substr(0, lv.back().second.back().s.length() - 4);

				line = line.substr(3);
				const auto key = unescape_key(line.substr(0, line.find(L"]")));
				line = line.substr(line.find(L"[") + 1);
				const auto hash = std::stoul(line.substr(0, line.find(L"]")));
				lv.back().second.push_back(FEntry{ key, hash, L"" });
				mode = 1;
				continue;
			}
			if (3 < line.length() && line.substr(0, 3) == L"=>{")
			{
				if (mode == 1)
					lv.back().second.back().s = lv.back().second.back().s.substr(0, lv.back().second.back().s.length() - 4);

				const auto ns = unescape_key(line.substr(3, line.find(L"}") - 3));
				if (ns == L"[END]")
					break;
				lv.emplace_back();
				lv.back().first = ns;
				mode = 0;
				continue;
			}
			if (mode == 1)
				lv.back().second.back().s += line + L"\n";
		}

		write_to_locres_file(old, lv, path_right);
		return 0;
	}

	print_help();
	return 1;
}