#include "drmoutput.h"

#include <QDir>
#include <QFile>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

DrmOutput::DrmOutput(QObject *parent) : QObject(parent) {
    // One timer does double duty: while disconnected it looks for the
    // Prompter, while connected it notices unplugs and DisplayLink restarts.
    m_poll.setInterval(1000);
    connect(&m_poll, &QTimer::timeout, this, [this] { isConnected() ? checkConnector() : tryConnect(); });
    m_poll.start();
    tryConnect();
}

DrmOutput::~DrmOutput() {
    if (isConnected()) {
        // Leave the glass dark rather than frozen on the last line of script.
        for (auto &buffer : m_buffers)
            if (buffer.pixels)
                memset(buffer.pixels, 0, buffer.size);
        if (m_buffers[0].fb)
            drmModeSetCrtc(m_fd, m_crtc, m_buffers[0].fb, 0, 0, &m_connector, 1, nullptr);
    }
    tearDown({});
}

void DrmOutput::setStatus(const QString &status) {
    if (status == m_status)
        return;
    m_status = status;
    emit statusChanged();
}

void DrmOutput::tryConnect() {
    bool sawEvdi = false;
    QString failure;

    const auto cards = QDir("/dev/dri").entryList({"card*"}, QDir::System, QDir::Name);
    for (const auto &name : cards) {
        const QString path = "/dev/dri/" + name;
        const int fd = open(QFile::encodeName(path).constData(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
        if (fd < 0) {
            if (errno == EACCES)
                failure = QString("No permission to open %1.").arg(path);
            continue;
        }
        drmVersionPtr version = drmGetVersion(fd);
        const bool evdi = version && qstrcmp(version->name, "evdi") == 0;
        drmFreeVersion(version);
        if (!evdi) {
            close(fd);
            continue;
        }
        sawEvdi = true;
        // setUp owns the descriptor from here, and closes it on failure.
        if (setUp(fd, path))
            return;
        if (m_status.startsWith("Another program"))
            failure = m_status;
    }

    if (!failure.isEmpty())
        setStatus(failure);
    else if (!sawEvdi)
        setStatus("DisplayLink isn't running, so there is no Prompter device. "
                  "Start it with: sudo systemctl start displaylink");
    else
        setStatus("Waiting for the Prompter. Is it plugged in?");
}

bool DrmOutput::setUp(int fd, const QString &path) {
    drmModeRes *resources = drmModeGetResources(fd);
    if (!resources) {
        close(fd);
        return false;
    }

    drmModeConnector *connector = nullptr;
    for (int i = 0; i < resources->count_connectors && !connector; ++i) {
        connector = drmModeGetConnector(fd, resources->connectors[i]);
        if (connector && (connector->connection != DRM_MODE_CONNECTED || connector->count_modes == 0)) {
            drmModeFreeConnector(connector);
            connector = nullptr;
        }
    }
    if (!connector || resources->count_crtcs == 0) {
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return false;
    }

    // Only become master once there is something to drive, so an idle evdi
    // card is never held hostage.
    if (drmSetMaster(fd) != 0) {
        setStatus(QString("Another program (probably Hyprland) is driving the Prompter on %1. "
                          "Keep Hyprland off it with AQ_DRM_DEVICES.").arg(path));
        drmModeFreeConnector(connector);
        drmModeFreeResources(resources);
        close(fd);
        return false;
    }

    drmModeModeInfo mode = connector->modes[0];
    for (int i = 0; i < connector->count_modes; ++i)
        if (connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
            mode = connector->modes[i];
            break;
        }

    m_fd = fd;
    m_crtc = resources->crtcs[0];
    m_connector = connector->connector_id;
    m_size = QSize(mode.hdisplay, mode.vdisplay);
    drmModeFreeConnector(connector);
    drmModeFreeResources(resources);

    for (auto &buffer : m_buffers)
        if (!createBuffer(buffer)) {
            tearDown(QString("Couldn't allocate frame buffers on %1: %2").arg(path, strerror(errno)));
            return false;
        }

    if (drmModeSetCrtc(m_fd, m_crtc, m_buffers[0].fb, 0, 0, &m_connector, 1, &mode) != 0) {
        tearDown(QString("Couldn't set the Prompter's mode: %1").arg(strerror(errno)));
        return false;
    }

    m_back = 1;
    m_flipPending = false;
    m_flipUnsupported = false;
    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &DrmOutput::readEvents);

    setStatus(QString("Connected · %1×%2").arg(m_size.width()).arg(m_size.height()));
    emit frameDone();
    return true;
}

void DrmOutput::tearDown(const QString &status) {
    m_notifier.reset();
    if (m_fd >= 0) {
        for (auto &buffer : m_buffers)
            destroyBuffer(buffer);
        drmDropMaster(m_fd);
        close(m_fd);
    }
    m_fd = -1;
    m_flipPending = false;
    m_size = {};
    if (!status.isEmpty())
        setStatus(status);
}

bool DrmOutput::createBuffer(Buffer &buffer) {
    drm_mode_create_dumb create{};
    create.width = m_size.width();
    create.height = m_size.height();
    create.bpp = 32;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0)
        return false;
    buffer.handle = create.handle;
    buffer.pitch = create.pitch;
    buffer.size = create.size;

    if (drmModeAddFB(m_fd, create.width, create.height, 24, 32, create.pitch, create.handle, &buffer.fb) != 0)
        return false;

    drm_mode_map_dumb map{};
    map.handle = create.handle;
    if (drmIoctl(m_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) != 0)
        return false;
    void *pixels = mmap(nullptr, buffer.size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, map.offset);
    if (pixels == MAP_FAILED)
        return false;
    buffer.pixels = static_cast<uchar *>(pixels);
    memset(buffer.pixels, 0, buffer.size);
    return true;
}

void DrmOutput::destroyBuffer(Buffer &buffer) {
    if (buffer.pixels)
        munmap(buffer.pixels, buffer.size);
    if (buffer.fb)
        drmModeRmFB(m_fd, buffer.fb);
    if (buffer.handle) {
        drm_mode_destroy_dumb destroy{};
        destroy.handle = buffer.handle;
        drmIoctl(m_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    buffer = {};
}

void DrmOutput::checkConnector() {
    drmModeConnector *connector = drmModeGetConnectorCurrent(m_fd, m_connector);
    const bool connected = connector && connector->connection == DRM_MODE_CONNECTED;
    drmModeFreeConnector(connector);
    if (!connected) {
        tearDown("The Prompter was disconnected. Waiting for it to come back.");
    }
}

void DrmOutput::present(const QImage &frame, bool mirror) {
    if (!readyForFrame())
        return;

    // Without page flips there is only the scanout buffer to draw into.
    Buffer &target = m_flipUnsupported ? m_buffers[0] : m_buffers[m_back];
    const int width = qMin(frame.width(), m_size.width());
    const int height = qMin(frame.height(), m_size.height());
    const QImage source = frame.format() == QImage::Format_RGB32 ? frame : frame.convertToFormat(QImage::Format_RGB32);

    // A beam-splitter reflects the panel, so the reader sees it left-right
    // reversed. Mirroring here, at the copy, keeps the preview readable.
    for (int y = 0; y < height; ++y) {
        const auto *in = reinterpret_cast<const uint32_t *>(source.constScanLine(y));
        auto *out = reinterpret_cast<uint32_t *>(target.pixels + size_t(y) * target.pitch);
        if (mirror)
            for (int x = 0; x < width; ++x)
                out[m_size.width() - 1 - x] = in[x];
        else
            memcpy(out, in, size_t(width) * 4);
    }

    if (m_flipUnsupported) {
        drmModeClip clip{0, 0, uint16_t(m_size.width()), uint16_t(m_size.height())};
        drmModeDirtyFB(m_fd, target.fb, &clip, 1);
        m_flipPending = true;
        QTimer::singleShot(16, this, [this] {
            m_flipPending = false;
            emit frameDone();
        });
        return;
    }

    if (drmModePageFlip(m_fd, m_crtc, target.fb, DRM_MODE_PAGE_FLIP_EVENT, this) == 0) {
        m_flipPending = true;
        return;
    }
    if (errno == EBUSY) {
        // The previous frame hasn't landed yet; try again shortly.
        m_flipPending = true;
        QTimer::singleShot(4, this, [this] {
            m_flipPending = false;
            emit frameDone();
        });
        return;
    }
    // Some drivers can't flip; fall back to drawing into the scanout buffer.
    m_flipUnsupported = true;
    m_flipPending = false;
    QTimer::singleShot(0, this, &DrmOutput::frameDone);
}

void DrmOutput::readEvents() {
    drmEventContext context{};
    context.version = 2;
    context.page_flip_handler = &DrmOutput::onPageFlip;
    if (drmHandleEvent(m_fd, &context) != 0 && errno != EAGAIN)
        tearDown(QString("Lost the Prompter: %1").arg(strerror(errno)));
}

void DrmOutput::onPageFlip(int, unsigned int, unsigned int, unsigned int, void *data) {
    auto *self = static_cast<DrmOutput *>(data);
    self->m_back ^= 1;
    self->m_flipPending = false;
    emit self->frameDone();
}
