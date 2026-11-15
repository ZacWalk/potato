// Purpose: Exercise platform-independent vocabulary and formatting without a desktop host.
#include "pch.h"

#include <iostream>

namespace
{
	std::vector<std::function<void()>> ui_tasks;
}

void webvis::dispatch_to_ui(std::function<void()> task)
{
	ui_tasks.push_back(std::move(task));
}

int main()
{
	int checks = 0;
	int failures = 0;
	const auto check = [&](const bool success, const char* name)
	{
		++checks;
		if (!success)
		{
			++failures;
			std::cerr << "FAIL: " << name << '\n';
		}
	};
	check(webvis::icmp("WiDtH", "width") == 0, "ASCII case folding");
	check(webvis::icmp("a", "ab") < 0, "prefix ordering");
	check(webvis::icmp("ab", "a") > 0, "longer key ordering");
	check(webvis::icmp(nullptr, nullptr) == 0, "identical absent keys");
	check(webvis::icmp(nullptr, "a") > 0, "absent left key");
	check(webvis::icmp("a", nullptr) < 0, "absent right key");
	check(webvis::nicmp("WIDTHx", "widthy", 5) == 0, "bounded nonterminated prefix");
	check(webvis::nicmp("a", "B", 1) < 0, "bounded ordering");
	check(webvis::nicmp(nullptr, nullptr, 0) == 0, "empty comparison reads nothing");
	check(webvis::icmp("\x80", "\xff") < 0, "unsigned non-ASCII byte ordering");
	check(webvis::is_equal(std::string_view{}, std::string_view{}), "empty views");
	check(webvis::is_equal(std::string_view("aBc", 2), std::string_view("ABd", 2)), "bounded views");
	check(webvis::ltstr_sv{}("a", "AB"), "transparent prefix comparator");
	check(!webvis::ltstr_sv{}("AB", "ab"), "transparent equivalent keys");
	check(webvis::format("%s:%d", "value", 42) == "value:42", "portable formatted values");
	check(webvis::format("%s", "") == "", "empty formatted output");
	const std::string long_text(4096, 'x');
	check(webvis::format("%s", long_text.c_str()) == long_text, "formatting beyond fixed buffers");
	check(webvis::value_index("TABLE-COLUMN", style_display_strings, -1) == 8, "CSS vocabulary lookup");
	bool dispatched = false;
	webvis::dispatch_to_ui([&] { dispatched = true; });
	check(!dispatched && ui_tasks.size() == 1, "host dispatch queues rather than running inline");
	for (const auto& task : ui_tasks) task();
	check(dispatched, "queued work runs on the host thread");
	webvis::tests passing;
	passing.register_test("passing case", [] {});
	const auto pass_report = passing.run_tests();
	check(pass_report.success() && pass_report.passed == 1 && pass_report.failed == 0,
	      "test reports count executed cases");
	check(pass_report.text.find("PASS  passing case") != std::string::npos &&
	          pass_report.html.find("passing case") != std::string::npos,
	      "console and HTML reports name the same case");
	webvis::tests failing;
	failing.register_test("failing case", [] { throw std::string("failure detail"); });
	const auto fail_report = failing.run_tests();
	check(!fail_report.success() && fail_report.passed == 0 && fail_report.failed == 1 &&
	          fail_report.text.find("FAIL  failing case: failure detail") != std::string::npos,
	      "a failed case is counted and its diagnostic reaches the console");
	failing.register_test("empty failure detail", [] { throw std::string{}; });
	check(failing.run_tests().failed == 2, "an empty exception message is still a failure");
	webvis::tests empty;
	check(!empty.run_tests().success(), "an empty test run cannot pass");
	std::cout << "webvis portable: " << checks << " checks, " << failures << " failures\n";
	return failures == 0 ? 0 : 1;
}
