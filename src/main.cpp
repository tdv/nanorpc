#include <iostream>
#include <map>
#include <tuple>
#include <vector>
#include <any>

// BOOST
#include <boost/core/ignore_unused.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/preprocessor.hpp>

namespace nanorpc::version
{

using protocol = std::integral_constant<std::uint32_t, 1>;

}   // namespace nanorpc::version

namespace nanorpc::common::type
{

using id = std::size_t;
using buffer = std::vector<char>;

}   // namespace nanorpc::common::type

namespace nanorpc::packer::detail
{
inline namespace traits
{

template <typename T>
constexpr std::true_type is_iterable(T const &,
        typename T::iterator = typename std::decay<T>::type{}.begin(),
        typename T::iterator = typename std::decay<T>::type{}.end(),
        typename T::const_iterator = typename std::decay<T>::type{}.cbegin(),
        typename T::const_iterator = typename std::decay<T>::type{}.cend(),
        typename T::value_type = * typename std::decay<T>::type{}.begin()
    ) noexcept;

constexpr std::false_type is_iterable(...) noexcept;

template <typename T>
constexpr bool is_iterable_v = std::decay_t<decltype(is_iterable(*static_cast<T const *>(nullptr)))>::value;

template <typename ... T>
constexpr std::true_type is_tuple(std::tuple<T ... > const &) noexcept;

constexpr std::false_type is_tuple(...) noexcept;

template <typename T>
constexpr bool is_tuple_v = std::decay_t<decltype(is_tuple(*static_cast<T const *>(nullptr)))>::value;

struct dummy_type final
{
    template <typename T>
    constexpr operator T () noexcept
    {
        return *static_cast<T const *>(nullptr);
    }
};

template <typename T, typename ... TArgs>
constexpr decltype(void(T{std::declval<TArgs>() ... }), std::declval<std::true_type>())
is_braces_constructible(std::size_t) noexcept;

template <typename, typename ... >
constexpr std::false_type is_braces_constructible(...) noexcept;

template <typename T, typename ... TArgs>
constexpr bool is_braces_constructible_v = std::decay_t<decltype(is_braces_constructible<T, TArgs ... >(0))>::value;

}   // inline namespace traits
}   // namespace nanorpc::packer::detail

namespace nanorpc::packer::detail
{

template <typename T>
auto to_tuple(T &&value)
{
    using type = std::decay_t<T>;

#define NANORPC_TO_TUPLE_LIMIT_FIELDS 64 // you can try to use BOOST_PP_LIMIT_REPEAT

#define NANORPC_TO_TUPLE_DUMMY_TYPE_N(_, n, data) \
    BOOST_PP_COMMA_IF(n) data

#define NANORPC_TO_TUPLE_PARAM_N(_, n, data) \
    BOOST_PP_COMMA_IF(n) data ## n

#define NANORPC_TO_TUPLE_ITEM_N(_, n, __) \
    if constexpr (is_braces_constructible_v<type, \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_DUMMY_TYPE_N, dummy_type) \
    >) { auto &&[ \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_PARAM_N, f) \
    ] = value; return std::make_tuple( \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_PARAM_N, f) \
    ); } else

#define NANORPC_TO_TUPLE_ITEMS(n) \
    BOOST_PP_REPEAT_FROM_TO(0, n, NANORPC_TO_TUPLE_ITEM_N, nil)

    NANORPC_TO_TUPLE_ITEMS(NANORPC_TO_TUPLE_LIMIT_FIELDS)
    {
        return std::make_tuple();
    }

#undef NANORPC_TO_TUPLE_ITEMS
#undef NANORPC_TO_TUPLE_ITEM_N
#undef NANORPC_TO_TUPLE_PARAM_N
#undef NANORPC_TO_TUPLE_DUMMY_TYPE_N
#undef NANORPC_TO_TUPLE_LIMIT_FIELDS
}

}

namespace nanorpc::packer
{

class plain_text final
{
private:
    class serializer;
    class deserializer;

public:
    using serializer_type = serializer;
    using deserializer_type = deserializer;

    template <typename T>
    serializer pack(T const &value)
    {
        return serializer{}.pack(value);
    }

    deserializer from_buffer(common::type::buffer buffer)
    {
        return deserializer{std::move(buffer)};
    }

private:
    class serializer final
    {
    public:
        serializer(serializer &&) noexcept = default;
        serializer& operator = (serializer &&) noexcept = default;
        ~serializer() noexcept = default;

        template <typename T>
        serializer pack(T const &value)
        {
            assert(stream_ && "Empty stream.");
            if (!stream_)
                throw std::runtime_error{"[" + std::string{__func__ } + "] Empty stream."};

            pack_value(value);
            return std::move(*this);
        }

        common::type::buffer to_buffer()
        {
            assert(stream_ && "Empty stream.");
            if (!stream_)
                throw std::runtime_error{"[" + std::string{__func__ } + "] Empty stream."};

            stream_.reset();
            auto tmp = std::move(*buffer_);
            buffer_.reset();
            return tmp;
        }

    private:
        using buffer_ptr = std::unique_ptr<common::type::buffer>;
        using stream_type = boost::iostreams::filtering_ostream;
        using stream_type_ptr = std::unique_ptr<stream_type>;

        buffer_ptr buffer_{std::make_unique<common::type::buffer>()};
        stream_type_ptr stream_{std::make_unique<stream_type>(boost::iostreams::back_inserter(*buffer_))};

        friend class plain_text;
        serializer() = default;

        serializer(serializer const &) = delete;
        serializer& operator = (serializer const &) = delete;

        template <typename T>
        static constexpr auto is_serializable(T &value) noexcept ->
                decltype(*static_cast<std::ostream *>(nullptr) << value, std::declval<std::true_type>());
        static constexpr std::false_type is_serializable(...) noexcept;
        template <typename T>
        static constexpr bool is_serializable_v = std::decay_t<decltype(is_serializable(*static_cast<T *>(nullptr)))>::value;

        template <typename T>
        std::enable_if_t<is_serializable_v<T> , void>
        pack_value(T const &value)
        {
            *stream_ << value << ' ';
        }

        template <typename T>
        std::enable_if_t<!is_serializable_v<T> && detail::traits::is_tuple_v<T>, void>
        pack_value(T const &value)
        {
            pack_tuple(value, std::make_index_sequence<std::tuple_size_v<T>>{});
        }

        template <typename T>
        std::enable_if_t<!is_serializable_v<T> && detail::traits::is_iterable_v<T>, void>
        pack_value(T const &value)
        {
            pack_value(value.size());
            for (auto const &i : value)
                pack_value(i);
        }

        template <typename T>
        std::enable_if_t
            <
                std::tuple_size_v<std::decay_t<decltype(detail::to_tuple(std::declval<std::decay_t<T>>()))>> != 0,
                void
            >
        pack_user_defined_type(T const &value)
        {
            pack_value(detail::to_tuple(value));
        }

        template <typename T>
        std::enable_if_t
            <
                !is_serializable_v<T> && !detail::traits::is_iterable_v<T> &&
                    !detail::is_tuple_v<T> && std::is_class_v<T>,
                void
            >
        pack_value(T const &value)
        {
            pack_user_defined_type(value);
        }

        template <typename ... T, std::size_t ... I>
        void pack_tuple(std::tuple<T ... > const &tuple, std::index_sequence<I ... >)
        {
            auto pack_tuple_item = [this] (auto value)
                {
                    pack_value(value);
                    *stream_ << ' ';
                };
            boost::ignore_unused(pack_tuple_item);
            (pack_tuple_item(std::get<I>(tuple)) , ... );
        }
    };

    class deserializer final
    {
    public:
        deserializer(deserializer &&) noexcept = default;
        deserializer& operator = (deserializer &&) noexcept = default;
        ~deserializer() noexcept = default;

        template <typename T>
        deserializer unpack(T &value)
        {
            assert(stream_ && "Empty stream.");
            if (!stream_)
                throw std::runtime_error{"[" + std::string{__func__ } + "] Empty stream."};

            unpack_value(value);
            return std::move(*this);
        }

    private:
        using buffer_ptr = std::unique_ptr<common::type::buffer>;
        using source_type = boost::iostreams::basic_array_source<char>;
        using source_type_ptr = std::unique_ptr<source_type>;
        using stream_type = boost::iostreams::stream<source_type>;
        using stream_type_ptr = std::unique_ptr<stream_type>;

        buffer_ptr buffer_;
        source_type_ptr source_{std::make_unique<source_type>(!buffer_->empty() ? buffer_->data() : nullptr, buffer_->size())};
        stream_type_ptr stream_{std::make_unique<stream_type>(*source_)};

        friend class plain_text;

        deserializer(deserializer const &) = delete;
        deserializer& operator = (deserializer const &) = delete;

        deserializer(common::type::buffer buffer)
            : buffer_{std::make_unique<common::type::buffer>(std::move(buffer))}
        {
        }

        template <typename T>
        static constexpr auto is_deserializable(T &value) noexcept ->
                decltype(*static_cast<std::istream *>(nullptr) >> value, std::declval<std::true_type>());
        static constexpr std::false_type is_deserializable(...) noexcept;
        template <typename T>
        static constexpr bool is_deserializable_v = std::decay_t<decltype(is_deserializable(*static_cast<std::decay_t<T> *>(nullptr)))>::value;

        template <typename T>
        std::enable_if_t<is_deserializable_v<T> , void>
        unpack_value(T &value)
        {
            (void)value;
            *stream_ >> value;
        }

        template <typename T>
        std::enable_if_t<!is_deserializable_v<T> && detail::traits::is_tuple_v<T>, void>
        unpack_value(T &value)
        {
            unpack_tuple(value, std::make_index_sequence<std::tuple_size_v<T>>{});
        }

        template <typename K, typename V>
        void unpack_value(std::pair<K const, V> &value)
        {
            using key_type = std::decay_t<K>;
            std::pair<key_type, V> pair;
            unpack_value(pair.first);
            unpack_value(pair.second);
            std::exchange(const_cast<key_type &>(value.first), pair.first);
            std::exchange(value.second, pair.second);
        }

        template <typename T>
        std::enable_if_t<!is_deserializable_v<T> && detail::traits::is_iterable_v<T>, void>
        unpack_value(T &value)
        {
            using size_type = typename std::decay_t<T>::size_type;
            using value_type = typename std::decay_t<T>::value_type;
            size_type count{};
            unpack_value(count);
            for (size_type i = 0 ; i < count ; ++i)
            {
                value_type item;
                unpack_value(item);
                *std::inserter(value, end(value)) = std::move(item);
            }
        }

        template <typename T, typename Tuple, std::size_t ... I>
        T make_from_tuple(Tuple && tuple, std::index_sequence<I ... >)
        {
            return T{std::move(std::get<I>(std::forward<Tuple>(tuple))) ... };
        }

        template <typename T>
        std::enable_if_t
            <
                std::tuple_size_v<std::decay_t<decltype(detail::to_tuple(std::declval<std::decay_t<T>>()))>> != 0,
                void
            >
        unpack_user_defined_type(T &value)
        {
            using tuple_type = std::decay_t<decltype(detail::to_tuple(value))>;
            tuple_type tuple;
            unpack_value(tuple);
            value = make_from_tuple<std::decay_t<T>>(std::move(tuple),
                    std::make_index_sequence<std::tuple_size_v<tuple_type>>{});
        }

        template <typename T>
        std::enable_if_t
            <
                !is_deserializable_v<T> && !detail::traits::is_iterable_v<T> &&
                    !detail::is_tuple_v<T> && std::is_class_v<T>,
                void
            >
        unpack_value(T &value)
        {
            unpack_user_defined_type(value);
        }

        template <typename ... T, std::size_t ... I>
        void unpack_tuple(std::tuple<T ... > &tuple, std::index_sequence<I ... >)
        {
            (unpack_value(std::get<I>(tuple)) , ... );
        }
    };

};

}   // namespace nanorpc::packer

namespace nanorpc::core::detail
{

template <typename>
struct function_meta;

template <typename R, typename ... T>
struct function_meta<std::function<R (T ... )>>
{
    using return_type = std::decay_t<R>;
    using arguments_tuple_type = std::tuple<std::decay_t<T> ... >;
};

}   // namespace nanorpc::core::detail

namespace nanorpc::core
{

template <typename TPacker>
class server final
{
public:
    template <typename TFunc>
    void handle(std::string_view name, TFunc func)
    {
        handle(std::hash<std::string_view>{}(name), std::move(func));
    }

    template <typename TFunc>
    void handle(common::type::id id, TFunc func)
    {
        if (handlers_.find(id) != end(handlers_))
        {
            throw std::invalid_argument{"[" + std::string{__func__ } + "] Failed to add handler. "
                    "The id \"" + std::to_string(id) + "\" already exists."};
        }

        auto wrapper = [f = std::move(func)] (deserializer_type &request, serializer_type &response)
            {
                std::function func{std::move(f)};
                using function_meta = detail::function_meta<decltype(func)>;
                using arguments_tuple_type = typename function_meta::arguments_tuple_type;
                arguments_tuple_type data;
                request.unpack(data);
                apply(std::move(func), std::move(data), response);
            };

        handlers_.emplace(std::move(id), std::move(wrapper));
    }

    common::type::buffer execute(common::type::buffer buffer)
    {
        if (handlers_.empty())
            throw std::runtime_error{"[" + std::string{__func__ } + "] No handlers."};

        packer_type packer;

        using meta_type = std::tuple<version::protocol::value_type, common::type::id>;
        meta_type meta;
        auto request = packer.from_buffer(std::move(buffer)).unpack(meta);

        auto const protocol_version = std::get<0>(meta);
        if (protocol_version != version::protocol::value)
        {
            throw std::runtime_error{"[" + std::string{__func__ } + "] Failed to process data. "
                    "Protocol \"" + std::to_string(protocol_version) + "\" not supported."};
        }

        auto const function_id = std::get<1>(meta);
        auto const iter = handlers_.find(function_id);
        if (iter == end(handlers_))
        {
            throw std::runtime_error{"[" + std::string{__func__ } + "] Function with id "
                    "\"" + std::to_string(function_id) + "\" not found."};
        }

        auto response = packer.pack(meta);
        iter->second(request, response);

        return response.to_buffer();
    }

private:
    using packer_type = TPacker;
    using serializer_type = typename packer_type::serializer_type;
    using deserializer_type = typename packer_type::deserializer_type;
    using handler_type = std::function<void (deserializer_type &, serializer_type &)>;
    using handlers_type = std::map<common::type::id, handler_type>;

    handlers_type handlers_;

    template <typename TFunc, typename TArgs>
    static
    std::enable_if_t<!std::is_same_v<std::decay_t<decltype(std::apply(std::declval<TFunc>(), std::declval<TArgs>()))>, void>, void>
    apply(TFunc func, TArgs args, serializer_type &serializer)
    {
        auto data = std::apply(std::move(func), std::move(args));
        serializer = serializer.pack(data);
    }

    template <typename TFunc, typename TArgs>
    static
    std::enable_if_t<std::is_same_v<std::decay_t<decltype(std::apply(std::declval<TFunc>(), std::declval<TArgs>()))>, void>, void>
    apply(TFunc func, TArgs args, serializer_type &serializer)
    {
        boost::ignore_unused(serializer);
        std::apply(std::move(func), std::move(args));
    }
};

}   // namespace nanorpc::core

namespace nanorpc::core
{

template <typename TPacker>
class client final
{
private:
    class result;

public:
    using sender_type = std::function<common::type::buffer (common::type::buffer)>;

    client(sender_type sender)
        : sender_{std::move(sender)}
    {
    }

    template <typename ... TArgs>
    result call(std::string_view name, TArgs && ... args)
    {
        return call(std::hash<std::string_view>{}(name), std::forward<TArgs>(args) ... );
    }

    template <typename ... TArgs>
    result call(common::type::id id, TArgs && ... args)
    {
        auto meta = std::make_tuple(version::protocol::value, std::move(id));
        auto data = std::make_tuple(std::forward<TArgs>(args) ... );

        packer_type packer;
        auto request = packer
                .pack(meta)
                .pack(data)
                .to_buffer();

        auto buffer = sender_(std::move(request));
        auto response = packer.from_buffer(std::move(buffer));
        decltype(meta) response_meta;
        response = response.unpack(response_meta);
        if (meta != response_meta)
            throw std::runtime_error{"[" + std::string{__func__ } + "] The meta in the response is bad."};

        return {std::move(response)};
    }

private:
    using packer_type = TPacker;
    using deserializer_type = typename packer_type::deserializer_type;

    sender_type sender_;

    class result final
    {
    public:
        result(result &&) noexcept = default;
        result& operator = (result &&) noexcept = default;
        ~result() noexcept = default;

        template <typename T>
        T as() const
        {
            if (!value_ && !deserializer_)
                throw std::runtime_error{"[" + std::string{__func__ } + "] No data."};

            using Type = std::decay_t<T>;

            if (!value_)
            {
                 if (!deserializer_)
                     throw std::runtime_error{"[" + std::string{__func__ } + "] No data."};

                 Type data{};
                 deserializer_->unpack(data);

                 value_ = std::move(data);
                 deserializer_.reset();
            }

            return std::any_cast<Type>(*value_);
        }

        template <typename T>
        operator T () const
        {
            return as<T>();
        }

    private:
        template <typename>
        friend class client;

        mutable std::optional<deserializer_type> deserializer_;
        mutable std::optional<std::any> value_;

        result(deserializer_type deserializer)
            : deserializer_{std::move(deserializer)}
        {
        }

        result(result const &) = delete;
        result& operator = (result const &) = delete;
    };
};

}   // namespace nanorpc::core

int main()
{
    try
    {
        nanorpc::core::server<nanorpc::packer::plain_text> server;
        server.handle("test", [] (std::string const &s) { std::cout << "Param: " << s << std::endl; } );

        auto sender = [&] (nanorpc::common::type::buffer request)
            {
                return server.execute(std::move(request));
            };

        nanorpc::core::client<nanorpc::packer::plain_text> client(std::move(sender));
        client.call("test", std::string{"test"});
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
