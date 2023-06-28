#include <string>
#include <tuple>
#include <variant>
#include <type_traits>
#include <bit>

namespace RSE {
#if __cplusplus >= 202002L

    template<typename T>
    using remove_cvref_t = std::remove_cvref_t<T>;

    using endian = std::endian;

#else

// For gcc only
    enum class endian {
        little = __ORDER_LITTLE_ENDIAN__,
        big    = __ORDER_BIG_ENDIAN__,
        native = __BYTE_ORDER__
    };

    template<typename T>
    using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
#endif


    template<typename T, typename ErrorType>
    static inline constexpr bool double_declaration_v = std::is_same_v<
        remove_cvref_t<T>,
        remove_cvref_t<ErrorType>
    >;

    template<typename ErrorType>
    class Error {
    public:
        ErrorType* error;

        template<typename ...Ts>
        constexpr inline explicit Error(Ts&&... args) noexcept
            : error(new ErrorType(std::forward<Ts>(args)...))
        {}

        constexpr inline explicit Error(ErrorType&& error) noexcept
            : error(new ErrorType(std::forward<ErrorType>(error)))
        {}

        constexpr inline explicit Error(const ErrorType& error) noexcept
            : error(new ErrorType(error))
        {}

        template <typename NewErrorType, typename std::enable_if<std::is_base_of_v<NewErrorType, ErrorType>, int>::type=0>
        constexpr inline operator Error<NewErrorType>&() {
            return reinterpret_cast<Error<NewErrorType>&>(*this);
        }

        constexpr inline Error(const Error& error) noexcept {
            this->error = new ErrorType(*error.error);
        }

        constexpr inline Error(Error&& error) noexcept {
            this->error = error.error;
            error.error = nullptr;
        }

        inline ~Error() noexcept = default; // Do not free there !! The lifetime is handle by Rse[Small/Big]

        constexpr inline void free() noexcept {
            delete this->error;
        };
    };

    template<typename T, typename ErrorType>
    class RseBig {
    public:
        using Error = RSE::Error<ErrorType>;

        class RseErrorWarp {
        public:
            constexpr inline explicit operator bool() const noexcept {
                return this->success == false;
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline operator const ErrorType&() const noexcept {
                return *this->error.error;
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline const ErrorType& operator*() const noexcept {
                return *this->error.error;
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline const ErrorType* operator->() const noexcept {
                return this->error.error;
            }

            constexpr inline explicit RseErrorWarp(T&& value) noexcept
                : success(true), value(std::forward<T>(value))
            {}

            constexpr inline explicit RseErrorWarp(Error&& error) noexcept
                : success(false), error(std::move(error))
            {}

            constexpr inline explicit RseErrorWarp(ErrorType error) noexcept
                : success(false), error(Error(std::move(error)))
            {}

            inline ~RseErrorWarp() noexcept {}

            constexpr inline RseErrorWarp(const RseErrorWarp& rseErrorWarp) noexcept = default;
            constexpr inline RseErrorWarp(RseErrorWarp&& rseErrorWarp) noexcept = default;

            friend RseBig;
        private:
            bool success;
            union {
                T value;
                Error error;
            };
        };


        constexpr inline RseBig(T value) noexcept // Perfect forwarding to avoid conflict between cont& and &&
            : rseErrorWarp(std::forward<T>(value))
        {}

        constexpr inline RseBig(Error error) noexcept
            : rseErrorWarp(std::move(error))
        {}

        template<typename InheritedErrorType>
        constexpr inline RseBig(RSE::Error<InheritedErrorType>&& error) noexcept
            : rseErrorWarp(std::move(static_cast<Error&>(error)))
        {}

        constexpr inline RseBig(ErrorType error) noexcept
            : rseErrorWarp(std::move(error))
        {}

        constexpr inline RseBig(RseBig&& rse) noexcept
            : rseErrorWarp(rse.rseErrorWarp)
        {
            rse.rseErrorWarp.error.error = nullptr; // set the pointer to a false state
        }

        inline ~RseBig() noexcept {
            if (this->rseErrorWarp.success == false)
                this->rseErrorWarp.error.free();
        }

        [[nodiscard]] constexpr inline explicit operator bool() const noexcept {
            return this->success == true;
        }

        [[nodiscard]] constexpr inline T& getValue() noexcept {
            return this->rseErrorWarp.value;
        };

        [[nodiscard]] constexpr inline const T& getValue() const noexcept {
            return this->rseErrorWarp.value;
        };

        [[nodiscard]] constexpr inline ErrorType& getError() noexcept {
            return this->rseErrorWarp.error;
        };

        [[nodiscard]] constexpr inline const ErrorType& getError() const noexcept {
            return this->rseErrorWarp.error;
        };

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() & noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() && noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() const & noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() const && noexcept {
            return get_helper<Index>(*this);
        }

    private:
        template<std::size_t Index, typename Self>
        static constexpr inline auto&& get_helper(Self&& t)
        {
            static_assert(Index < 2, "Index out of bounds");
            if constexpr (Index == 0) return std::forward<Self>(t).rseErrorWarp.value;
            if constexpr (Index == 1) return std::forward<Self>(t).rseErrorWarp;
        }

        RseErrorWarp rseErrorWarp;
    };

    template<typename T, typename ErrorType>
    class RseSmall {
        static inline constexpr bool is_little_endian = endian::native == endian::little;
        static inline constexpr bool is_big_endian = endian::native == endian::big;

        static inline constexpr size_t size = sizeof(T);

        constexpr static size_t getOffset() {
            if constexpr (is_little_endian) {
                return size <= 4 ? 4 : size <= 6 ? 2 : 1; // [1u4] = 4, [5u6] = 2, [7] = 1
            } else if constexpr (is_big_endian) {
                return 0; // [1u7] = 0
            } else {
                static_assert(is_little_endian || is_big_endian, "Endianness not supported");
            }
        }
        static inline constexpr size_t offset = getOffset();
    public:
        using Error = RSE::Error<ErrorType>;

        class RseErrorWarp {
        public:
            constexpr inline explicit operator bool() const noexcept {
                return !(reinterpret_cast<size_t>(this->error.error) & 1);
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline operator const ErrorType&() const noexcept {
                return *this->error.error;
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline const ErrorType& operator*() const noexcept {
                return *this->error.error;
            }

            // Not safe make sur to check bool 1st
            [[nodiscard]] constexpr inline const ErrorType* operator->() const noexcept {
                return this->error.error;
            }

            constexpr inline explicit RseErrorWarp(T value) noexcept
                : value(std::forward<T>(value))
            {
                this->error.error = reinterpret_cast<ErrorType*>(size_t(this->error.error) | size_t(1));
            }

            constexpr inline explicit RseErrorWarp(Error error) noexcept
                : error(std::move(error))
            {}

            //template<typename _t=T, typename _error_type=ErrorType>
            //requires (!double_declaration_v<_t, _error_type>) // disable the function if T and ErrorType are the same
            template<typename _t=T, typename _error_type=ErrorType, typename std::enable_if<!double_declaration_v<_t, _error_type>, int>::type=0>
            constexpr inline explicit RseErrorWarp(ErrorType error) noexcept
                : error(Error(std::move(error)))
            {}

            constexpr inline RseErrorWarp(RseErrorWarp&& rseErrorWarp) noexcept = default;

            friend RseSmall;
        private:

            union {
                struct {
                    uint8_t _offset[offset]; // Is unused, there only
                    T value;
                };
                Error error;
            };
        };

        constexpr inline RseSmall(T value) noexcept // Perfect forwarding to avoid conflict between cont& and &&
            : rseErrorWarp(std::forward<T>(value))
        {}

        constexpr inline RseSmall(Error&& error) noexcept
            : rseErrorWarp(std::move(error))
        {}

        template<typename InheritedErrorType>
        constexpr inline RseSmall(RSE::Error<InheritedErrorType>&& error) noexcept
            : rseErrorWarp(std::move(static_cast<Error&>(error)))
        {}


        template<typename _t=T, typename _error_type=ErrorType, typename std::enable_if<!double_declaration_v<_t, _error_type>, int>::type=0>
        constexpr inline RseSmall(ErrorType error) noexcept
            : rseErrorWarp(std::move(error))
        {}

        constexpr inline RseSmall(RseSmall&& rse) noexcept
            : rseErrorWarp(rse.rseErrorWarp)
        {
            rse.rseErrorWarp.error = 1; // set the pointer to a false state
        }

        inline ~RseSmall() noexcept {
            if ((reinterpret_cast<size_t>(this->rseErrorWarp.error.error) & 1) == 0)
                this->rseErrorWarp.error.free();
        }

        [[nodiscard]] constexpr inline explicit operator bool() const noexcept {
            return !bool(this->rseErrorWarp);
        }

        [[nodiscard]] constexpr inline T& getValue() noexcept {
            return this->rseErrorWarp.value;
        };

        [[nodiscard]] constexpr inline const T& getValue() const noexcept {
            return this->rseErrorWarp.value;
        };

        [[nodiscard]] constexpr inline ErrorType& getError() noexcept {
            return this->rseErrorWarp.error;
        };

        [[nodiscard]] constexpr inline const ErrorType& getError() const noexcept {
            return this->rseErrorWarp.error;
        };

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() & noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() && noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() const & noexcept {
            return get_helper<Index>(*this);
        }

        template<std::size_t Index>
        [[nodiscard]] constexpr inline decltype(auto) get() const && noexcept {
            return get_helper<Index>(*this);
        }

    private:
        template<std::size_t Index, typename Self>
        static constexpr inline auto&& get_helper(Self&& t)
        {
            static_assert(Index < 2, "Index out of bounds");
            if constexpr (Index == 0) return std::forward<Self>(t).rseErrorWarp.value;
            if constexpr (Index == 1) return std::forward<Self>(t).rseErrorWarp;
        }

        RseErrorWarp rseErrorWarp;
    };
}

namespace std {
    template<typename T, typename ErrorType>
    struct tuple_size<RSE::RseBig<T, ErrorType>> : integral_constant<size_t, 2> {};

    template<size_t Index, typename T, typename ErrorType>
    struct tuple_element<Index, RSE::RseBig<T, ErrorType>>
        : tuple_element<Index, tuple<T, typename RSE::RseBig<T, ErrorType>::RseErrorWarp>>
    {};

    template<typename T, typename ErrorType>
    struct tuple_size<RSE::RseSmall<T, ErrorType>> : integral_constant<size_t, 2> {};

    template<size_t Index, typename T, typename ErrorType>
    struct tuple_element<Index, RSE::RseSmall<T, ErrorType>>
        : tuple_element<Index, tuple<T, typename RSE::RseSmall<T, ErrorType>::RseErrorWarp>>
    {};
}

template<typename T, typename ErrorType=std::string>
using Rse = typename std::conditional_t<sizeof(T) < 8, RSE::RseSmall<T, ErrorType>, RSE::RseBig<T, ErrorType>>;
