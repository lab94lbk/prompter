#include "renderer.h"

#include <QAbstractTextDocumentLayout>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <cmath>

namespace {
constexpr double LineSpacing = 1.25;
}

Renderer::Renderer() {
    m_document.setDocumentMargin(0);
    m_document.setUndoRedoEnabled(false);
}

void Renderer::setText(const QString &text) {
    if (text == m_text)
        return;
    m_text = text;
    m_dirty = true;
}

void Renderer::setFontSize(int pixels) {
    if (pixels == m_fontSize)
        return;
    m_fontSize = pixels;
    m_dirty = true;
}

void Renderer::setMargin(int pixels) {
    if (pixels == m_margin)
        return;
    m_margin = pixels;
    m_dirty = true;
}

void Renderer::setReadingLine(double fraction) { m_readingLine = fraction; }

void Renderer::setAccent(const QColor &accent) {
    if (accent == m_accent)
        return;
    m_accent = accent;
    m_dirty = true;
}

void Renderer::setFrameSize(const QSize &size) {
    if (size == m_size || size.isEmpty())
        return;
    m_size = size;
    m_dirty = true;
}

QString Renderer::toMarkdown(const QString &text) {
    QStringList lines = text.split('\n');
    for (auto &line : lines)
        if (!line.trimmed().isEmpty())
            line += "  ";
    return lines.join('\n');
}

double Renderer::lineHeight() const {
    QFont font;
    font.setPixelSize(m_fontSize);
    return QFontMetricsF(font).lineSpacing() * LineSpacing;
}

double Renderer::maxOffset() {
    layout();
    return std::max(0.0, m_document.size().height() - lineHeight());
}

void Renderer::layout() {
    if (!m_dirty)
        return;
    m_dirty = false;

    QFont font;
    font.setPixelSize(m_fontSize);
    m_document.setDefaultFont(font);
    m_document.setMarkdown(toMarkdown(m_text), QTextDocument::MarkdownDialectGitHub);
    m_document.setTextWidth(std::max(100, m_size.width() - 2 * m_margin));

    // Generous line spacing and a gap between paragraphs: the eye has to find
    // its place again after every glance at the lens.
    QTextCursor cursor(&m_document);
    cursor.beginEditBlock();
    for (QTextBlock block = m_document.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        format.setLineHeight(LineSpacing * 100, QTextBlockFormat::ProportionalHeight);
        format.setBottomMargin(m_fontSize * 0.6);
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }

    // [Stage directions] read as cues, not lines: accent colored and italic.
    QTextCharFormat cue;
    cue.setForeground(m_accent);
    cue.setFontItalic(true);
    const QRegularExpression direction(R"(\[[^\]\n]+\])");
    for (QTextCursor match = m_document.find(direction); !match.isNull();
         match = m_document.find(direction, match))
        match.mergeCharFormat(cue);
    cursor.endEditBlock();
}

void Renderer::paint(QImage &frame, double offset, bool paused) {
    if (frame.size() != m_size || frame.format() != QImage::Format_RGB32)
        frame = QImage(m_size, QImage::Format_RGB32);
    layout();
    frame.fill(Qt::black);

    QPainter painter(&frame);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);

    const double width = m_size.width();
    const double height = m_size.height();
    const double line = lineHeight();
    const double readingY = height * m_readingLine;

    if (m_text.trimmed().isEmpty()) {
        QFont font;
        font.setPixelSize(std::max(18, m_fontSize / 2));
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 90));
        painter.drawText(QRectF(0, 0, width, height), Qt::AlignCenter, "Type or paste your script");
        return;
    }

    // Whole pixels only: fractional positions make text shimmer as it moves.
    const double top = std::round(readingY - line / 2 - offset);
    painter.save();
    painter.translate(m_margin, top);
    QAbstractTextDocumentLayout::PaintContext context;
    context.palette.setColor(QPalette::Text, Qt::white);
    context.clip = QRectF(0, -top, width, height);
    painter.setClipRect(context.clip);
    m_document.documentLayout()->draw(&painter, context);
    painter.restore();

    // Fade what's already been read, so the eye stays on the reading line.
    const double fadeEnd = readingY - line / 2;
    if (fadeEnd > 0) {
        QLinearGradient fade(0, 0, 0, fadeEnd);
        fade.setColorAt(0, QColor(0, 0, 0, 200));
        fade.setColorAt(1, QColor(0, 0, 0, 0));
        painter.fillRect(QRectF(0, 0, width, fadeEnd), fade);
    }

    // Reading line: an arrow in each margin pointing at the current line.
    const double arrow = std::clamp(line * 0.35, 10.0, 26.0);
    const double inset = std::clamp(m_margin * 0.25, 4.0, 24.0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_accent);
    QPainterPath left;
    left.moveTo(inset, readingY - arrow);
    left.lineTo(inset + arrow, readingY);
    left.lineTo(inset, readingY + arrow);
    left.closeSubpath();
    painter.drawPath(left);
    QPainterPath right;
    right.moveTo(width - inset, readingY - arrow);
    right.lineTo(width - inset - arrow, readingY);
    right.lineTo(width - inset, readingY + arrow);
    right.closeSubpath();
    painter.drawPath(right);

    const double max = maxOffset();
    const double progress = max > 0 ? std::clamp(offset / max, 0.0, 1.0) : 0;
    painter.fillRect(QRectF(0, height - 4, width * progress, 4), m_accent);

    if (paused) {
        const double bar = 6, gap = 6, tall = 22, x = width - 24 - 2 * bar - gap, y = 20;
        painter.drawRect(QRectF(x, y, bar, tall));
        painter.drawRect(QRectF(x + bar + gap, y, bar, tall));
    }
}
