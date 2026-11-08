#include "server/TextFile.hpp"

#include <fstream>
#include <system_error>

namespace server {

bool replaceFile(const std::filesystem::path& file, std::string_view content)
{
    std::filesystem::path temporary = file;
    temporary += ".tmp";

    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << content;
        output.close();
        if (!output) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
            return false;
        }
    }

    // Not fatal: the permissions only make the file harder to read for others
    std::error_code error;
    std::filesystem::permissions(
        temporary, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write, error);

    std::filesystem::rename(temporary, file, error);
    if (error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
    return true;
}

bool keepDamagedCopy(const std::filesystem::path& file)
{
    // Streams, not copy_file(): with MinGW it refuses to replace an older copy
    std::ifstream input(file, std::ios::binary);
    std::filesystem::path copy = file;
    copy += ".damaged";
    std::ofstream output(copy, std::ios::binary | std::ios::trunc);
    output << input.rdbuf();
    output.close();
    return static_cast<bool>(output);
}

} // namespace server
