#pragma once

#include <type_traits>

namespace sigmax
{

/// @brief Cast a scoped enum to its underlying integral type.
/// @details Use this at every conversion site instead of writing static_cast<UnderlyingType>(...);
///          keeps the cast tied to the enum's declared underlying type even if it changes later.
template<typename E>
constexpr auto AsInt(E e) noexcept -> std::underlying_type_t<E>
{
    static_assert(std::is_enum_v<E>, "AsInt requires an enum type");
    return static_cast<std::underlying_type_t<E>>(e);
}

}
