#include "db/mysql_client.h"

#include <cppconn/driver.h>
#include <mysql_driver.h>

#include <sstream>

namespace db {

MysqlClient::MysqlClient(config::AuthServerConfig config)
    : config_(std::move(config)) {}

std::unique_ptr<sql::Connection> MysqlClient::create_connection() const {
    sql::Driver* driver = sql::mysql::get_mysql_driver_instance();

    std::ostringstream url;
    url << "tcp://" << config_.mysql_host << ":" << config_.mysql_port;

    std::unique_ptr<sql::Connection> conn(
        driver->connect(url.str(), config_.mysql_user, config_.mysql_password));
    conn->setSchema(config_.mysql_database);
    return conn;
}

}  // namespace db
