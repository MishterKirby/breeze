#include "breezetoolsareamanager.h"
#include <QObject>
#include <QWidget>
#include <QToolBar>
#include <QWindow>
#include <QMainWindow>
#include <QMoveEvent>
#include <QDebug>

namespace Breeze {
    ToolsAreaManager::ToolsAreaManager(Helper *helper, QObject *parent) : QObject(parent), _helper(helper) {}

    ToolsAreaManager::~ToolsAreaManager() {
        for (auto &x: _connections) {
            disconnect(x);
        }
    }

    void ToolsAreaManager::updateAnimations() {
        for (auto entry : animationMap) {
            entry.foregroundColorAnimation->setStartValue(_helper->titleBarTextColor(false));
            entry.foregroundColorAnimation->setEndValue(_helper->titleBarTextColor(true));

            entry.backgroundColorAnimation->setStartValue(_helper->titleBarColor(false));
            entry.backgroundColorAnimation->setEndValue(_helper->titleBarColor(true));
            KColorScheme(QPalette::Inactive, KColorScheme::Header);

            entry.foregroundColorAnimation->setDuration(
                _helper->decorationConfig()->animationsEnabled() ?
                _helper->decorationConfig()->animationsDuration() :
                0
            );
            entry.backgroundColorAnimation->setDuration(
                _helper->decorationConfig()->animationsEnabled() ?
                _helper->decorationConfig()->animationsDuration() :
                0
            );
        }
    }

    void ToolsAreaManager::registerAnimation(QWidget *widget) {
        auto window = widget->window()->windowHandle();
        if (window && !animationMap.contains(window)) {

            auto foregroundColorAnimation = new QVariantAnimation(this);
            _connections << connect(foregroundColorAnimation, &QVariantAnimation::valueChanged,
                    this, &ToolsAreaManager::toolbarUpdated);

            auto backgroundColorAnimation = new QVariantAnimation(this);
            _connections << connect(backgroundColorAnimation, &QVariantAnimation::valueChanged,
                    this, &ToolsAreaManager::toolbarUpdated);

            foregroundColorAnimation->setStartValue(_helper->titleBarTextColor(false));
            foregroundColorAnimation->setEndValue(_helper->titleBarTextColor(true));

            backgroundColorAnimation->setStartValue(_helper->titleBarColor(false));
            backgroundColorAnimation->setEndValue(_helper->titleBarColor(true));

            foregroundColorAnimation->setDuration(
                _helper->decorationConfig()->animationsEnabled() ?
                _helper->decorationConfig()->animationsDuration() :
                0
            );
            backgroundColorAnimation->setDuration(
                _helper->decorationConfig()->animationsEnabled() ?
                _helper->decorationConfig()->animationsDuration() :
                0
            );

            animationMap[window] = ToolsAreaAnimation{
                foregroundColorAnimation,
                backgroundColorAnimation,
                window->isActive(),
            };

            _connections << connect(window, &QWindow::activeChanged,
                    this, [=]() {
                        if (animationMap[window].foregroundColorAnimation.isNull() || animationMap[window].backgroundColorAnimation.isNull()) return;

                        auto prevActive = animationMap[window].prevActive;
                        if (prevActive && !window->isActive()) {
                            animationMap[window].foregroundColorAnimation->setDirection(QAbstractAnimation::Backward);
                            animationMap[window].backgroundColorAnimation->setDirection(QAbstractAnimation::Backward);

                            animationMap[window].foregroundColorAnimation->start();
                            animationMap[window].backgroundColorAnimation->start();
                        } else if (!prevActive && window->isActive()) {
                            animationMap[window].foregroundColorAnimation->setDirection(QAbstractAnimation::Forward);
                            animationMap[window].backgroundColorAnimation->setDirection(QAbstractAnimation::Forward);

                            animationMap[window].foregroundColorAnimation->start();
                            animationMap[window].backgroundColorAnimation->start();
                        }
                        animationMap[window].prevActive = window->isActive();
                    });

        }
    }

    bool ToolsAreaManager::animationRunning(const QWidget *widget) {
        auto window = widget->window()->windowHandle();
        if (window && animationMap.contains(window)) {
            return (
                animationMap[window].foregroundColorAnimation->state() == QAbstractAnimation::Running
                &&
                animationMap[window].backgroundColorAnimation->state() == QAbstractAnimation::Running
            );
        }
        return false;
    }

    QColor ToolsAreaManager::foreground(const QWidget *widget) {
        auto window = widget->window()->windowHandle();
        if (window && animationMap.contains(window) && animationMap[window].foregroundColorAnimation) {
            return animationMap[window].foregroundColorAnimation->currentValue().value<QColor>();
        }
        return QColor();
    }

    QColor ToolsAreaManager::background(const QWidget *widget) {
        auto window = widget->window()->windowHandle();
        if (window && animationMap.contains(window) && animationMap[window].backgroundColorAnimation) {
            return animationMap[window].backgroundColorAnimation->currentValue().value<QColor>();
        }
        return QColor();
    }

    QPalette ToolsAreaManager::toolsPalette(const QWidget *widget) {
        auto color = KColorScheme(widget->isActiveWindow() ? widget->isEnabled() ? QPalette::Normal : QPalette::Disabled : QPalette::Inactive, KColorScheme::Header);
        QPalette palette = widget->palette();
        color.adjustForeground(palette);
        color.adjustBackground(palette);
        return palette;
    }

    void ToolsAreaManager::registerWindow(QWindow *window)
    {
        if (!_registeredWindows.contains(window)) {
            auto geoUpdate = [=]() {
                _helper->_invalidateCachedRects = true;
                emit toolbarUpdated();
            };
            _connections << connect(window, &QWindow::widthChanged, geoUpdate);
            _connections << connect(window, &QWindow::heightChanged, geoUpdate);
            _connections << connect(window, &QWindow::destroyed, geoUpdate);
            _registeredWindows << window;
        }
    }

    bool ToolsAreaManager::eventFilter(QObject *watched, QEvent *event)
    {
        Q_UNUSED(watched)
        if (event->type() == QEvent::Move) {
            auto moveEvent = static_cast<QMoveEvent*>(event);
            if (moveEvent->oldPos() != moveEvent->pos()) {
                Q_EMIT toolbarUpdated();
            }
        }
        return false;
    }

    void ToolsAreaManager::registerWidget(QWidget *widget)
    {
        auto win = widget->window();
        if (win) {
            auto handle = win->windowHandle();
            if (handle) {
                registerWindow(handle);
            }
        }
        auto window = qobject_cast<QMainWindow*> (widget);
        if (window) {
            _connections << connect(this, &ToolsAreaManager::toolbarUpdated,
                    window, [this, window]() {
                        if (_helper->toolsAreaHasContents(window)) {
                            window->setContentsMargins(0,0,0,0);
                        } else {
                            window->setContentsMargins(0,1,0,0);
                        }
                    });
        }
        _connections << connect(this, &ToolsAreaManager::toolbarUpdated,
                widget, [widget, this]() {
                    widget->update();
                    auto win = widget->window();
                    if (win) {
                        auto handle = win->windowHandle();
                        if (handle) {
                            _helper->_cachedRects.remove(handle);
                            widget->update();
                        }
                    }
                });
        auto toolbar = qobject_cast<QToolBar*>(widget);
        if (toolbar) {
            _connections << connect(this, &ToolsAreaManager::toolbarUpdated,
                    widget, [=]() {
                        const auto rect = _helper->toolsAreaToolbarsRect(widget);
                        if (rect.bottom() != widget->geometry().bottom()) {
                            toolbar->setContentsMargins(0,0,0,0);
                        } else {
                            toolbar->setContentsMargins(0,0,0,4);
                        }
                    });
            _connections << connect(toolbar, &QToolBar::visibilityChanged,
                    this, [this]() {
                        emit toolbarUpdated();
                    });
            _connections << connect(toolbar, &QToolBar::orientationChanged,
                    this, [this]() {
                        emit toolbarUpdated();
                    });
            _connections << connect(toolbar, &QToolBar::topLevelChanged,
                    this, [this]() {
                        emit toolbarUpdated();
                    });
            if (!toolbar->property("__breezeEventFilter").isValid()) {
                toolbar->installEventFilter(this);
            }
        }
        _connections << connect(widget, &QObject::destroyed,
                this, [this, widget]() {
                    unregisterWidget(widget);
                });
        registerAnimation(widget);
        _registeredWidgets << widget;
        emit toolbarUpdated();
    }

    bool ToolsAreaManager::widgetHasCorrectPaletteSet(const QWidget *widget)
    {
        if (animationRunning(widget)) return true;
        return (
            widget->palette().color(QPalette::Window) == background(widget)
            &&
            widget->palette().color(QPalette::WindowText) == foreground(widget)
        );
    }

    void ToolsAreaManager::unregisterWidget(QWidget *widget)
    {
        if (qobject_cast<QToolBar*>(widget)) {
            widget->setContentsMargins(0,0,0,0);
            if (widget->property("__breezeEventFilter").isValid()) {
                widget->removeEventFilter(this);
            }
        }
        _registeredWidgets.remove(widget);
        QList<QWindow*> toRemove;
        for (auto window : animationMap.keys()) {
            if (std::none_of(_registeredWidgets.begin(), _registeredWidgets.end(), [window](QWidget *widget) {
                return window == widget->window()->windowHandle();
            })) {
                delete animationMap[window].foregroundColorAnimation;
                delete animationMap[window].backgroundColorAnimation;
                toRemove << window;
            }
        }
        for (auto entry : toRemove) {
            animationMap.remove(entry);
        }
    }
}