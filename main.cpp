#include <QApplication>
#include <QCoreApplication>
#include <QWidget>
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <QElapsedTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QDebug>
#include <QSettings>
#include <QDialog>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QPixmap>
#include <QSocketNotifier>
#include <QVector>
#include <QtGlobal>
#include <QRectF>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

enum class Corner {
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight
};

enum class TextColorMode {
    White,
    Black
};

struct HudSettings {
    Corner corner = Corner::TopRight;
    int durationMinutes = 0;          // 0 = count up, default
    int margin = 0;                   // pixels from screen corner
    bool useAvailableGeometry = true; // avoid panels
    int fontSize = 28;
    TextColorMode textColorMode = TextColorMode::White;
};

static QString cornerToString(Corner corner) {
    switch (corner) {
        case Corner::TopLeft:
            return QStringLiteral("top-left");
        case Corner::TopRight:
            return QStringLiteral("top-right");
        case Corner::BottomLeft:
            return QStringLiteral("bottom-left");
        case Corner::BottomRight:
            return QStringLiteral("bottom-right");
    }

    return QStringLiteral("top-right");
}

static Corner cornerFromString(const QString& raw) {
    const QString v = raw.trimmed().toLower();

    if (v == QStringLiteral("tl") ||
        v == QStringLiteral("top-left") ||
        v == QStringLiteral("topleft")) {
        return Corner::TopLeft;
        }

        if (v == QStringLiteral("tr") ||
            v == QStringLiteral("top-right") ||
            v == QStringLiteral("topright")) {
            return Corner::TopRight;
            }

            if (v == QStringLiteral("bl") ||
                v == QStringLiteral("bottom-left") ||
                v == QStringLiteral("bottomleft")) {
                return Corner::BottomLeft;
                }

                if (v == QStringLiteral("br") ||
                    v == QStringLiteral("bottom-right") ||
                    v == QStringLiteral("bottomright")) {
                    return Corner::BottomRight;
                    }

                    return Corner::TopRight;
}

static QString textColorToString(TextColorMode mode) {
    switch (mode) {
        case TextColorMode::White:
            return QStringLiteral("white");
        case TextColorMode::Black:
            return QStringLiteral("black");
    }

    return QStringLiteral("white");
}

static TextColorMode textColorFromString(const QString& raw) {
    const QString v = raw.trimmed().toLower();

    if (v == QStringLiteral("black") ||
        v == QStringLiteral("always-black")) {
        return TextColorMode::Black;
        }

        return TextColorMode::White;
}

static HudSettings loadSettings() {
    QSettings qs;
    HudSettings s;

    s.corner = cornerFromString(
        qs.value(QStringLiteral("corner"), cornerToString(Corner::TopRight)).toString()
    );

    // Default duration is count-up.
    s.durationMinutes = qBound(
        0,
        qs.value(QStringLiteral("durationMinutes"), 0).toInt(),
                               999999
    );

    s.margin = qBound(
        0,
        qs.value(QStringLiteral("margin"), 0).toInt(),
                      500
    );

    s.useAvailableGeometry = qs.value(
        QStringLiteral("useAvailableGeometry"),
                                      true
    ).toBool();

    s.fontSize = qBound(
        8,
        qs.value(QStringLiteral("fontSize"), 28).toInt(),
                        96
    );

    s.textColorMode = textColorFromString(
        qs.value(
            QStringLiteral("textColorMode"),
                 textColorToString(TextColorMode::White)
        ).toString()
    );

    return s;
}

static void saveSettings(const HudSettings& s) {
    QSettings qs;

    qs.setValue(QStringLiteral("corner"), cornerToString(s.corner));
    qs.setValue(QStringLiteral("durationMinutes"), s.durationMinutes);
    qs.setValue(QStringLiteral("margin"), s.margin);
    qs.setValue(QStringLiteral("useAvailableGeometry"), s.useAvailableGeometry);
    qs.setValue(QStringLiteral("fontSize"), s.fontSize);
    qs.setValue(QStringLiteral("textColorMode"), textColorToString(s.textColorMode));
}

static bool s_xGrabError = false;

static int xGrabErrorHandler(Display*, XErrorEvent*) {
    s_xGrabError = true;
    return 0;
}

// This asks KWin/X11 to treat the HUD like a special on-screen-display window.
// This improves the chance that it remains visible above fullscreen windows.
static void applyX11OnScreenWindowType(WId windowId) {
    if (!QGuiApplication::platformName().startsWith(QLatin1String("xcb"))) {
        return;
    }

    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        return;
    }

    Window xwindow = static_cast<Window>(windowId);

    Atom netWmWindowType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);

    Atom typeKdeOsd =
    XInternAtom(display, "_KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY", False);

    Atom typeOsd =
    XInternAtom(display, "_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY", False);

    Atom typeDock =
    XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", False);

    Atom typeNotification =
    XInternAtom(display, "_NET_WM_WINDOW_TYPE_NOTIFICATION", False);

    Atom typeNormal =
    XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);

    Atom windowTypes[5] = {
        typeKdeOsd,
        typeOsd,
        typeDock,
        typeNotification,
        typeNormal
    };

    XChangeProperty(
        display,
        xwindow,
        netWmWindowType,
        XA_ATOM,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char*>(windowTypes),
                    5
    );

    Atom netWmState = XInternAtom(display, "_NET_WM_STATE", False);

    Atom stateAbove =
    XInternAtom(display, "_NET_WM_STATE_ABOVE", False);

    Atom stateSkipTaskbar =
    XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);

    Atom stateSkipPager =
    XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);

    Atom states[3] = {
        stateAbove,
        stateSkipTaskbar,
        stateSkipPager
    };

    XChangeProperty(
        display,
        xwindow,
        netWmState,
        XA_ATOM,
        32,
        PropModeReplace,
        reinterpret_cast<unsigned char*>(states),
                    3
    );

    // Also request the window manager to add the ABOVE state.
    XEvent event = {};

    event.xclient.type = ClientMessage;
    event.xclient.window = xwindow;
    event.xclient.message_type = netWmState;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
    event.xclient.data.l[1] = static_cast<long>(stateAbove);
    event.xclient.data.l[2] = 0;
    event.xclient.data.l[3] = 1; // source indication: application
    event.xclient.data.l[4] = 0;

    XSendEvent(
        display,
        DefaultRootWindow(display),
               False,
               SubstructureNotifyMask | SubstructureRedirectMask,
               &event
    );

    XFlush(display);
    XCloseDisplay(display);
}

class X11GlobalHotkey : public QObject {
    Q_OBJECT

public:
    explicit X11GlobalHotkey(QObject* parent = nullptr)
    : QObject(parent)
    {
        m_display = XOpenDisplay(nullptr);

        if (!m_display) {
            qWarning() << "Global hotkey disabled: XOpenDisplay() failed.";
            return;
        }

        m_root = DefaultRootWindow(m_display);

        m_keycode = XKeysymToKeycode(m_display, XK_T);
        if (!m_keycode) {
            m_keycode = XKeysymToKeycode(m_display, XK_t);
        }

        if (!m_keycode) {
            qWarning() << "Global hotkey disabled: could not find keycode for T.";
            XCloseDisplay(m_display);
            m_display = nullptr;
            return;
        }

        const unsigned int base = ControlMask | Mod1Mask;

        // Grab with common lock modifiers so Num Lock / Caps Lock do not break it.
        const QVector<unsigned int> extraMasks = {
            0,
            LockMask,
            Mod2Mask,
            LockMask | Mod2Mask
        };

        XErrorHandler oldHandler = XSetErrorHandler(xGrabErrorHandler);

        for (unsigned int extra : extraMasks) {
            const unsigned int mods = base | extra;

            s_xGrabError = false;
            XGrabKey(
                m_display,
                m_keycode,
                mods,
                m_root,
                False,
                GrabModeAsync,
                GrabModeAsync
            );

            XSync(m_display, False);

            if (!s_xGrabError) {
                m_grabs.append(Grab{m_keycode, mods});
            } else {
                qWarning()
                << "Global hotkey: failed to grab Ctrl+Alt+T with modifier mask"
                << mods
                << "(it may already be used by another application/KDE shortcut).";
            }
        }

        XSetErrorHandler(oldHandler);

        if (m_grabs.isEmpty()) {
            qWarning() << "Global hotkey Ctrl+Alt+T could not be registered.";
            XCloseDisplay(m_display);
            m_display = nullptr;
            return;
        }

        XSelectInput(m_display, m_root, KeyPressMask);

        m_notifier = new QSocketNotifier(
            ConnectionNumber(m_display),
                                         QSocketNotifier::Read,
                                         this
        );

        connect(m_notifier, &QSocketNotifier::activated, this, [this] {
            processXEvents();
        });
    }

    ~X11GlobalHotkey() override {
        if (!m_display) {
            return;
        }

        for (const Grab& grab : m_grabs) {
            XUngrabKey(m_display, grab.keycode, grab.mod, m_root);
        }

        if (m_notifier) {
            m_notifier->setEnabled(false);
            delete m_notifier;
            m_notifier = nullptr;
        }

        XCloseDisplay(m_display);
        m_display = nullptr;
    }

signals:
    void activated();

private:
    struct Grab {
        KeyCode keycode;
        unsigned int mod;
    };

    void processXEvents() {
        if (!m_display) {
            return;
        }

        while (XPending(m_display) > 0) {
            XEvent event;
            XNextEvent(m_display, &event);

            if (event.type != KeyPress) {
                continue;
            }

            XKeyEvent& keyEvent = event.xkey;

            const unsigned int wanted = ControlMask | Mod1Mask;

            if (keyEvent.keycode == m_keycode &&
                (keyEvent.state & wanted) == wanted &&
                !(keyEvent.state & ShiftMask)) {
                emit activated();
                }
        }
    }

    Display* m_display = nullptr;
    Window m_root = 0;
    KeyCode m_keycode = 0;
    QSocketNotifier* m_notifier = nullptr;
    QVector<Grab> m_grabs;
};

class TimerHud : public QWidget {
    Q_OBJECT

public:
    explicit TimerHud(const HudSettings& settings, QWidget* parent = nullptr)
    : QWidget(parent),
    m_settings(settings)
    {
        setFixedSize(300, 150);

        Qt::WindowFlags flags =
        Qt::FramelessWindowHint |
        Qt::WindowStaysOnTopHint |
        Qt::Tool |
        Qt::WindowTransparentForInput |
        Qt::NoDropShadowWindowHint;

        #if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
        flags |= Qt::WindowDoesNotAcceptFocus;
        #endif

        setWindowFlags(flags);

        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);

        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);

        m_elapsed.start();

        QTimer* timer = new QTimer(this);
        timer->setInterval(100);

        connect(timer, &QTimer::timeout, this, [this] {
            const qint64 limit = durationMs();

            if (!m_finished && limit > 0 && m_elapsed.elapsed() >= limit) {
                m_finished = true;
            }

            update();
        });

        timer->start();

        updateScreenConnection();

        if (QGuiApplication* gui = qobject_cast<QGuiApplication*>(QCoreApplication::instance())) {
            connect(gui, &QGuiApplication::primaryScreenChanged, this, [this](QScreen*) {
                updateScreenConnection();
            });
        }
    }

    void applySettings(const HudSettings& settings) {
        const bool durationChanged =
        (m_settings.durationMinutes != settings.durationMinutes);

        const bool wasFinished = m_finished;

        m_settings = settings;

        // If the timer was in the flashing finished state, applying settings
        // clears that state.
        if (durationChanged || wasFinished) {
            restartTimer();
        }

        update();
        snapToCorner();
    }

    void restartTimer() {
        m_elapsed.restart();
        m_finished = false;
        update();
    }

protected:
    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);

        if (!m_onScreenWindowTypeApplied) {
            applyX11OnScreenWindowType(winId());
            m_onScreenWindowTypeApplied = true;
        }

        snapToCorner();
        raise();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::TextAntialiasing);

        const qint64 limit = durationMs();
        const qint64 elapsed = m_elapsed.elapsed();

        qint64 displayMs = 0;
        bool flashing = false;

        if (limit > 0 && m_finished) {
            // Countdown finished: count up from zero and flash.
            displayMs = qMax(qint64(0), elapsed - limit);
            flashing = true;
        } else if (limit > 0) {
            // Normal countdown.
            displayMs = qMax(qint64(0), limit - elapsed);
        } else {
            // Normal count-up.
            displayMs = elapsed;
        }

        const QColor white(255, 255, 255);
        const QColor black(0, 0, 0);
        const QColor red(255, 32, 32);

        QColor normalColor = white;

        if (m_settings.textColorMode == TextColorMode::Black) {
            normalColor = black;
        }

        QColor color = normalColor;

        if (flashing) {
            const bool flash = ((displayMs / 500) % 2) == 0;

            if (flash) {
                color = red;
            } else {
                color = white;
            }
        }

        QFont f = font();
        f.setPointSize(qBound(8, m_settings.fontSize, 96));
        f.setBold(true);

        p.setFont(f);
        p.setPen(color);

        // Text only: no background, no border.
        p.drawText(rect(), Qt::AlignCenter, formatMs(displayMs));
    }

private:
    qint64 durationMs() const {
        return (m_settings.durationMinutes > 0)
        ? qint64(m_settings.durationMinutes) * 60 * 1000
        : 0;
    }

    void updateScreenConnection() {
        if (m_geometryConnection) {
            QObject::disconnect(m_geometryConnection);
        }

        if (m_availableGeometryConnection) {
            QObject::disconnect(m_availableGeometryConnection);
        }

        if (QScreen* screen = QGuiApplication::primaryScreen()) {
            m_geometryConnection = connect(
                screen,
                &QScreen::geometryChanged,
                this,
                [this] { snapToCorner(); }
            );

            m_availableGeometryConnection = connect(
                screen,
                &QScreen::availableGeometryChanged,
                this,
                [this] { snapToCorner(); }
            );
        }

        snapToCorner();
    }

    void snapToCorner() {
        QScreen* screen = QGuiApplication::primaryScreen();
        if (!screen) {
            return;
        }

        const QRect r = m_settings.useAvailableGeometry
        ? screen->availableGeometry()
        : screen->geometry();

        const int m = qMax(0, m_settings.margin);

        int x = r.x() + m;
        int y = r.y() + m;

        switch (m_settings.corner) {
            case Corner::TopLeft:
                break;

            case Corner::TopRight:
                x = r.x() + r.width() - width() - m;
                break;

            case Corner::BottomLeft:
                y = r.y() + r.height() - height() - m;
                break;

            case Corner::BottomRight:
                x = r.x() + r.width() - width() - m;
                y = r.y() + r.height() - height() - m;
                break;
        }

        move(x, y);
    }

    static QString formatMs(qint64 ms) {
        const qint64 totalSecs = ms / 1000;
        const qint64 hours = totalSecs / 3600;
        const int minutes = int((totalSecs % 3600) / 60);
        const int seconds = int(totalSecs % 60);

        return QStringLiteral("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
    }

    HudSettings m_settings;
    QElapsedTimer m_elapsed;
    bool m_finished = false;
    bool m_onScreenWindowTypeApplied = false;

    QMetaObject::Connection m_geometryConnection;
    QMetaObject::Connection m_availableGeometryConnection;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const HudSettings& settings, QWidget* parent = nullptr)
    : QDialog(parent),
    m_settings(settings)
    {
        setWindowTitle(QStringLiteral("Timer HUD Settings"));

        // Keep settings above the HUD while editing.
        setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

        m_cornerCombo = new QComboBox(this);
        m_cornerCombo->addItem(QStringLiteral("Top Left"), int(Corner::TopLeft));
        m_cornerCombo->addItem(QStringLiteral("Top Right"), int(Corner::TopRight));
        m_cornerCombo->addItem(QStringLiteral("Bottom Left"), int(Corner::BottomLeft));
        m_cornerCombo->addItem(QStringLiteral("Bottom Right"), int(Corner::BottomRight));

        const int cornerIndex = m_cornerCombo->findData(int(m_settings.corner));
        if (cornerIndex >= 0) {
            m_cornerCombo->setCurrentIndex(cornerIndex);
        }

        m_durationSpin = new QSpinBox(this);
        m_durationSpin->setRange(0, 999999);
        m_durationSpin->setValue(m_settings.durationMinutes);
        m_durationSpin->setSuffix(QStringLiteral(" min"));
        m_durationSpin->setSpecialValueText(QStringLiteral("Count up"));

        m_marginSpin = new QSpinBox(this);
        m_marginSpin->setRange(0, 500);
        m_marginSpin->setValue(m_settings.margin);
        m_marginSpin->setSuffix(QStringLiteral(" px"));

        m_fontSpin = new QSpinBox(this);
        m_fontSpin->setRange(8, 96);
        m_fontSpin->setValue(m_settings.fontSize);
        m_fontSpin->setSuffix(QStringLiteral(" pt"));

        m_textColorCombo = new QComboBox(this);
        m_textColorCombo->addItem(
            QStringLiteral("Always white"),
                                  int(TextColorMode::White)
        );
        m_textColorCombo->addItem(
            QStringLiteral("Always black"),
                                  int(TextColorMode::Black)
        );

        const int textColorIndex =
        m_textColorCombo->findData(int(m_settings.textColorMode));

        if (textColorIndex >= 0) {
            m_textColorCombo->setCurrentIndex(textColorIndex);
        }

        m_useAvailableGeometryCheck = new QCheckBox(
            QStringLiteral("Use available geometry (avoid panels)"),
                                                    this
        );
        m_useAvailableGeometryCheck->setChecked(m_settings.useAvailableGeometry);

        QFormLayout* form = new QFormLayout;
        form->addRow(QStringLiteral("Corner"), m_cornerCombo);
        form->addRow(QStringLiteral("Duration"), m_durationSpin);
        form->addRow(QStringLiteral("Corner margin"), m_marginSpin);
        form->addRow(QStringLiteral("Font size"), m_fontSpin);
        form->addRow(QStringLiteral("Text color"), m_textColorCombo);
        form->addRow(QString(), m_useAvailableGeometryCheck);

        QLabel* hotkeyLabel = new QLabel(
            QStringLiteral(
                "Global kill switch: Ctrl+Alt+T\n"
                "If it does not work, another KDE shortcut may already be using Ctrl+Alt+T."
            ),
            this
        );
        hotkeyLabel->setWordWrap(true);

        QDialogButtonBox* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            this
        );

        connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(hotkeyLabel);
        layout->addWidget(buttons);
    }

    HudSettings settings() const {
        return m_settings;
    }

    void accept() override {
        m_settings.corner = static_cast<Corner>(
            m_cornerCombo->currentData().toInt()
        );

        m_settings.durationMinutes = m_durationSpin->value();
        m_settings.margin = m_marginSpin->value();
        m_settings.useAvailableGeometry = m_useAvailableGeometryCheck->isChecked();
        m_settings.fontSize = m_fontSpin->value();

        m_settings.textColorMode = static_cast<TextColorMode>(
            m_textColorCombo->currentData().toInt()
        );

        QDialog::accept();
    }

private:
    HudSettings m_settings;

    QComboBox* m_cornerCombo = nullptr;
    QSpinBox* m_durationSpin = nullptr;
    QSpinBox* m_marginSpin = nullptr;
    QSpinBox* m_fontSpin = nullptr;
    QComboBox* m_textColorCombo = nullptr;
    QCheckBox* m_useAvailableGeometryCheck = nullptr;
};

static QIcon createTrayIcon() {
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);

    p.setBrush(QColor(20, 20, 20, 220));
    p.setPen(QPen(QColor(255, 255, 255, 80), 1));
    p.drawRoundedRect(QRectF(pixmap.rect()).adjusted(2, 2, -2, -2), 6, 6);

    QFont f = p.font();
    f.setBold(true);
    f.setPointSize(16);
    p.setFont(f);

    p.setPen(Qt::white);
    p.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("T"));

    return QIcon(pixmap);
}

#include "main.moc"

int main(int argc, char* argv[]) {
    // Default to XCB/X11 behavior unless the user explicitly overrides it.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("xcb"));
    }

    bool showSettingsAtStartup = false;

    // Parse very simple startup options before creating QApplication.
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);

        if (arg == QStringLiteral("-h") || arg == QStringLiteral("--help")) {
            qInfo().noquote() << QStringLiteral(
                "Usage: timer-hud [--settings]\n\n"
                "Duration is configured in minutes.\n"
                "Duration 0 means count up.\n"
                "The settings dialog can be opened from the tray icon or with --settings.\n"
                "Global kill switch: Ctrl+Alt+T (X11/XCB)."
            );
            return 0;
        }

        if (arg == QStringLiteral("--settings")) {
            showSettingsAtStartup = true;
        }
    }

    #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    #endif

    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("TimerHud"));
    QCoreApplication::setApplicationName(QStringLiteral("timer-hud"));

    // Tray app behavior: do not quit just because a dialog was closed.
    app.setQuitOnLastWindowClosed(false);

    HudSettings settings = loadSettings();

    TimerHud hud(settings);
    hud.show();

    X11GlobalHotkey hotkey;

    QObject::connect(&hotkey, &X11GlobalHotkey::activated, &app, [] {
        QCoreApplication::quit();
    });

    QMenu trayMenu;

    QAction* settingsAction = trayMenu.addAction(QStringLiteral("Settings..."));
    QAction* restartAction = trayMenu.addAction(QStringLiteral("Restart timer"));

    trayMenu.addSeparator();

    QAction* quitAction = trayMenu.addAction(QStringLiteral("Quit (Ctrl+Alt+T)"));

    QSystemTrayIcon tray;
    tray.setIcon(createTrayIcon());
    tray.setToolTip(QStringLiteral("Timer HUD"));
    tray.setContextMenu(&trayMenu);
    tray.show();

    auto openSettings = [&settings, &hud]() {
        SettingsDialog dialog(settings);

        if (dialog.exec() == QDialog::Accepted) {
            settings = dialog.settings();
            saveSettings(settings);
            hud.applySettings(settings);
        }
    };

    QObject::connect(settingsAction, &QAction::triggered, &app, openSettings);

    QObject::connect(restartAction, &QAction::triggered, &hud, [&hud] {
        hud.restartTimer();
    });

    QObject::connect(quitAction, &QAction::triggered, &app, [] {
        QCoreApplication::quit();
    });

    QObject::connect(
        &tray,
        &QSystemTrayIcon::activated,
        &app,
        [&openSettings](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                openSettings();
            }
        }
    );

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        showSettingsAtStartup = true;
    }

    if (showSettingsAtStartup) {
        QTimer::singleShot(0, &app, openSettings);
    }

    return app.exec();
}
