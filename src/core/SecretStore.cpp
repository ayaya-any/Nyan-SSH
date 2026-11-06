#include "core/SecretStore.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace nyan {
namespace {

#ifdef Q_OS_WIN
QByteArray entropy()
{
    return QByteArrayLiteral("NyanSSH.Secrets.v1");
}

DATA_BLOB toBlob(const QByteArray &bytes)
{
    DATA_BLOB blob{};
    blob.cbData = static_cast<DWORD>(bytes.size());
    blob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(bytes.constData()));
    return blob;
}
#endif

} // namespace

bool SecretStore::available()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString SecretStore::protect(const QString &plainText)
{
#ifdef Q_OS_WIN
    if (plainText.isEmpty())
        return {};

    const QByteArray utf8 = plainText.toUtf8();
    QByteArray entropyBytes = entropy();
    DATA_BLOB data = toBlob(utf8);
    DATA_BLOB entropyBlob = toBlob(entropyBytes);
    DATA_BLOB output{};

    const BOOL ok = ::CryptProtectData(&data, L"Nyan SSH credential", &entropyBlob, nullptr, nullptr,
                                       CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok)
        return {};

    const QByteArray encrypted(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
    ::LocalFree(output.pbData);
    return QString::fromLatin1(encrypted.toBase64());
#else
    Q_UNUSED(plainText);
    return {};
#endif
}

QString SecretStore::unprotect(const QString &base64Blob)
{
#ifdef Q_OS_WIN
    if (base64Blob.isEmpty())
        return {};

    const QByteArray encrypted = QByteArray::fromBase64(base64Blob.toLatin1());
    if (encrypted.isEmpty())
        return {};

    QByteArray entropyBytes = entropy();
    DATA_BLOB data = toBlob(encrypted);
    DATA_BLOB entropyBlob = toBlob(entropyBytes);
    DATA_BLOB output{};

    const BOOL ok = ::CryptUnprotectData(&data, nullptr, &entropyBlob, nullptr, nullptr,
                                         CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok)
        return {};

    const QByteArray plain(reinterpret_cast<const char *>(output.pbData), static_cast<int>(output.cbData));
    ::LocalFree(output.pbData);
    return QString::fromUtf8(plain);
#else
    Q_UNUSED(base64Blob);
    return {};
#endif
}

} // namespace nyan
