#pragma once

#include "strings.h"

class Should
{
public:

	static void Equal(const wchar_t * expected, const wchar_t * actual, const wchar_t * message = L"Test")
	{
		if (!is_equal(actual, expected))
		{
			throw Format(L"%s: expected '%s', got '%s'", message, expected, actual);
		}
	}

	static void Equal(const std::wstring &expected, const std::wstring &actual, const wchar_t * message = L"Test")
	{
		Equal(expected.c_str(), actual.c_str(), message);
	}

	static void Equal(int expected, int actual, const wchar_t * message = L"Test")
	{
		static const int size = 64;
		wchar_t expected_text[size], actual_text[size];
		_itow_s(expected, expected_text, size, 10);
		_itow_s(actual, actual_text, size, 10);
		Equal(expected_text, actual_text, message);
	}

	static void Equal(bool expected, bool actual, const wchar_t * message = L"Test")
	{
		Equal(From(expected), From(actual), message);
	}

	static void EqualTrue(bool actual, const wchar_t * message = L"Test")
	{
		Equal(true, actual, message);
	}
};

class Tests
{
private:

	static std::chrono::high_resolution_clock::time_point now()
	{
		return std::chrono::high_resolution_clock::now();
	};

	static long long duration_in_microseconds(const std::chrono::high_resolution_clock::time_point &started)
	{
		auto dur = now() - started;
		return std::chrono::duration_cast<std::chrono::microseconds>(dur).count();
	};

	std::map<std::wstring, std::function<void()>> _tests;

public:

	inline void Register(const std::wstring &name, const std::function<void()> &f)
	{
		_tests[name] = f;
	}

	void Run(std::wstringstream &output)
	{
		auto started = now();
		auto count = 0;

        output << "<html>"; 
        output << "<style>";
        output << "body { background-color: LightSlateGray; }";
        output << "td.fail { background-color: OrangeRed; }";
        output << "</style>";
        output << "<body><table>";

		for (auto &test : _tests)
		{
			output << "<tr><td>" << test.first << "</td><td>";
			auto started = now();

			try
			{
				test.second();

                output << "<td>";
                output << " success in " << duration_in_microseconds(started) << " microseconds" << "<br>";
            }
			catch (const std::wstring &message)
			{
                output << "<td class='fail'>";
				output << " FAILED in " << duration_in_microseconds(started) << L" microseconds" << "<br>";
				output << message;
			}
			catch (const std::exception &e)
			{
                output << "<td class='fail'>";
                output << " FAILED in " << duration_in_microseconds(started) << " microseconds" << "<br>";
				output << e.what();
			}

            output << "</td></tr>";

			count += 1;
		}

        output << "</table>";
		output << "<h1>" << L"Completed " << count << L" tests in " << duration_in_microseconds(started) << L" microseconds" << "</h1>";
        output << "</body></html>";
	}
};
