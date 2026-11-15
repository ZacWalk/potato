#include <string_view>
#include <cstdio>

int main()
{
	constexpr std::string_view name = "potato";
	std::puts("potato: build-only smoke; behavioural counts come from potato_selftest");
	return name.empty() ? 1 : 0;
}
