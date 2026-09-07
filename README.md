Minimalist Timer HUD Overlay
A lightweight, click-through, always-on-top timer overlay for Linux desktops. Specifically optimized for KDE Plasma (KWin) on X11, it acts as a true on-screen display (OSD) that stays visible even over fullscreen applications like games or videos.

✨ Features

    🖥️ True Overlay: Frameless, transparent background, text-only display. Completely click-through (does not block mouse input).
    📍 Corner Snapping: Automatically snaps to the Top-Left, Top-Right, Bottom-Left, or Bottom-Right of your primary screen.
    ⏱️ Flexible Timer: Set a countdown in minutes, or use the default "Count Up" mode.
    🚨 Finished Alert: When a countdown reaches 00:00:00, it switches to count-up and flashes Red / White to alert you.
    🎨 Customizable Text: Choose between Always White or Always Black text for optimal contrast.
    📺 Fullscreen Bypass: Uses KWin/X11 specific window hints (_KDE_NET_WM_WINDOW_TYPE_ON_SCREEN_DISPLAY, _NET_WM_STATE_ABOVE) to stay on top of exclusive fullscreen apps.
    ⚙️ GUI Settings: Easy configuration via the system tray icon. Settings are saved automatically.
    ⌨️ Global Kill Switch: Instantly close the app from anywhere with Ctrl+Alt+T.

🛠️ Requirements & Dependencies

This project requires Qt 5.15+ or Qt 6, CMake, and X11 libraries (for the global hotkey and window manager hints).


Arch Linux
# For Qt6 (Recommended)
sudo pacman -S --needed base-devel cmake ninja qt6-base libx11 xorgproto

# OR for Qt5
sudo pacman -S --needed base-devel cmake ninja qt5-base libx11 xorgproto

Ubuntu / Debian
sudo apt install build-essential cmake ninja-build qtbase5-dev libx11-dev x11proto-core-dev
# OR for Qt6: qt6-base-dev

Fedora
sudo dnf install gcc-c++ cmake ninja-build qt6-qtbase-devel libX11-devel

🏗️ Build Instructions
# Clone the repository
git clone https://github.com/yourusername/timer-hud.git
cd timer-hud

# Configure and build
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build

📸 Usage

    Run the app:
./build/timer-hud
Open Settings:

    Right-click the Tray Icon in your system tray and select Settings...
    OR launch with the settings flag: ./build/timer-hud --settings

Configure:

    Choose your preferred screen corner.
    Set the duration (in minutes). Set to 0 for an infinite count-up timer.
    Adjust font size, margin, and text color.

Stop the timer:

    Press Ctrl+Alt+T
    OR right-click the tray icon and select Quit.
