#pragma once

#include <QImage>
#include <QObject>
#include <QSocketNotifier>
#include <QString>
#include <QTimer>
#include <array>
#include <cstdint>
#include <memory>

// Drives the Elgato Prompter's panel directly. The Prompter is a DisplayLink
// device: DisplayLinkManager exposes it as an evdi DRM card, and Hyprland is
// kept off that card (AQ_DRM_DEVICES), so this class can become its DRM master
// and page-flip CPU-rendered frames onto it. No compositor involved.
class DrmOutput : public QObject {
    Q_OBJECT

public:
    explicit DrmOutput(QObject *parent = nullptr);
    ~DrmOutput() override;

    bool isConnected() const { return m_fd >= 0; }
    QString status() const { return m_status; }
    QSize size() const { return m_size; }

    // True when the previous frame has reached the panel and another can go.
    bool readyForFrame() const { return isConnected() && !m_flipPending; }

    // Copies the frame into the back buffer (mirrored if asked) and flips it.
    void present(const QImage &frame, bool mirror);

signals:
    void statusChanged();
    void frameDone();

private:
    struct Buffer {
        uint32_t handle = 0;
        uint32_t fb = 0;
        uint32_t pitch = 0;
        uint64_t size = 0;
        uchar *pixels = nullptr;
    };

    void tryConnect();
    bool setUp(int fd, const QString &path);
    void tearDown(const QString &status);
    bool createBuffer(Buffer &buffer);
    void destroyBuffer(Buffer &buffer);
    void checkConnector();
    void readEvents();
    void setStatus(const QString &status);
    static void onPageFlip(int fd, unsigned int, unsigned int, unsigned int, void *data);

    int m_fd = -1;
    uint32_t m_crtc = 0;
    uint32_t m_connector = 0;
    QSize m_size;
    std::array<Buffer, 2> m_buffers;
    int m_back = 1;
    bool m_flipPending = false;
    bool m_flipUnsupported = false;
    QString m_status;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QTimer m_poll;
};
