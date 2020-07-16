#include "breezetoolsareamanager.h"
#include <QObject>
#include <QWidget>
#include <QToolBar>
#include <QWindow>
#include <QMainWindow>
#include <QMoveEvent>
#include <QMenuBar>
#include <QMdiArea>
#include <QDebug>
#include <QTimer>
#include <KWindowSystem>
#include <KColorUtils>
#include "signal.h"

namespace Breeze {
    ToolsAreaManager::ToolsAreaManager(Helper *helper, QObject *parent) : QObject(parent), _timer(new QTimer(this)), _helper(helper) {
        _timer->setInterval(10);
        _timer->setSingleShot(true);
        _timer->callOnTimeout([=]() {
            for (auto &x: _toolsArea.keys()) {
                for (auto &w: _toolsArea[x]->widgets) {
                    evaluateToolsArea(x, w);
                    w->update();
                }
            }
        });
        connect(this, &ToolsAreaManager::toolbarUpdated, this, &ToolsAreaManager::setWindowMargins);
        connect(this, &ToolsAreaManager::toolbarUpdated, this, &ToolsAreaManager::rerenderWidgets);
    }

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
                        if (KWindowSystem::isPlatformX11()) {
                            return;
                        }

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

    QColor ToolsAreaManager::opacify(const QWidget *widget, const QColor& in) {
        if (widget->isEnabled()) {
            return in;
        }
        return KColorUtils::mix(in, Qt::transparent, 0.2);
    }

    QColor ToolsAreaManager::foreground(const QWidget *widget) {
        if (KWindowSystem::isPlatformX11()) {
            return opacify(widget, _helper->titleBarTextColor(true));
        }
        auto window = widget->window()->windowHandle();
        if (window && animationMap.contains(window) && animationMap[window].foregroundColorAnimation) {
            return opacify(widget, animationMap[window].foregroundColorAnimation->currentValue().value<QColor>());
        }
        return QColor();
    }

    QColor ToolsAreaManager::background(const QWidget *widget) {
        if (KWindowSystem::isPlatformX11()) {
            return opacify(widget, _helper->titleBarColor(true));
        }
        auto window = widget->window()->windowHandle();
        if (window && animationMap.contains(window) && animationMap[window].backgroundColorAnimation) {
            return opacify(widget, animationMap[window].backgroundColorAnimation->currentValue().value<QColor>());
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
                for (auto main : _toolsArea.keys()) {
                    if (reinterpret_cast<QObject*>(main->windowHandle()) == window) {
                        recomputeRect(main);
                    }
                }
                emit toolbarUpdated();
            };
            _connections << connect(window, &QWindow::widthChanged, geoUpdate);
            _connections << connect(window, &QWindow::heightChanged, geoUpdate);
            _connections << connect(window, &QWindow::destroyed, geoUpdate);
            _registeredWindows << window;
        }
    }

    void ToolsAreaManager::evaluateToolsArea(QMainWindow *window, QWidget *widget, bool forceVisible, bool forceInvisible)
    {
        if (!window) {
            return;
        }
        if (!widget) {
            return;
        }

        if (!_helper->shouldDrawToolsArea(widget)) {
            getWidgetList(window)->remove(widget);
            return;
        }

        auto checkToolbarInToolsArea = [this, window](const QWidget* widget) {
            auto toolbar = qobject_cast<const QToolBar*>(widget);
            if (!toolbar) return false;

            if (window) {
                if (toolbar->isFloating()) return false;
                if (toolbar->orientation() == Qt::Vertical) return false;
                if (window->toolBarArea(const_cast<QToolBar*>(toolbar)) != Qt::TopToolBarArea) return false;
            }

            return true;
        };
        auto checkMenubarInToolsArea = [window](const QWidget *widget) {
            if (window) {
                if (window->menuWidget() == widget) {
                    return true;
                }
            }

            return false;
        };
        auto checkWidgetInToolsArea = [window](QWidget *widget) {
            auto docked = qobject_cast<QDockWidget*>(widget);
            if (!docked) {
                return false;
            }

            return window->dockWidgetArea(docked) == Qt::TopDockWidgetArea;
        };

        if ((forceInvisible || !widget->isVisible()) && !forceVisible) {
            getWidgetList(window)->remove(widget);
            return;
        }
        if (widget->window()->windowType() == Qt::Dialog) {
            getWidgetList(window)->remove(widget);
            return;
        }

        auto parent = widget;
        while (parent != nullptr) {
            if (qobject_cast<const QMdiArea*>(parent) || qobject_cast<const QDockWidget*>(parent)) {
                getWidgetList(window)->remove(widget);
                return;
            }
            if (checkToolbarInToolsArea(parent)) {
                getWidgetList(window)->insert(widget);
                return;
            }
            if (checkMenubarInToolsArea(parent)) {
                getWidgetList(window)->insert(widget);
                return;
            }
            if (checkWidgetInToolsArea(parent)) {
                getWidgetList(window)->insert(widget);
                return;
            }
            parent = parent->parentWidget();
        }

        getWidgetList(window)->remove(widget);
    }

    bool ToolsAreaManager::eventFilter(QObject *watched, QEvent *event)
    {
        if (qobject_cast<QToolBar*>(watched)) {
            if (event->type() == QEvent::Move) {
                auto moveEvent = static_cast<QMoveEvent*>(event);
                if (moveEvent->oldPos() != moveEvent->pos()) {
                    Q_EMIT toolbarUpdated();
                }
                _timer->start();
            }
        }
        if (qobject_cast<QMainWindow*>(watched)) {
            if (event->type() == QEvent::ChildAdded) {
                auto ev = static_cast<QChildEvent*>(event);
                auto wi = qobject_cast<QMainWindow*>(watched);
                auto wd = qobject_cast<QWidget*>(ev->child());
                if (wd) {
                    evaluateToolsArea(wi, wd);
                    recomputeRect(wi);
                }
            }
            if (event->type() == QEvent::ChildRemoved) {
                auto ev = static_cast<QChildEvent*>(event);
                auto wi = qobject_cast<QMainWindow*>(watched);
                auto wd = qobject_cast<QWidget*>(ev->child());
                if (wd) {
                    getWidgetList(wi)->remove(wd);
                    recomputeRect(wi);
                }
            }
        } else if (qobject_cast<QWidget*>(watched)) {
            if (event->type() == QEvent::Show || event->type() == QEvent::Hide) {
                auto widget = qobject_cast<QWidget*>(watched);
                auto window = qobject_cast<QMainWindow*>(widget->window());
                if (event->type() == QEvent::Show) {
                    evaluateToolsArea(window, widget, true);
                } else {
                    evaluateToolsArea(window, widget, false, true);
                }
                recomputeRect(window);
                _timer->start();
            } else if (event->type() == QEvent::HideToParent || event->type() == QEvent::ShowToParent) {
                auto widget = qobject_cast<QWidget*>(watched);
                auto window = qobject_cast<QMainWindow*>(widget->window());
                evaluateToolsArea(window, widget);
                recomputeRect(window);
                _timer->start();
            }
        }
        return false;
    }

    void ToolsAreaManager::recomputeRect(QMainWindow *w)
    {
        if (!w) return;
        QRect rect = QRect();
        auto widgets = getWidgetList(w)->widgets;
        for (auto widget : widgets) {
            if (widget) {
                auto widgetRect = widget->geometry();
                if (widgetRect.isValid()) {
                    rect = rect.united(widgetRect);
                }
            }
        }
        rect.setWidth(w->width());
        _rects[w] = rect;
        _timer->start();
    }

    bool ToolsAreaManager::isInToolsArea(const QWidget *widget)
    {
        auto nc = const_cast<QWidget*>(widget);
        auto win = qobject_cast<QMainWindow*>(nc->window());
        if (!win) {
            return false;
        }
        return getWidgetList(win)->widgets.contains(nc);
    }

    void ToolsAreaManager::setWindowMargins()
    {
        for (auto win : _toolsArea.keys()) {
            if (hasContents(win)) {
                win->setContentsMargins(0,0,0,0);
            } else {
                win->setContentsMargins(0,1,0,0);
            }
        }
    }

    void ToolsAreaManager::rerenderWidgets()
    {
        for (auto widget : _registeredWidgets) {
            widget->update();
        }
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
            if (!window->property("__breezeEventFilter").isValid()) {
                window->setProperty("__breezeEventFilter", true);
                window->installEventFilter(this);
            }
        }
        if (!window) {
            window = qobject_cast<QMainWindow*>(widget->window());
            evaluateToolsArea(window, widget);
            recomputeRect(window);
            if (!widget->property("__breezeEventFilter").isValid()) {
                widget->setProperty("__breezeEventFilter", true);
                widget->installEventFilter(this);
            }
        }
        auto toolbar = qobject_cast<QToolBar*>(widget);
        if (toolbar) {
            evaluateToolsArea(window, widget);
            _connections << connect(toolbar, &QToolBar::visibilityChanged,
                    this, [this, window, widget](bool visible) {
                        evaluateToolsArea(window, widget, visible, !visible);
                        emit toolbarUpdated();
                    });
            _connections << connect(toolbar, &QToolBar::orientationChanged,
                    this, [this, window, widget]() {
                        evaluateToolsArea(window, widget);
                        emit toolbarUpdated();
                    });
            _connections << connect(toolbar, &QToolBar::topLevelChanged,
                    this, [this, window, widget]() {
                        evaluateToolsArea(window, widget);
                        emit toolbarUpdated();
                    });
            if (!toolbar->property("__breezeEventFilter").isValid()) {
                toolbar->setProperty("__breezeEventFilter", true);
                toolbar->installEventFilter(this);
            }
        }
        _connections << connect(widget, &QObject::destroyed,
                this, [this, widget]() {
                    unregisterWidget(widget);
                });
        registerAnimation(widget);
        _timer->start();
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
                widget->setProperty("__breezeEventFilter", QVariant());
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
        _timer->start();
    }
}