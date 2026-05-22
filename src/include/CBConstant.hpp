#pragma once
#include <Geode/Geode.hpp>

using namespace geode::prelude;
namespace comment {
    inline std::string baseUrl = "http://127.0.0.1:8010";

    inline std::string getResponseMessage(web::WebResponse const& response,
        std::string const& fallback) {
        auto message = response.string().unwrapOrDefault();
        if (!message.empty())
            return message;
        return fallback;
    }
};