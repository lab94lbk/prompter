#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <cmath>

#include "drmoutput.h"

Backend::Backend(DrmOutput *output, const QString &stateDirectory, QObject *parent)
    : QObject(parent), m_output(output),
      m_stateDirectory(stateDirectory.isEmpty()
                           ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           : stateDirectory) {
    QSettings settings(m_stateDirectory + "/settings.ini", QSettings::IniFormat);
    m_speed = std::clamp(settings.value("speed", m_speed).toInt(), MinSpeed, MaxSpeed);
    m_fontSize = std::clamp(settings.value("fontSize", m_fontSize).toInt(), MinFontSize, MaxFontSize);
    m_margin = std::clamp(settings.value("margin", m_margin).toInt(), MinMargin, MaxMargin);
    m_readingLine = std::clamp(settings.value("readingLine", m_readingLine).toDouble(), 0.1, 0.7);
    m_mirror = settings.value("mirror", m_mirror).toBool();

    QFile draft(draftPath());
    if (draft.open(QIODevice::ReadOnly))
        m_text = QString::fromUtf8(draft.readAll());

    m_renderer.setText(m_text);
    m_renderer.setFontSize(m_fontSize);
    m_renderer.setMargin(m_margin);
    m_renderer.setReadingLine(m_readingLine);

    // Without the Prompter, a timer stands in for its page flips.
    m_tick.setInterval(16);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, [this] {
        if (outputConnected() || (!m_playing && !m_dirty))
            m_tick.stop();
        else
            render();
    });

    m_draftSave.setSingleShot(true);
    m_draftSave.setInterval(600);
    connect(&m_draftSave, &QTimer::timeout, this, &Backend::saveDraft);

    if (m_output) {
        connect(m_output, &DrmOutput::frameDone, this, &Backend::onFrameDone);
        connect(m_output, &DrmOutput::statusChanged, this, [this] {
            if (outputConnected())
                m_renderer.setFrameSize(m_output->size());
            emit outputChanged();
            requestFrame();
        });
        if (m_output->isConnected())
            m_renderer.setFrameSize(m_output->size());
    }
    requestFrame();
}

Backend::~Backend() {
    if (m_draftSave.isActive())
        saveDraft();
}

bool Backend::outputConnected() const { return m_output && m_output->isConnected(); }

QString Backend::outputStatus() const { return m_output ? m_output->status() : QString("No output"); }

void Backend::setText(const QString &text) {
    if (text == m_text)
        return;
    m_text = text;
    m_renderer.setText(text);
    setOffset(std::min(m_offset, m_renderer.maxOffset()));
    emit textChanged();
    m_draftSave.start();
    requestFrame();
}

void Backend::setSpeed(int linesPerMinute) {
    linesPerMinute = std::clamp(linesPerMinute, MinSpeed, MaxSpeed);
    if (linesPerMinute == m_speed)
        return;
    m_speed = linesPerMinute;
    saveSettings();
}

void Backend::setFontSize(int pixels) {
    pixels = std::clamp(pixels, MinFontSize, MaxFontSize);
    if (pixels == m_fontSize)
        return;
    // Keep the same place in the script while the text reflows around it.
    const double before = progress();
    m_fontSize = pixels;
    m_renderer.setFontSize(pixels);
    setOffset(before * m_renderer.maxOffset());
    saveSettings();
}

void Backend::setMargin(int pixels) {
    pixels = std::clamp(pixels, MinMargin, MaxMargin);
    if (pixels == m_margin)
        return;
    const double before = progress();
    m_margin = pixels;
    m_renderer.setMargin(pixels);
    setOffset(before * m_renderer.maxOffset());
    saveSettings();
}

void Backend::setReadingLine(double fraction) {
    fraction = std::clamp(fraction, 0.1, 0.7);
    if (qFuzzyCompare(fraction, m_readingLine))
        return;
    m_readingLine = fraction;
    m_renderer.setReadingLine(fraction);
    saveSettings();
}

void Backend::setMirror(bool mirror) {
    if (mirror == m_mirror)
        return;
    m_mirror = mirror;
    saveSettings();
}

void Backend::setAccent(const QColor &accent) {
    m_renderer.setAccent(accent);
    requestFrame();
}

double Backend::progress() {
    const double max = m_renderer.maxOffset();
    return max > 0 ? std::clamp(m_offset / max, 0.0, 1.0) : 0;
}

void Backend::togglePlay() {
    // Starting again at the very end means "from the top".
    if (!m_playing && m_offset >= m_renderer.maxOffset() && m_offset > 0)
        setOffset(0);
    setPlaying(!m_playing);
}

void Backend::faster() { setSpeed(m_speed + (m_speed < 40 ? 2 : 5)); }

void Backend::slower() { setSpeed(m_speed - (m_speed <= 40 ? 2 : 5)); }

void Backend::nudge(double lines) { scrub(lines * m_renderer.lineHeight()); }

void Backend::scrub(double pixels) { setOffset(m_offset + pixels); }

void Backend::toTop() {
    setPlaying(false);
    setOffset(0);
}

void Backend::openFile(const QUrl &url) {
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setMessage(QString("Couldn't open %1: %2").arg(path, file.errorString()));
        return;
    }
    // The file is only read; edits go to the draft, never back to it.
    setPlaying(false);
    setText(QString::fromUtf8(file.readAll()));
    setOffset(0);
    setMessage(QString("Opened %1").arg(QFileInfo(path).fileName()));
}

void Backend::advance(double seconds) {
    if (!m_playing)
        return;
    const double pixelsPerSecond = m_speed / 60.0 * m_renderer.lineHeight();
    const double max = m_renderer.maxOffset();
    setOffset(m_offset + pixelsPerSecond * seconds);
    if (m_offset >= max)
        setPlaying(false);
}

void Backend::setOffset(double offset) {
    offset = std::clamp(offset, 0.0, m_renderer.maxOffset());
    if (offset == m_offset)
        return;
    m_offset = offset;
    requestFrame();
}

void Backend::setPlaying(bool playing) {
    if (playing == m_playing)
        return;
    m_playing = playing;
    m_clock.restart();
    emit playingChanged();
    requestFrame();
}

void Backend::setMessage(const QString &message) {
    m_message = message;
    emit messageChanged();
}

void Backend::requestFrame() {
    m_dirty = true;
    if (outputConnected()) {
        if (m_output->readyForFrame())
            QMetaObject::invokeMethod(this, &Backend::render, Qt::QueuedConnection);
    } else if (!m_tick.isActive()) {
        m_tick.start();
    }
}

void Backend::onFrameDone() {
    if (m_playing || m_dirty)
        render();
}

void Backend::render() {
    if (outputConnected() && !m_output->readyForFrame())
        return;

    // Time since the last frame, capped so a stall doesn't leap ahead.
    const double seconds = m_clock.isValid() ? std::min(m_clock.restart() / 1000.0, 0.1) : 0;
    advance(seconds);

    m_dirty = false;
    m_renderer.paint(m_frame, m_offset, !m_playing);
    if (outputConnected())
        m_output->present(m_frame, m_mirror);
    emit frameChanged();
}

void Backend::saveSettings() {
    QSettings settings(m_stateDirectory + "/settings.ini", QSettings::IniFormat);
    settings.setValue("speed", m_speed);
    settings.setValue("fontSize", m_fontSize);
    settings.setValue("margin", m_margin);
    settings.setValue("readingLine", m_readingLine);
    settings.setValue("mirror", m_mirror);
    emit settingsChanged();
    requestFrame();
}

QString Backend::draftPath() const { return m_stateDirectory + "/script.md"; }

void Backend::saveDraft() {
    QDir().mkpath(m_stateDirectory);
    QSaveFile file(draftPath());
    if (!file.open(QIODevice::WriteOnly) || file.write(m_text.toUtf8()) < 0 || !file.commit())
        setMessage(QString("Couldn't save the draft: %1").arg(file.errorString()));
}
