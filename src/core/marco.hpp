#ifndef INCLUDE_TREELANG_CORE_MARCO_HPP
#define INCLUDE_TREELANG_CORE_MARCO_HPP

#define DEFAULT_CONSTRUCTOR(name)            \
    name() = default;                        \
    name(const name &) = default;            \
    name(name &&) noexcept = default;        \
    name &operator=(const name &) = default; \
    name &operator=(name &&) noexcept = default;


#define DEFINE_ATTRIBUTE(type, name)             \
private:                                         \
    type name;                                   \
                                                 \
public:                                          \
    type &get_##name() noexcept { return name; } \
    const type &get_##name() const noexcept { return name; }

#endif  // INCLUDE_TREELANG_CORE_MARCO_HPP