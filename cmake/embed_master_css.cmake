# Generates a translation unit exposing res/master.css as
# webvis::master_stylesheet(). The user-agent stylesheet is part of the
# library's rendering semantics, so it is compiled in rather than left for the
# host to supply.
#
# Invoked at build time: cmake -DINPUT=<css> -DOUTPUT=<cpp> -P embed_master_css.cmake

file(READ "${INPUT}" css)

# The delimiter must not appear in the stylesheet itself.
if(css MATCHES "\\)webviscss\"")
    message(FATAL_ERROR "master.css contains the raw-string delimiter )webviscss\"")
endif()

file(WRITE "${OUTPUT}"
"// Generated from ${INPUT} by embed_master_css.cmake - do not edit.

#include <string_view>

namespace webvis
{
	std::string_view master_stylesheet();

	std::string_view master_stylesheet()
	{
		return R\"webviscss(${css})webviscss\";
	}
}
")
