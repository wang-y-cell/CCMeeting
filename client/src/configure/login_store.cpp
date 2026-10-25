#include "login_store.h"

#include <QByteArray>
#include "storage.h"

#include <spdlog/spdlog.h>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

constexpr const char *kOrg = "CCMeeting";
constexpr const char *kApp = "Client";
constexpr const char *kKeyUser = "login/username";
constexpr const char *kKeyPass = "login/password";
constexpr const char *kKeyAuto = "login/auto";

}  // namespace

QString LoginStore::encrypt(const QString &plain) {
#ifdef Q_OS_WIN
    if (plain.isEmpty()) {
        return QString();
    }
    const QByteArray utf8 = plain.toUtf8();
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(utf8.constData()));
    in.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB out{};
    // 按当前登录用户加密：换用户或换机器都解不开
    if (!CryptProtectData(&in, L"CCMeeting login", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        spdlog::warn("[LoginStore] CryptProtectData failed err={}",
                     static_cast<unsigned long>(GetLastError()));
        return QString();
    }
    const QByteArray cipher(reinterpret_cast<const char *>(out.pbData),
                            static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return QString::fromLatin1(cipher.toBase64());
#else
    spdlog::warn("[LoginStore] 非 Windows 平台，密码仅做 Base64 处理");
    return QString::fromLatin1(plain.toUtf8().toBase64());
#endif
}

QString LoginStore::decrypt(const QString &cipher) {
    if (cipher.isEmpty()) {
        return QString();
    }
    const QByteArray raw =
        QByteArray::fromBase64(cipher.toLatin1());
#ifdef Q_OS_WIN
    DATA_BLOB in{};
    in.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(raw.constData()));
    in.cbData = static_cast<DWORD>(raw.size());

    DATA_BLOB out{};
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        spdlog::warn("[LoginStore] CryptUnprotectData failed err={}",
                     static_cast<unsigned long>(GetLastError()));
        return QString();
    }
    const QString plain = QString::fromUtf8(
        reinterpret_cast<const char *>(out.pbData), static_cast<int>(out.cbData));
    LocalFree(out.pbData);
    return plain;
#else
    return QString::fromUtf8(raw);
#endif
}

bool LoginStore::has_credentials() { return Storage::instance().hasCredentials(); }

LoginStore::Credentials LoginStore::load() {
    Credentials creds;
    Storage::Credentials stored;
    if (!Storage::instance().loadCredentials(stored)) {
        return creds;  // 无凭据：autoLogin 保持 false
    }
    creds.username = stored.username;
    creds.password = decrypt(stored.secret);
    creds.autoLogin = !creds.username.isEmpty() && !creds.password.isEmpty();
    if (!creds.autoLogin) {
        spdlog::warn("[LoginStore] 凭据解密失败，需重新登录");
    }
    return creds;
}

bool LoginStore::save(const QString &username, const QString &password) {
    if (username.isEmpty() || password.isEmpty()) {
        return false;
    }
    const QString cipher = encrypt(password);
    if (cipher.isEmpty()) {
        spdlog::warn("[LoginStore] 密码加密失败，不保存凭据");
        return false;
    }
    if (!Storage::instance().saveCredentials(username, cipher)) {
        return false;
    }
    spdlog::info("[LoginStore] credentials saved for user={}",
                 username.toUtf8().constData());
    return true;
}

void LoginStore::clear() {
    Storage::instance().clearCredentials();
    spdlog::info("[LoginStore] credentials cleared (auto login off)");
}
