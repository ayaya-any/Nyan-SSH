#include "core/HostKeyStore.h"
#include "core/SecretStore.h"
#include "core/SessionProfile.h"
#include "core/SessionStore.h"
#include "core/SftpTypes.h"
#include "term/TerminalScreen.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace nyan;
using namespace nyan::term;

class CoreTests : public QObject {
    Q_OBJECT
private slots:
    void secretStoreRoundTrip()
    {
        if (!SecretStore::available())
            QSKIP("DPAPI unavailable on this platform");
        const QString plain = QStringLiteral("猫猫密码 nyaa-123");
        const QString blob = SecretStore::protect(plain);
        QVERIFY(!blob.isEmpty());
        QCOMPARE(SecretStore::unprotect(blob), plain);
        QVERIFY(SecretStore::unprotect(QStringLiteral("not-base64")).isEmpty());
    }

    void profilePersistsOnlyRememberedPassword()
    {
        SessionProfile profile;
        profile.name = QStringLiteral("测试");
        profile.host = QStringLiteral("example.com");
        profile.port = 2222;
        profile.username = QStringLiteral("nyan");
        profile.setPassword(QStringLiteral("secret"), true);
        profile.autoLogin = true;

        const QJsonObject object = profile.toJson();
        QVERIFY(!object.value(QStringLiteral("password")).toString().isEmpty());

        SessionProfile restored = SessionProfile::fromJson(object);
        QCOMPARE(restored.password(), QStringLiteral("secret"));
        QCOMPARE(restored.displayName(), QStringLiteral("测试"));
        QCOMPARE(restored.port, quint16(2222));

        profile.setPassword(QStringLiteral("not-persisted"), false);
        QVERIFY(profile.toJson().value(QStringLiteral("password")).toString().isEmpty());
        QCOMPARE(profile.password(), QStringLiteral("not-persisted"));
    }

    void storeRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("sessions.json"));

        SessionStore store(path);
        SessionProfile profile;
        profile.host = QStringLiteral("127.0.0.1");
        profile.username = QStringLiteral("nyan");
        profile.name = QStringLiteral("本地");
        store.upsert(profile);
        QString error;
        QVERIFY2(store.save(&error), qPrintable(error));

        SessionStore loaded(path);
        QVERIFY2(loaded.load(&error), qPrintable(error));
        QCOMPARE(loaded.profiles().size(), 1);
        QCOMPARE(loaded.profiles().first().host, QStringLiteral("127.0.0.1"));
        QCOMPARE(loaded.profiles().first().id, profile.id);
    }

    void hostKeyStoreRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("known_hosts.json"));
        HostKeyStore store(path);
        store.trust(QStringLiteral("Example.COM"), 2222, QStringLiteral("ssh-ed25519"),
                    QStringLiteral("SHA256:abc"));
        QString error;
        QVERIFY2(store.save(&error), qPrintable(error));

        HostKeyStore loaded(path);
        QVERIFY2(loaded.load(&error), qPrintable(error));
        const auto lookup = loaded.lookup(QStringLiteral("example.com"), 2222, QStringLiteral("SHA256:abc"));
        QVERIFY(lookup.known);
        QVERIFY(lookup.matches);
        QVERIFY(!loaded.lookup(QStringLiteral("example.com"), 22, QStringLiteral("SHA256:abc")).known);
    }

    void remotePaths()
    {
        QCOMPARE(normalizeRemotePath(QStringLiteral("/a/./b/../c")), QStringLiteral("/a/c"));
        QCOMPARE(joinRemotePath(QStringLiteral("/home/nyan"), QStringLiteral("x")),
                 QStringLiteral("/home/nyan/x"));
        QCOMPARE(parentRemotePath(QStringLiteral("/home/nyan/x")), QStringLiteral("/home/nyan"));
        QCOMPARE(parentRemotePath(QStringLiteral("/")), QStringLiteral("/"));
        QCOMPARE(normalizeRemotePath(QStringLiteral("//a///b/")), QStringLiteral("/a/b"));
    }

    void screenBasicText()
    {
        TerminalScreen screen(20, 6);
        screen.feed("hello\r\nworld");
        QCOMPARE(screen.lineText(0), QStringLiteral("hello"));
        QCOMPARE(screen.lineText(1), QStringLiteral("world"));
        QCOMPARE(screen.cursorRow(), 1);
        QCOMPARE(screen.cursorCol(), 5);
    }

    void screenSgrColors()
    {
        TerminalScreen screen(20, 4);
        screen.feed("\x1b[1;31mred\x1b[0m plain\x1b[38;5;196mX\x1b[48;2;10;20;30mY");
        const int top = screen.screenTop();
        QCOMPARE(screen.cellAt(top, 0).fg, 1u);
        QVERIFY(screen.cellAt(top, 0).flags & Bold);
        QCOMPARE(screen.cellAt(top, 4).fg, DefaultColor);
        QCOMPARE(screen.cellAt(top, 9).fg, 196u);
        const Cell &y = screen.cellAt(top, 10);
        QCOMPARE(y.fg, 196u);
        QCOMPARE(y.bg, RgbColorFlag | (10u << 16) | (20u << 8) | 30u);
    }

    void screenEraseAndCursor()
    {
        TerminalScreen screen(10, 4);
        screen.feed("abcdef\x1b[2J\x1b[Hxy");
        QCOMPARE(screen.lineText(0), QStringLiteral("xy"));
        QCOMPARE(screen.cursorRow(), 0);
        QCOMPARE(screen.cursorCol(), 2);
        screen.feed("\x1b[3;4HZ");
        QCOMPARE(screen.lineText(2), QStringLiteral("   Z"));
    }

    void screenScrollback()
    {
        TerminalScreen screen(20, 4);
        screen.setMaxScrollback(100);
        for (int i = 0; i < 10; ++i)
            screen.feed(QStringLiteral("line %1\r\n").arg(i).toUtf8());
        QCOMPARE(screen.rows(), 4);
        QCOMPARE(screen.scrollbackLines(), 7);
        QCOMPARE(screen.lineText(screen.totalLines() - 2), QStringLiteral("line 9"));
        QCOMPARE(screen.lineText(screen.totalLines() - 3), QStringLiteral("line 8"));
    }

    void screenWideCharacters()
    {
        TerminalScreen screen(10, 4);
        screen.feed(QStringLiteral("猫a").toUtf8());
        const int top = screen.screenTop();
        QCOMPARE(screen.cellAt(top, 0).ch, char32_t(0x732B));
        QCOMPARE(screen.cellAt(top, 0).width, quint8(2));
        QCOMPARE(screen.cellAt(top, 1).width, quint8(0));
        QCOMPARE(screen.cellAt(top, 2).ch, char32_t(U'a'));
        QCOMPARE(screen.cursorCol(), 3);
    }

    void screenAlternateBuffer()
    {
        TerminalScreen screen(20, 5);
        screen.feed("primary");
        screen.feed("\x1b[?1049h");
        QVERIFY(screen.isAltScreen());
        screen.feed("alternate");
        QCOMPARE(screen.lineText(0), QStringLiteral("alternate"));
        screen.feed("\x1b[?1049l");
        QVERIFY(!screen.isAltScreen());
        QCOMPARE(screen.lineText(0), QStringLiteral("primary"));
        QCOMPARE(screen.cursorCol(), 7);
    }

    void screenTitleAndResponses()
    {
        TerminalScreen screen(20, 5);
        screen.feed("\x1b]0;nyan@test\x07");
        QString title;
        QVERIFY(screen.takeTitleChanged(&title));
        QCOMPARE(title, QStringLiteral("nyan@test"));
        screen.feed("\x1b[6n");
        QCOMPARE(screen.takeResponse(), QByteArray("\x1b[1;1R"));
        screen.feed("abc\x1b[6n");
        QCOMPARE(screen.takeResponse(), QByteArray("\x1b[1;4R"));
    }

    void screenWrap()
    {
        TerminalScreen screen(5, 3);
        screen.feed("abcdef");
        QCOMPARE(screen.lineText(0), QStringLiteral("abcde"));
        QCOMPARE(screen.lineText(1), QStringLiteral("f"));
        QCOMPARE(screen.cursorRow(), 1);
        QCOMPARE(screen.cursorCol(), 1);
    }

    void screenInsertDeleteLines()
    {
        TerminalScreen screen(10, 6);
        screen.feed("one\r\ntwo\r\nthree\r\nfour\r\nfive");
        screen.feed("\x1b[2;1H\x1b[L");
        QCOMPARE(screen.lineText(1), QString());
        QCOMPARE(screen.lineText(2), QStringLiteral("two"));
        QCOMPARE(screen.lineText(5), QStringLiteral("five"));
        screen.feed("\x1b[2;1H\x1b[M");
        QCOMPARE(screen.lineText(1), QStringLiteral("two"));
        QCOMPARE(screen.lineText(2), QStringLiteral("three"));
        QCOMPARE(screen.lineText(3), QStringLiteral("four"));
        QCOMPARE(screen.lineText(4), QStringLiteral("five"));
    }
};

QTEST_MAIN(CoreTests)
#include "test_main.moc"
