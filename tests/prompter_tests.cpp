#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include "backend.h"
#include "renderer.h"
#include "theme.h"

class PrompterTests : public QObject {
    Q_OBJECT

private slots:
    void markdownKeepsLineBreaks()
    {
        QCOMPARE(Renderer::toMarkdown("one\ntwo\n\nthree"), QString("one  \ntwo  \n\nthree  "));
    }

    void rendererFillsFrame()
    {
        Renderer renderer;
        renderer.setFrameSize({1024, 600});
        renderer.setText("Hello there");
        QImage frame;
        renderer.paint(frame, 0, false);
        QCOMPARE(frame.size(), QSize(1024, 600));
        QCOMPARE(frame.format(), QImage::Format_RGB32);
        // Some white text should be on the reading line.
        bool sawText = false;
        const int y = 600 * 0.33;
        for (int x = 0; x < 1024 && !sawText; ++x)
            for (int dy = -10; dy <= 10 && !sawText; ++dy)
                sawText = qGray(frame.pixel(x, y + dy)) > 200;
        QVERIFY(sawText);
    }

    void scrollsAtSpeedAndStopsAtEnd()
    {
        QTemporaryDir dir;
        Backend backend(nullptr, dir.path());
        backend.setText(QString("line\n").repeated(40));
        backend.setSpeed(60);
        backend.togglePlay();
        QVERIFY(backend.playing());
        backend.advance(1.0);
        QVERIFY(backend.offset() > 0);
        backend.advance(1000.0);
        QCOMPARE(backend.progress(), 1.0);
        QVERIFY(!backend.playing());
        // Rolling again from the end starts over.
        backend.togglePlay();
        QCOMPARE(backend.offset(), 0.0);
    }

    void settingsAreClampedAndPersist()
    {
        QTemporaryDir dir;
        {
            Backend backend(nullptr, dir.path());
            backend.setSpeed(100000);
            backend.setFontSize(1);
            QCOMPARE(backend.mirror(), false);
            backend.setMirror(true);
            QCOMPARE(backend.speed(), int(Backend::MaxSpeed));
            QCOMPARE(backend.fontSize(), int(Backend::MinFontSize));
        }
        Backend again(nullptr, dir.path());
        QCOMPARE(again.speed(), int(Backend::MaxSpeed));
        QCOMPARE(again.mirror(), true);
    }

    void draftSurvivesRestartAndOriginalsAreUntouched()
    {
        QTemporaryDir dir;
        const QString original = dir.path() + "/talk.md";
        QFile file(original);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("original");
        file.close();
        {
            Backend backend(nullptr, dir.path() + "/state");
            backend.openFile(QUrl::fromLocalFile(original));
            QCOMPARE(backend.text(), QString("original"));
            backend.setText("edited");
        }
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original"));
        Backend again(nullptr, dir.path() + "/state");
        QCOMPARE(again.text(), QString("edited"));
    }

    void themeFallsBack()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
        QCOMPARE(theme.accentForeground(), QString("black"));
    }

    void themeReadsAccent()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        QFile colors(dir.path() + "/theme/colors.toml");
        QVERIFY(colors.open(QIODevice::WriteOnly));
        colors.write("accent = \"#112233\"\n");
        colors.close();
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#112233"));
        QCOMPARE(theme.accentForeground(), QString("white"));
    }
};

QTEST_MAIN(PrompterTests)
#include "prompter_tests.moc"
