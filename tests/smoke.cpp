#include <string_view>

int main()
{
    constexpr std::string_view name = "potato";
    return name.empty() ? 1 : 0;
}
