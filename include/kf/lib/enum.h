#ifndef KF_LIB_ENUM_H
#define KF_LIB_ENUM_H

template <typename Enum, typename Storage>
    requires (__is_enum(Enum) && __is_integral(Storage))
class KfEnumStorage {
public:
    KfEnumStorage() = default;
    constexpr KfEnumStorage(Enum value) : value_(static_cast<Storage>(value)) {}

    constexpr operator Enum() const { return static_cast<Enum>(value_); }
    constexpr Storage encoded_value() const { return value_; }

    constexpr KfEnumStorage& operator=(Enum value)
    {
        value_ = static_cast<Storage>(value);
        return *this;
    }

private:
    Storage value_;
};

template <typename Enum, typename Left, typename Right>
constexpr bool operator==(KfEnumStorage<Enum, Left> lhs, KfEnumStorage<Enum, Right> rhs)
{
    return lhs.encoded_value() == rhs.encoded_value();
}

template <typename Enum, typename Integer>
    requires (__is_enum(Enum) && __is_integral(Integer))
constexpr Enum kf_enum_decode(Integer value)
{
    return static_cast<Enum>(value);
}

template <typename Integer, typename Enum>
    requires (__is_integral(Integer) && __is_enum(Enum))
constexpr Integer kf_enum_encode(Enum value)
{
    return static_cast<Integer>(value);
}

template <typename Integer, typename Enum, typename Storage>
    requires (__is_enum(Enum) && __is_integral(Integer))
constexpr Integer kf_enum_encode(KfEnumStorage<Enum, Storage> value)
{
    return static_cast<Integer>(value.encoded_value());
}

#endif // KF_LIB_ENUM_H
