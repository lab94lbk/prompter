#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QTextDocument>

// Lays out a script and paints prompter frames. Pure: no timing, no device,
// so the same frame feeds the glass and the preview, and tests can check it.
class Renderer {
public:
    Renderer();

    void setText(const QString &text);
    void setFontSize(int pixels);
    void setMargin(int pixels);
    void setReadingLine(double fraction);
    void setAccent(const QColor &accent);
    void setFrameSize(const QSize &size);

    QSize frameSize() const { return m_size; }

    // Total scrollable distance: the script's height, so offset 0 puts the
    // first line on the reading line and maxOffset() puts the last one there.
    double maxOffset();
    double lineHeight() const;

    // Markdown, but with every line break kept: scripts are written line by
    // line, and CommonMark would fold single newlines into spaces.
    static QString toMarkdown(const QString &text);

    void paint(QImage &frame, double offset, bool paused);

private:
    void layout();

    QTextDocument m_document;
    QString m_text;
    QSize m_size{1024, 600};
    int m_fontSize = 56;
    int m_margin = 80;
    double m_readingLine = 0.33;
    QColor m_accent{"#FFD60A"};
    bool m_dirty = true;
};
