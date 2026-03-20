# AirPlay core library — builds RPiPlay's lib/ as a static library.
#
# Downloads RPiPlay via FetchContent, compiles all C sources from lib/
# EXCEPT dnssd.c (we handle mDNS ourselves via mjansson/mdns.h).
# A stub dnssd implementation is provided to satisfy raop.c's header dependency.

include(FetchContent)

FetchContent_Declare(
    rpiplay
    GIT_REPOSITORY https://github.com/FD-/RPiPlay.git
    GIT_TAG        64d0341ed3bef098c940c9ed0675948870a271f9
    GIT_SHALLOW    TRUE
    EXCLUDE_FROM_ALL
)

# Download RPiPlay source — EXCLUDE_FROM_ALL prevents its own CMakeLists.txt
# from being processed (we compile lib/ sources ourselves below).
FetchContent_MakeAvailable(rpiplay)

# --------------------------------------------------------------------------
# RPiPlay lib/ sources (excluding dnssd.c — we handle mDNS ourselves)
# --------------------------------------------------------------------------
set(RPIPLAY_LIB_DIR "${rpiplay_SOURCE_DIR}/lib")

set(RPIPLAY_SOURCES
    ${RPIPLAY_LIB_DIR}/raop.c
    ${RPIPLAY_LIB_DIR}/raop_rtp.c
    ${RPIPLAY_LIB_DIR}/raop_rtp_mirror.c
    ${RPIPLAY_LIB_DIR}/raop_ntp.c
    ${RPIPLAY_LIB_DIR}/raop_buffer.c
    ${RPIPLAY_LIB_DIR}/mirror_buffer.c
    ${RPIPLAY_LIB_DIR}/httpd.c
    ${RPIPLAY_LIB_DIR}/http_request.c
    ${RPIPLAY_LIB_DIR}/http_response.c
    ${RPIPLAY_LIB_DIR}/crypto.c
    ${RPIPLAY_LIB_DIR}/fairplay_playfair.c
    ${RPIPLAY_LIB_DIR}/pairing.c
    ${RPIPLAY_LIB_DIR}/netutils.c
    ${RPIPLAY_LIB_DIR}/logger.c
    ${RPIPLAY_LIB_DIR}/byteutils.c
    ${RPIPLAY_LIB_DIR}/utils.c

    # llhttp (embedded HTTP parser)
    ${RPIPLAY_LIB_DIR}/llhttp/api.c
    ${RPIPLAY_LIB_DIR}/llhttp/http.c
    ${RPIPLAY_LIB_DIR}/llhttp/llhttp.c

    # FairPlay (PlayFair SAP crypto)
    ${RPIPLAY_LIB_DIR}/playfair/hand_garble.c
    ${RPIPLAY_LIB_DIR}/playfair/modified_md5.c
    ${RPIPLAY_LIB_DIR}/playfair/omg_hax.c
    ${RPIPLAY_LIB_DIR}/playfair/playfair.c
    ${RPIPLAY_LIB_DIR}/playfair/sap_hash.c

    # Stub dnssd — satisfies header dependency without Bonjour SDK
    ${CMAKE_CURRENT_SOURCE_DIR}/dnssd_stub.c
)

add_library(airplay_core STATIC ${RPIPLAY_SOURCES})

target_include_directories(airplay_core
    PUBLIC  ${RPIPLAY_LIB_DIR}
    PRIVATE ${RPIPLAY_LIB_DIR}/llhttp
    PRIVATE ${RPIPLAY_LIB_DIR}/playfair
)

target_link_libraries(airplay_core
    PRIVATE
        OpenSSL::SSL
        OpenSSL::Crypto
        ws2_32
)

# RPiPlay uses C99; suppress MSVC warnings for POSIX functions
target_compile_definitions(airplay_core PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    _WINSOCK_DEPRECATED_NO_WARNINGS
)

if(MSVC)
    # Suppress common C warnings in third-party code
    target_compile_options(airplay_core PRIVATE
        /W3
        /wd4244   # conversion, possible loss of data
        /wd4267   # size_t to int conversion
        /wd4996   # deprecated POSIX names
    )
else()
    target_compile_options(airplay_core PRIVATE -w)
endif()
