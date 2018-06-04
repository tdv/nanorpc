//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     06.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __EXAMPLES_SSL_HELLO_WORLD_H__
#define __EXAMPLES_SSL_HELLO_WORLD_H__

// STD
#include <string>

// BOOST
#include <boost/asio/ssl/context.hpp>

inline boost::asio::ssl::context prepare_ssl_context(std::string const &cert_file_name,
        std::string const &key_file_name, std::string const &db_file_name)
{
    boost::asio::ssl::context context{boost::asio::ssl::context::sslv23};
    context.set_options(boost::asio::ssl::context::default_workarounds
            | boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::single_dh_use);
    context.set_password_callback(std::bind([] { return std::string{"test"}; } ));
    context.use_certificate_chain_file(cert_file_name);
    context.use_private_key_file(key_file_name, boost::asio::ssl::context::pem);
    context.use_tmp_dh_file(db_file_name);
    return context;
}

#endif  // !__EXAMPLES_SSL_HELLO_WORLD_H__
