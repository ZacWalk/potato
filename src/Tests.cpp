#include "pch.h"
#include "Should.h"
#include "css.h"

static void ShouldFindValueIndex()
{
    auto index = value_index(L"table-column", style_display_strings, display_inline);
    Should::Equal(8, index);
}

static void ShouldPassCssSize()
{
    css_length sz;
    sz.fromString(L"2em", font_size_strings);

    Should::Equal(2, sz.val());
    Should::Equal(css_units_em, sz.units());
}


std::wstring RunTests()
{	
	Tests tests;

    tests.Register(L"Should find value index", ShouldFindValueIndex);
    tests.Register(L"Should pass css size", ShouldPassCssSize);

	std::wstringstream output;
	tests.Run(output);
	return output.str();
}