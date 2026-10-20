#pragma once

#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QUrl>

#include "renderer.h"

class DrmOutput;

// The prompter's state and frame loop. Frames are paced by the Prompter's page
// flips while it's connected, and by a timer otherwise so the preview still
// runs. The output is optional so tests never touch real hardware.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY settingsChanged)
    Q_PROPERTY(int fontSize READ fontSize WRITE setFontSize NOTIFY settingsChanged)
    Q_PROPERTY(int margin READ margin WRITE setMargin NOTIFY settingsChanged)
    Q_PROPERTY(double readingLine READ readingLine WRITE setReadingLine NOTIFY settingsChanged)
    Q_PROPERTY(bool mirror READ mirror WRITE setMirror NOTIFY settingsChanged)
    Q_PROPERTY(double progress READ progress NOTIFY frameChanged)
    Q_PROPERTY(bool outputConnected READ outputConnected NOTIFY outputChanged)
    Q_PROPERTY(QString outputStatus READ outputStatus NOTIFY outputChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)

public:
    static constexpr int MinSpeed = 4, MaxSpeed = 200;
    static constexpr int MinFontSize = 24, MaxFontSize = 160;
    static constexpr int MinMargin = 0, MaxMargin = 320;

    explicit Backend(DrmOutput *output, const QString &stateDirectory = {}, QObject *parent = nullptr);
    ~Backend() override;

    QString text() const { return m_text; }
    void setText(const QString &text);
    bool playing() const { return m_playing; }
    int speed() const { return m_speed; }
    void setSpeed(int linesPerMinute);
    int fontSize() const { return m_fontSize; }
    void setFontSize(int pixels);
    int margin() const { return m_margin; }
    void setMargin(int pixels);
    double readingLine() const { return m_readingLine; }
    void setReadingLine(double fraction);
    bool mirror() const { return m_mirror; }
    void setMirror(bool mirror);
    double progress();
    double offset() const { return m_offset; }
    bool outputConnected() const;
    QString outputStatus() const;
    QString message() const { return m_message; }

    const QImage &frame() const { return m_frame; }

    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void faster();
    Q_INVOKABLE void slower();
    Q_INVOKABLE void nudge(double lines);
    Q_INVOKABLE void scrub(double pixels);
    Q_INVOKABLE void toTop();
    Q_INVOKABLE void openFile(const QUrl &url);
    void setAccent(const QColor &accent);

    // Advances the scroll by elapsed seconds; public so tests can drive time.
    void advance(double seconds);

signals:
    void textChanged();
    void playingChanged();
    void settingsChanged();
    void frameChanged();
    void outputChanged();
    void messageChanged();

private:
    void requestFrame();
    void render();
    void onFrameDone();
    void setOffset(double offset);
    void setPlaying(bool playing);
    void setMessage(const QString &message);
    void saveSettings();
    void saveDraft();
    QString draftPath() const;

    DrmOutput *m_output = nullptr;
    Renderer m_renderer;
    QImage m_frame;
    QString m_stateDirectory;
    QString m_text;
    QString m_message;
    bool m_playing = false;
    bool m_dirty = true;
    int m_speed = 30;
    int m_fontSize = 56;
    int m_margin = 80;
    double m_readingLine = 0.33;
    bool m_mirror = false;
    double m_offset = 0;
    QElapsedTimer m_clock;
    QTimer m_tick;
    QTimer m_draftSave;
};
