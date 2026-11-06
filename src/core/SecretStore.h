#pragma once

#include <QString>

namespace nyan {

/// Windows DPAPI-backed secret storage.  Secrets never touch disk in plain
/// text; the resulting base64 blob is tied to the current Windows user.
class SecretStore {
public:
    static bool available();

    /// Encrypt a UTF-8 string.  Returns an empty string on failure.
    static QString protect(const QString &plainText);

    /// Decrypt a blob previously produced by protect().  Returns an empty
    /// string when the blob is invalid or belongs to another user/machine.
    static QString unprotect(const QString &base64Blob);
};

} // namespace nyan
