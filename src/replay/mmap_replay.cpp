#include <windows.h>
#include <cstdint>
#include <iostream>

#include "../feed/feed_handler.hpp"
#include "../feed/binary_parser.hpp"

std::uint64_t run_mmap_replay(FeedHandler& fh, const char* filename) {
    HANDLE hFile = CreateFileA(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open file: " << filename << "\n";
        return 0;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        std::cerr << "GetFileSizeEx failed\n";
        CloseHandle(hFile);
        return 0;
    }

    HANDLE hMap = CreateFileMappingA(
        hFile,
        nullptr,
        PAGE_READONLY,
        0,
        0,
        nullptr
    );

    if (!hMap) {
        std::cerr << "CreateFileMapping failed\n";
        CloseHandle(hFile);
        return 0;
    }

    const uint8_t* ptr = static_cast<const uint8_t*>(
        MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0)
    );

    if (!ptr) {
        std::cerr << "MapViewOfFile failed\n";
        CloseHandle(hMap);
        CloseHandle(hFile);
        return 0;
    }

    const uint8_t* end = ptr + fileSize.QuadPart;

    BinaryParser parser;
    std::uint64_t count = 0;

    while (ptr < end) {
        MarketUpdate u{};
        std::size_t consumed = parser.parse(ptr, end, u);
        if (consumed == 0) {
            break; // malformed or truncated
        }
        ptr += consumed;


        while (!fh.onUpdate(u)) {
            // spin until queue has space
            std::cout << "FULL\n";
        }

        ++count;
    }

    UnmapViewOfFile(ptr);
    CloseHandle(hMap);
    CloseHandle(hFile);

    return count;
}