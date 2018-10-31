#include <CustomException.hpp>

using vkpbr::CustomException;

CustomException::CustomException(std::string info)
	: whatInfo(std::move(info))
{
}

auto CustomException::what() const noexcept -> const char*
{
	return whatInfo.c_str();
}