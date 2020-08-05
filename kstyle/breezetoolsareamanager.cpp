#include "breezetoolsareamanager.h"

#include <QMainWindow>
#include <QMenuBar>
#include <QObject>
#include <QToolBar>
#include <QWidget>
#include <QWindow>

#include <KColorUtils>

// DON'T MERGE THIS PATCH IF THIS IS STILL HERE
// THIS IS SO THAT I CAN CHANGE WHETHER I WANT TO
// RELY ON UNIFIEDTITLEANDTOOLBARONMAC FROM A
// SINGLE PLACE

#define mergeToolBarHint mainWindow->unifiedTitleAndToolBarOnMac()
#define mergeToolBarHint true

namespace Breeze {
    ToolsAreaManager::ToolsAreaManager(Helper *helper, QObject *parent) : QObject(parent), _helper(helper) {}

    ToolsAreaManager::~ToolsAreaManager() {}

    template<class T1, class T2>
    void appendIfNotAlreadyExists(T1* list, T2 item) {
        for (auto listItem : *list) {
            if (listItem == item) {
                return;
            }
        }
        list->append(item);
    }

    QRect ToolsAreaManager::toolsAreaRect(const QMainWindow *window)
    {
        Q_ASSERT(window);

        int itemHeight = window->menuWidget() ? window->menuWidget()->height() : 0;
        for (auto item : _windows[const_cast<QMainWindow*>(window)]) {
            if (!item.isNull() && item->isVisible() && window->toolBarArea(item) == Qt::TopToolBarArea) {
                itemHeight = qMax(item->mapTo(window, item->rect().bottomLeft()).y(), itemHeight);
            }
        }

        if (itemHeight == 0) {
            auto win = const_cast<QMainWindow*>(window);
            win->setContentsMargins(0, 0, 0, 1);
        } else {
            auto win = const_cast<QMainWindow*>(window);
            win->setContentsMargins(0, 0, 0, 0);
        }

        return QRect(0, 0, window->width(), itemHeight);
    }

    bool ToolsAreaManager::tryRegisterToolBar(QPointer<QMainWindow> window, QPointer<QWidget> widget)
    {
        Q_ASSERT(!widget.isNull());

        QPointer<QToolBar> toolbar;
        if (!(toolbar = qobject_cast<QToolBar*>(widget))) return false;

        if (window->toolBarArea(toolbar) == Qt::TopToolBarArea) {
            appendIfNotAlreadyExists(&_windows[window], toolbar);
            return true;
        }

        return false;
    }

    void ToolsAreaManager::tryUnregisterToolBar(QPointer<QMainWindow> window, QPointer<QWidget> widget)
    {
        Q_ASSERT(!widget.isNull());

        QPointer<QToolBar> toolbar;
        if (!(toolbar = qobject_cast<QToolBar*>(widget))) return;

        if (window->toolBarArea(toolbar) != Qt::TopToolBarArea) {
            _windows[window].removeAll(toolbar);
        }
    }

    QPalette ToolsAreaManager::toolsAreaPalette()
    {
        static QPalette palette = QPalette();
        const char* colorProperty = "KDE_COLOR_SCHEME_PATH";

        if (palette != QPalette()) {
            return palette;
        }

        KSharedConfigPtr schemeFile = KSharedConfig::openConfig();

        if (qApp && qApp->property(colorProperty).isValid()) {
            auto path = qApp->property(colorProperty).toString();
            schemeFile = KSharedConfig::openConfig(path);
        }

        KColorScheme scheme(QPalette::Active, KColorScheme::Header, schemeFile);

        palette = scheme.createApplicationPalette(schemeFile);

        palette.setBrush(QPalette::Active, QPalette::Window, scheme.background());
        palette.setBrush(QPalette::Active, QPalette::WindowText, scheme.foreground());

        return palette;
    }

    bool ToolsAreaManager::eventFilter(QObject *watched, QEvent *event)
    {
        Q_ASSERT(watched);
        Q_ASSERT(event);

        QPointer<QObject> parent = watched;
        QPointer<QMainWindow> mainWindow = nullptr;
        while (parent != nullptr) {
            if (qobject_cast<QMainWindow*>(parent)) {
                mainWindow = qobject_cast<QMainWindow*>(parent);
                break;
            }
            parent = parent->parent();
        }

        if (QPointer<QMainWindow> mw = qobject_cast<QMainWindow*>(watched)) {
            QChildEvent *ev;
            if (event->type() == QEvent::ChildAdded || event->type() == QEvent::ChildRemoved)
                ev = static_cast<QChildEvent*>(event);

            QPointer<QToolBar> tb = qobject_cast<QToolBar*>(ev->child());
            if (tb.isNull())
                return false;

            if (ev->added()) {
                if (mw->toolBarArea(tb) == Qt::TopToolBarArea)
                    appendIfNotAlreadyExists(&_windows[mw], tb);
            } else if (ev->removed()) {
                _windows[mw].removeAll(tb);
            }
        } else if (qobject_cast<QToolBar*>(watched)) {
            if (!mainWindow.isNull()) {
                tryUnregisterToolBar(mainWindow, qobject_cast<QWidget*>(watched));
            }
        }

        return false;
    }

    void ToolsAreaManager::registerWidget(QWidget *widget)
    {
        Q_ASSERT(widget);
        auto ptr = QPointer<QWidget>(widget);

        auto parent = ptr;
        QPointer<QMainWindow> mainWindow = nullptr;
        while (parent != nullptr) {
            if (qobject_cast<QMainWindow*>(parent)) {
                mainWindow = qobject_cast<QMainWindow*>(parent);
                break;
            }
            parent = parent->parentWidget();
        } if (mainWindow == nullptr || !mergeToolBarHint) {
            return;
        }

        if (tryRegisterToolBar(mainWindow, widget)) return;
    }

    void ToolsAreaManager::unregisterWidget(QWidget *widget)
    {
        Q_ASSERT(widget);
        auto ptr = QPointer<QWidget>(widget);

        if (QPointer<QMainWindow> window = qobject_cast<QMainWindow*>(ptr)) {
            _windows.remove(window);
            return;
        } else if (QPointer<QToolBar> toolbar = qobject_cast<QToolBar*>(ptr)) {
            auto parent = ptr;
            QPointer<QMainWindow> mainWindow = nullptr;
            while (parent != nullptr) {
                if (qobject_cast<QMainWindow*>(parent)) {
                    mainWindow = qobject_cast<QMainWindow*>(parent);
                    break;
                }
                parent = parent->parentWidget();
            } if (mainWindow == nullptr || !mergeToolBarHint) {
                return;
            }
            _windows[mainWindow].removeAll(toolbar);
        }
    }
}