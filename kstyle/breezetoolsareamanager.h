#ifndef breezetoolsareamanager_h
#define breezetoolsareamanager_h

#include <QObject>
#include <QVariantAnimation>
#include "breezestyle.h"
#include "breezehelper.h"

namespace Breeze {
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

        QColor foreground(const QWidget *widget);
        QColor background(const QWidget *widget);
        QPalette toolsPalette(const QWidget *widget);

        bool widgetHasCorrectPaletteSet(const QWidget *widget);

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override;

    Q_SIGNALS:
        void toolbarUpdated();

    private:
        void registerWindow ( QWindow *window );
        void registerAnimation( QWidget *widget );
        bool animationRunning( const QWidget *widget );
        QSet<QWidget*> _registeredWidgets;
        QSet<QWindow*> _registeredWindows;
        QList<QMetaObject::Connection> _connections;
        Helper* _helper;

        QMap<QWindow*,ToolsAreaAnimation> animationMap;
    };
}

#endif