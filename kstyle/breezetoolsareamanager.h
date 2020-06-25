#ifndef breezetoolsareamanager_h
#define breezetoolsareamanager_h

#include <QObject>
#include <QVariantAnimation>
#include "breezestyle.h"
#include "breezehelper.h"

namespace Breeze {
    struct WidgetList : public QObject
    {
        Q_OBJECT

    public:
        QList<QWidget*> widgets = QList<QWidget*>();

        void widgetRemoved() {
            auto widget = qobject_cast<QWidget*>(sender());
            widgets.removeAll(widget);
        }
        void remove(QWidget *widget) {
            widgets.removeAll(widget);
            disconnect(widget, &QObject::destroyed, this, &WidgetList::widgetRemoved);
        }
        void insert(QWidget *widget) {
            if (!widgets.contains(widget)) {
                widgets << widget;
                connect(widget, &QObject::destroyed, this, &WidgetList::widgetRemoved, Qt::UniqueConnection);
            }
        }
    };
    struct ToolsAreaAnimation {
        QPointer<QVariantAnimation> foregroundColorAnimation;
        QPointer<QVariantAnimation> backgroundColorAnimation;
        bool prevActive;
    };
    class ToolsAreaManager: public QObject
    {
        Q_OBJECT
    
    public:
        explicit ToolsAreaManager(Helper* helper, QObject *parent = nullptr);
        ~ToolsAreaManager();
        void registerWidget(QWidget *widget);
        void unregisterWidget(QWidget *widget);
        void updateAnimations();
        bool isInToolsArea(const QWidget *widget);
        void evaluateToolsArea(QMainWindow *window, QWidget *widget, bool forceVisible = false, bool forceInvisible = false);

        QColor foreground(const QWidget *widget);
        QColor background(const QWidget *widget);
        QPalette toolsPalette(const QWidget *widget);
        QRect rect(const QWidget *w) const {
            auto m = qobject_cast<const QMainWindow*>(w->window());
            if (m)
                return rect(m);
            return QRect();
        }
        QRect rect(const QMainWindow *w) const {
            auto nc = const_cast<QMainWindow*>(w);
            return _rects[nc];
        }
        bool hasContents(const QWidget *w) const {
            auto m = qobject_cast<QMainWindow*>(w->window());
            if (m)
                return hasContents(m);
            return false;
        }
        bool hasContents(const QMainWindow *w) const {
            auto nc = const_cast<QMainWindow*>(w);
            return _toolsArea[nc]->widgets.length() > 0;
        }

        bool widgetHasCorrectPaletteSet(const QWidget *widget);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    Q_SIGNALS:
        void toolbarUpdated();

    public Q_SLOTS:
        void recomputeRect(QMainWindow *w);

    private:
        void registerWindow ( QWindow *window );
        void registerAnimation( QWidget *widget );
        bool animationRunning( const QWidget *widget );
        QSet<QWidget*> _registeredWidgets;
        QSet<QWindow*> _registeredWindows;
        QList<QMetaObject::Connection> _connections;
        Helper* _helper;

        QMap<QMainWindow*,WidgetList*> _toolsArea;
        WidgetList* getWidgetList(QMainWindow* win) {
            if (!_toolsArea.contains(win)) {
                auto list = new WidgetList;
                _toolsArea[win] = list;
                return list;
            }
            return _toolsArea[win];
        }
        QMap<QMainWindow*,QRect> _rects;
        QMap<QWindow*,ToolsAreaAnimation> animationMap;
    };
}

#endif