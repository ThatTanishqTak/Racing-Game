#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Powertrain
{
	class JsonValue;
	struct JsonMember;

	using JsonArray = std::vector<JsonValue>;

	// Members keep their document order; lookups are linear, which is fine for the sizes the engine parses
	using JsonObject = std::vector<JsonMember>;

	// In the order of the variant below
	enum class JsonType : uint8_t
	{
		Null,
		Bool,
		Number,
		String,
		Array,
		Object
	};

	class JsonValue
	{
	public:
		JsonValue();
		explicit JsonValue(bool value);
		explicit JsonValue(double value);
		explicit JsonValue(int64_t value);
		explicit JsonValue(std::string value);
		explicit JsonValue(JsonArray value);
		explicit JsonValue(JsonObject value);
		~JsonValue();

		JsonValue(const JsonValue& other);
		JsonValue(JsonValue&& other) noexcept;
		JsonValue& operator=(const JsonValue& other);
		JsonValue& operator=(JsonValue&& other) noexcept;

		JsonType GetType() const;
		bool IsNull() const { return GetType() == JsonType::Null; }
		bool IsBool() const { return GetType() == JsonType::Bool; }
		bool IsNumber() const { return GetType() == JsonType::Number; }
		bool IsString() const { return GetType() == JsonType::String; }
		bool IsArray() const { return GetType() == JsonType::Array; }
		bool IsObject() const { return GetType() == JsonType::Object; }

		bool AsBool(bool fallback = false) const;
		double AsNumber(double fallback = 0.0) const;
		float AsFloat(float fallback = 0.0f) const { return static_cast<float>(AsNumber(static_cast<double>(fallback))); }
		int64_t AsInteger(int64_t fallback = 0) const;
		std::string_view AsString(std::string_view fallback = {}) const;

		// Empty when this is not an array or object
		std::span<const JsonValue> AsArray() const;
		std::span<const JsonMember> AsObject() const;

		// Element or member count; zero for scalars
		size_t Size() const;

		// nullptr when out of range or not an array
		const JsonValue* At(size_t index) const;

		// nullptr when missing or not an object
		const JsonValue* Find(std::string_view key) const;

		bool GetBool(std::string_view key, bool fallback) const;
		double GetNumber(std::string_view key, double fallback) const;
		float GetFloat(std::string_view key, float fallback) const { return static_cast<float>(GetNumber(key, static_cast<double>(fallback))); }
		int64_t GetInteger(std::string_view key, int64_t fallback) const;
		std::string_view GetString(std::string_view key, std::string_view fallback) const;

	private:
		std::variant<std::monostate, bool, double, std::string, JsonArray, JsonObject> m_Value;
	};

	struct JsonMember
	{
		std::string Key;
		JsonValue Value;
	};

	class JsonParser
	{
	public:
		static constexpr uint32_t k_MaxDepth = 256;

		bool Parse(std::string_view text, JsonValue& out);
		const std::string& GetError() const { return m_Error; }

	private:
		bool ParseValue(JsonValue& out, uint32_t depth);
		bool ParseObject(JsonValue& out, uint32_t depth);
		bool ParseArray(JsonValue& out, uint32_t depth);
		bool ParseString(std::string& out);
		bool ParseNumber(JsonValue& out);
		bool ParseLiteral(std::string_view literal, JsonValue value, JsonValue& out);
		bool ParseCodeUnit(uint32_t& codeUnit);

		void SkipWhitespace();
		bool AtEnd() const { return m_Position >= m_Text.size(); }
		char Peek() const { return AtEnd() ? '\0' : m_Text[m_Position]; }
		bool Consume(char expected);
		bool Fail(std::string_view message);

	private:
		std::string_view m_Text;
		size_t m_Position = 0;
		std::string m_Error;
	};
}