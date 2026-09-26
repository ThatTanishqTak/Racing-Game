#include "Powertrain/Assets/Json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <format>
#include <utility>

namespace Powertrain
{
	namespace
	{
		constexpr std::string_view k_Utf8Bom = "\xEF\xBB\xBF";

		bool IsDigit(char character)
		{
			return character >= '0' && character <= '9';
		}

		int HexValue(char character)
		{
			if (character >= '0' && character <= '9')
			{
				return character - '0';
			}
			if (character >= 'a' && character <= 'f')
			{
				return character - 'a' + 10;
			}
			if (character >= 'A' && character <= 'F')
			{
				return character - 'A' + 10;
			}

			return -1;
		}

		void AppendUtf8(std::string& out, uint32_t codePoint)
		{
			if (codePoint < 0x80)
			{
				out.push_back(static_cast<char>(codePoint));
			}
			else if (codePoint < 0x800)
			{
				out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
			else if (codePoint < 0x10000)
			{
				out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
			else
			{
				out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
				out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
			}
		}
	}

	JsonValue::JsonValue() = default;
	JsonValue::~JsonValue() = default;
	JsonValue::JsonValue(const JsonValue& other) = default;
	JsonValue::JsonValue(JsonValue&& other) noexcept = default;
	JsonValue& JsonValue::operator=(const JsonValue& other) = default;
	JsonValue& JsonValue::operator=(JsonValue&& other) noexcept = default;

	JsonValue::JsonValue(bool value) : m_Value(value)
	{

	}

	JsonValue::JsonValue(double value) : m_Value(value)
	{

	}

	JsonValue::JsonValue(int64_t value) : m_Value(static_cast<double>(value))
	{

	}

	JsonValue::JsonValue(std::string value) : m_Value(std::move(value))
	{

	}

	JsonValue::JsonValue(JsonArray value) : m_Value(std::move(value))
	{

	}

	JsonValue::JsonValue(JsonObject value) : m_Value(std::move(value))
	{

	}

	JsonType JsonValue::GetType() const
	{
		return static_cast<JsonType>(m_Value.index());
	}

	bool JsonValue::AsBool(bool fallback) const
	{
		const bool* l_Value = std::get_if<bool>(&m_Value);

		return l_Value != nullptr ? *l_Value : fallback;
	}

	double JsonValue::AsNumber(double fallback) const
	{
		const double* l_Value = std::get_if<double>(&m_Value);

		return l_Value != nullptr ? *l_Value : fallback;
	}

	int64_t JsonValue::AsInteger(int64_t fallback) const
	{
		const double* l_Value = std::get_if<double>(&m_Value);

		return l_Value != nullptr ? static_cast<int64_t>(std::llround(*l_Value)) : fallback;
	}

	std::string_view JsonValue::AsString(std::string_view fallback) const
	{
		const std::string* l_Value = std::get_if<std::string>(&m_Value);

		return l_Value != nullptr ? std::string_view(*l_Value) : fallback;
	}

	std::span<const JsonValue> JsonValue::AsArray() const
	{
		const JsonArray* l_Value = std::get_if<JsonArray>(&m_Value);

		return l_Value != nullptr ? std::span<const JsonValue>(*l_Value) : std::span<const JsonValue>();
	}

	std::span<const JsonMember> JsonValue::AsObject() const
	{
		const JsonObject* l_Value = std::get_if<JsonObject>(&m_Value);

		return l_Value != nullptr ? std::span<const JsonMember>(*l_Value) : std::span<const JsonMember>();
	}

	size_t JsonValue::Size() const
	{
		if (const JsonArray* l_Array = std::get_if<JsonArray>(&m_Value))
		{
			return l_Array->size();
		}
		if (const JsonObject* l_Object = std::get_if<JsonObject>(&m_Value))
		{
			return l_Object->size();
		}

		return 0;
	}

	const JsonValue* JsonValue::At(size_t index) const
	{
		const JsonArray* l_Array = std::get_if<JsonArray>(&m_Value);

		return l_Array != nullptr && index < l_Array->size() ? &(*l_Array)[index] : nullptr;
	}

	const JsonValue* JsonValue::Find(std::string_view key) const
	{
		const JsonObject* l_Object = std::get_if<JsonObject>(&m_Value);
		if (l_Object == nullptr)
		{
			return nullptr;
		}

		const auto l_Found = std::find_if(l_Object->begin(), l_Object->end(), [key](const JsonMember& member)
		{
			return member.Key == key;
		});

		return l_Found != l_Object->end() ? &l_Found->Value : nullptr;
	}

	bool JsonValue::GetBool(std::string_view key, bool fallback) const
	{
		const JsonValue* l_Value = Find(key);

		return l_Value != nullptr ? l_Value->AsBool(fallback) : fallback;
	}

	double JsonValue::GetNumber(std::string_view key, double fallback) const
	{
		const JsonValue* l_Value = Find(key);

		return l_Value != nullptr ? l_Value->AsNumber(fallback) : fallback;
	}

	int64_t JsonValue::GetInteger(std::string_view key, int64_t fallback) const
	{
		const JsonValue* l_Value = Find(key);

		return l_Value != nullptr ? l_Value->AsInteger(fallback) : fallback;
	}

	std::string_view JsonValue::GetString(std::string_view key, std::string_view fallback) const
	{
		const JsonValue* l_Value = Find(key);

		return l_Value != nullptr ? l_Value->AsString(fallback) : fallback;
	}

	bool JsonParser::Parse(std::string_view text, JsonValue& out)
	{
		m_Text = text;
		m_Position = 0;
		m_Error.clear();

		if (m_Text.starts_with(k_Utf8Bom))
		{
			m_Position = k_Utf8Bom.size();
		}

		SkipWhitespace();
		if (AtEnd())
		{
			return Fail("Empty document");
		}

		JsonValue l_Value;
		if (!ParseValue(l_Value, 0))
		{
			return false;
		}

		SkipWhitespace();
		if (!AtEnd())
		{
			return Fail("Trailing characters after the document");
		}

		out = std::move(l_Value);

		return true;
	}

	bool JsonParser::ParseValue(JsonValue& out, uint32_t depth)
	{
		if (depth > k_MaxDepth)
		{
			return Fail(std::format("Nesting deeper than {}", k_MaxDepth));
		}

		SkipWhitespace();

		const char l_Character = Peek();
		switch (l_Character)
		{
			case '{':
			{
				return ParseObject(out, depth);
			}
			case '[':
			{
				return ParseArray(out, depth);
			}
			case '"':
			{
				std::string l_String;
				if (!ParseString(l_String))
				{
					return false;
				}

				out = JsonValue(std::move(l_String));

				return true;
			}
			case 't':
			{
				return ParseLiteral("true", JsonValue(true), out);
			}
			case 'f':
			{
				return ParseLiteral("false", JsonValue(false), out);
			}
			case 'n':
			{
				return ParseLiteral("null", JsonValue(), out);
			}
			default:
			{
				if (l_Character == '-' || IsDigit(l_Character))
				{
					return ParseNumber(out);
				}

				return AtEnd() ? Fail("Unexpected end of document") : Fail(std::format("Unexpected character '{}'", l_Character));
			}
		}
	}

	bool JsonParser::ParseObject(JsonValue& out, uint32_t depth)
	{
		Consume('{');

		JsonObject l_Members;

		SkipWhitespace();
		if (Consume('}'))
		{
			out = JsonValue(std::move(l_Members));

			return true;
		}

		while (true)
		{
			SkipWhitespace();
			if (Peek() != '"')
			{
				return Fail("Expected a string key");
			}

			JsonMember l_Member;
			if (!ParseString(l_Member.Key))
			{
				return false;
			}

			SkipWhitespace();
			if (!Consume(':'))
			{
				return Fail("Expected ':' after the key");
			}

			if (!ParseValue(l_Member.Value, depth + 1))
			{
				return false;
			}

			l_Members.push_back(std::move(l_Member));

			SkipWhitespace();
			if (Consume(','))
			{
				continue;
			}
			if (Consume('}'))
			{
				break;
			}

			return Fail("Expected ',' or '}' in object");
		}

		out = JsonValue(std::move(l_Members));

		return true;
	}

	bool JsonParser::ParseArray(JsonValue& out, uint32_t depth)
	{
		Consume('[');

		JsonArray l_Elements;

		SkipWhitespace();
		if (Consume(']'))
		{
			out = JsonValue(std::move(l_Elements));

			return true;
		}

		while (true)
		{
			JsonValue l_Element;
			if (!ParseValue(l_Element, depth + 1))
			{
				return false;
			}

			l_Elements.push_back(std::move(l_Element));

			SkipWhitespace();
			if (Consume(','))
			{
				continue;
			}
			if (Consume(']'))
			{
				break;
			}

			return Fail("Expected ',' or ']' in array");
		}

		out = JsonValue(std::move(l_Elements));

		return true;
	}

	bool JsonParser::ParseString(std::string& out)
	{
		Consume('"');

		while (true)
		{
			if (AtEnd())
			{
				return Fail("Unterminated string");
			}

			const char l_Character = m_Text[m_Position++];
			if (l_Character == '"')
			{
				return true;
			}

			if (static_cast<unsigned char>(l_Character) < 0x20)
			{
				return Fail("Control character in string");
			}

			if (l_Character != '\\')
			{
				out.push_back(l_Character);

				continue;
			}

			if (AtEnd())
			{
				return Fail("Unterminated escape sequence");
			}

			const char l_Escape = m_Text[m_Position++];
			switch (l_Escape)
			{
				case '"':
				{
					out.push_back('"');
					
					break;
				}
				case '\\':
				{
					out.push_back('\\');
					
					break;
				}
				case '/':
				{
					out.push_back('/');
					
					break;
				}
				case 'b':
				{
					out.push_back('\b');
					
					break;
				}
				case 'f':
				{
					out.push_back('\f');
					
					break;
				}
				case 'n':
				{
					out.push_back('\n');
					
					break;
				}
				case 'r':
				{
					out.push_back('\r');
					
					break;
				}
				case 't':
				{
					out.push_back('\t');
					
					break;
				}
				case 'u':
				{
					uint32_t l_CodePoint = 0;
					if (!ParseCodeUnit(l_CodePoint))
					{
						return false;
					}

					// A high surrogate must be followed by an escaped low surrogate
					if (l_CodePoint >= 0xD800 && l_CodePoint <= 0xDBFF)
					{
						if (!Consume('\\') || !Consume('u'))
						{
							return Fail("High surrogate without a low surrogate");
						}

						uint32_t l_Low = 0;
						if (!ParseCodeUnit(l_Low))
						{
							return false;
						}
						if (l_Low < 0xDC00 || l_Low > 0xDFFF)
						{
							return Fail("Invalid low surrogate");
						}

						l_CodePoint = 0x10000 + ((l_CodePoint - 0xD800) << 10) + (l_Low - 0xDC00);
					}
					else if (l_CodePoint >= 0xDC00 && l_CodePoint <= 0xDFFF)
					{
						return Fail("Low surrogate without a high surrogate");
					}

					AppendUtf8(out, l_CodePoint);

					break;
				}
				default:
					return Fail(std::format("Invalid escape '\\{}'", l_Escape));
			}
		}
	}

	bool JsonParser::ParseCodeUnit(uint32_t& codeUnit)
	{
		if (m_Position + 4 > m_Text.size())
		{
			return Fail("Truncated \\u escape");
		}

		codeUnit = 0;
		for (int l_Index = 0; l_Index < 4; ++l_Index)
		{
			const int l_Digit = HexValue(m_Text[m_Position++]);
			if (l_Digit < 0)
			{
				return Fail("Invalid hex digit in \\u escape");
			}

			codeUnit = (codeUnit << 4) | static_cast<uint32_t>(l_Digit);
		}

		return true;
	}

	bool JsonParser::ParseNumber(JsonValue& out)
	{
		// Validate against the JSON grammar first so from_chars never sees hex, infinities or leading '+'
		const size_t l_Start = m_Position;

		Consume('-');

		if (Consume('0'))
		{
			// A leading zero stands alone
		}
		else if (IsDigit(Peek()))
		{
			while (IsDigit(Peek()))
			{
				++m_Position;
			}
		}
		else
		{
			return Fail("Expected a digit");
		}

		if (Consume('.'))
		{
			if (!IsDigit(Peek()))
			{
				return Fail("Expected a digit after '.'");
			}
			while (IsDigit(Peek()))
			{
				++m_Position;
			}
		}

		if (Peek() == 'e' || Peek() == 'E')
		{
			++m_Position;
			if (Peek() == '+' || Peek() == '-')
			{
				++m_Position;
			}
			if (!IsDigit(Peek()))
			{
				return Fail("Expected a digit in the exponent");
			}
			while (IsDigit(Peek()))
			{
				++m_Position;
			}
		}

		double l_Number = 0.0;
		const char* l_Begin = m_Text.data() + l_Start;
		const char* l_End = m_Text.data() + m_Position;
		const std::from_chars_result l_Result = std::from_chars(l_Begin, l_End, l_Number);
		if (l_Result.ec != std::errc() || l_Result.ptr != l_End)
		{
			return Fail("Number out of range");
		}

		out = JsonValue(l_Number);

		return true;
	}

	bool JsonParser::ParseLiteral(std::string_view literal, JsonValue value, JsonValue& out)
	{
		if (!m_Text.substr(m_Position).starts_with(literal))
		{
			return Fail(std::format("Expected '{}'", literal));
		}

		m_Position += literal.size();
		out = std::move(value);

		return true;
	}

	void JsonParser::SkipWhitespace()
	{
		while (!AtEnd())
		{
			const char l_Character = m_Text[m_Position];
			if (l_Character != ' ' && l_Character != '\t' && l_Character != '\n' && l_Character != '\r')
			{
				break;
			}

			++m_Position;
		}
	}

	bool JsonParser::Consume(char expected)
	{
		if (Peek() != expected)
		{
			return false;
		}

		++m_Position;

		return true;
	}

	bool JsonParser::Fail(std::string_view message)
	{
		// Line and column are computed only on failure, so the hot path never tracks them
		size_t l_Line = 1;
		size_t l_LineStart = 0;
		const size_t l_Position = std::min(m_Position, m_Text.size());
		for (size_t l_Index = 0; l_Index < l_Position; ++l_Index)
		{
			if (m_Text[l_Index] == '\n')
			{
				++l_Line;
				l_LineStart = l_Index + 1;
			}
		}

		m_Error = std::format("{} at line {}, column {}", message, l_Line, l_Position - l_LineStart + 1);

		return false;
	}
}