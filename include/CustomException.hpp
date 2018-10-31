#pragma once

#include <exception>
#include <string>

namespace vkpbr {

    class CustomException : public std::exception {
    public:
        explicit CustomException(std::string info);
        auto what() const noexcept -> const char*;

    protected:
        const std::string whatInfo;
    };
}