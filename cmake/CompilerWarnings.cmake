# rtype_set_warnings(<target>)
#
# Applies the project's warning policy to a target.
# Warnings are PRIVATE: they apply when compiling the target itself,
# never to the code of whoever links against it.

function(rtype_set_warnings target)
    set(msvc_warnings
        /W4
        /permissive-
        /w14242 # conversion, possible loss of data
        /w14254 # operator conversion, possible loss of data
        /w14263 # member function does not override a base class virtual function
        /w14265 # class has virtual functions but non-virtual destructor
        /w14287 # unsigned/negative constant mismatch
        /w14296 # expression is always true/false
        /w14311 # pointer truncation
        /w14826 # conversion is sign-extended
        /w14928 # illegal copy-initialization
    )

    set(common_warnings
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )

    set(gcc_warnings
        ${common_warnings}
        -Wmisleading-indentation
        -Wduplicated-cond
        -Wduplicated-branches
        -Wlogical-op
        -Wuseless-cast
    )

    if(RTYPE_WARNINGS_AS_ERRORS)
        list(APPEND msvc_warnings /WX)
        list(APPEND common_warnings -Werror)
        list(APPEND gcc_warnings -Werror)
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE ${msvc_warnings})
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE ${common_warnings})
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE ${gcc_warnings})
    endif()
endfunction()
