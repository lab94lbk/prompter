#include "preview.h"

#include <QPainter>

Preview::Preview(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setFillColor(Qt::black);
}

void Preview::setBackend(Backend *backend) {
    if (backend == m_backend)
        return;
    if (m_backend)
        disconnect(m_backend, nullptr, this, nullptr);
    m_backend = backend;
    if (m_backend)
        connect(m_backend, &Backend::frameChanged, this, [this] { update(); });
    emit backendChanged();
    update();
}

void Preview::paint(QPainter *painter) {
    if (!m_backend || m_backend->frame().isNull())
        return;
    const QImage &frame = m_backend->frame();
    const QSizeF scaled = QSizeF(frame.size()).scaled(size(), Qt::KeepAspectRatio);
    const QRectF target(QPointF((width() - scaled.width()) / 2, (height() - scaled.height()) / 2), scaled);
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, frame);
}
