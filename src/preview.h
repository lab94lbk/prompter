#pragma once

#include <QPointer>
#include <QQuickPaintedItem>

#include "backend.h"

// Shows the exact frame going to the glass, unmirrored so it reads normally.
class Preview : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Backend *backend READ backend WRITE setBackend NOTIFY backendChanged)

public:
    explicit Preview(QQuickItem *parent = nullptr);

    Backend *backend() const { return m_backend; }
    void setBackend(Backend *backend);
    void paint(QPainter *painter) override;

signals:
    void backendChanged();

private:
    QPointer<Backend> m_backend;
};
