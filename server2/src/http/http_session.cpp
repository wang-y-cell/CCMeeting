#include "http/http_session.h"

#include "util/json_helper.h"

#include <spdlog/spdlog.h>

namespace http_api {

namespace {

http::response<http::string_body> make_json_response(
    http::status status,
    const std::string& body,
    unsigned version,
    bool keep_alive) {
    http::response<http::string_body> res{status, version};
    res.set(http::field::server, "CloudMeetingAuthServer");
    res.set(http::field::content_type, "application/json; charset=utf-8");
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_headers, "Content-Type");
    res.set(http::field::access_control_allow_methods, "POST, OPTIONS");
    res.keep_alive(keep_alive);
    res.body() = body;
    res.prepare_payload();
    return res;
}

}  // namespace

HttpSession::HttpSession(tcp::socket socket,
                         std::shared_ptr<service::AuthService> auth_service)
    : stream_(std::move(socket))
    , auth_service_(std::move(auth_service)) {}

void HttpSession::run() {
    do_read();
}

void HttpSession::do_read() {
    request_ = {};
    http::async_read(
        stream_,
        buffer_,
        request_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t) {
    if (ec == http::error::end_of_stream) {
        beast::error_code ignored;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
        return;
    }
    if (ec) {
        spdlog::warn("[HttpSession] read error: {}", ec.message());
        return;
    }

    handle_request(std::move(request_));
}

void HttpSession::handle_request(http::request<http::string_body>&& req) {
    const auto version = req.version();
    const bool keep_alive = req.keep_alive();
    const std::string target(req.target());

    if (req.method() == http::verb::options) {
        send_response(make_json_response(http::status::no_content, "", version, keep_alive));
        return;
    }

    if (req.method() == http::verb::get && (target == "/health" || target == "/api/health")) {
        send_response(make_json_response(
            http::status::ok, R"({"code":0,"message":"ok"})", version, keep_alive));
        return;
    }

    if (req.method() == http::verb::post && target == "/api/login") {
        std::string username;
        std::string password;
        if (!util::parse_login_request(req.body(), username, password)) {
            model::LoginResult bad;
            bad.code = 400;
            bad.message = "invalid json body";
            send_response(make_json_response(
                http::status::bad_request,
                util::to_login_response_json(bad),
                version,
                keep_alive));
            return;
        }

        std::string client_ip = "unknown";
        try {
            client_ip = stream_.socket().remote_endpoint().address().to_string();
        } catch (...) {
        }

        std::string device_info;
        if (const auto it = req.find(http::field::user_agent); it != req.end()) {
            device_info = std::string(it->value());
        }

        const model::LoginResult result =
            auth_service_->login(username, password, client_ip, device_info);

        http::status status = http::status::unauthorized;
        if (result.success) {
            status = http::status::ok;
        } else if (result.code == 400) {
            status = http::status::bad_request;
        } else if (result.code == 403) {
            status = http::status::forbidden;
        } else if (result.code == 500) {
            status = http::status::internal_server_error;
        }

        send_response(make_json_response(
            status, util::to_login_response_json(result), version, keep_alive));
        return;
    }

    model::LoginResult not_found;
    not_found.code = 404;
    not_found.message = "not found";
    send_response(make_json_response(
        http::status::not_found,
        util::to_login_response_json(not_found),
        version,
        keep_alive));
}

void HttpSession::send_response(http::response<http::string_body>&& res) {
    auto sp = std::make_shared<http::response<http::string_body>>(std::move(res));
    const bool keep_alive = sp->keep_alive();

    http::async_write(
        stream_,
        *sp,
        [self = shared_from_this(), sp, keep_alive](beast::error_code ec, std::size_t) {
            if (ec) {
                spdlog::warn("[HttpSession] write error: {}", ec.message());
                return;
            }
            if (!keep_alive) {
                beast::error_code ignored;
                self->stream_.socket().shutdown(tcp::socket::shutdown_send, ignored);
                return;
            }
            self->do_read();
        });
}

}  // namespace http_api
