#ifndef breezetoolsareamanager_h
#define breezetoolsareamanager_h

#include <QObject>
#include <QVariantAnimation>
#include "breezestyle.h"
#include "breezehelper.h"

namespace Breeze {
    struct ToolsAreaPalette
    {
        KColorScheme active;
        KColorScheme inactive;
        KColorScheme disabled;
    };

    class ToolsAreaManager: public QObject
    {
        Q_OBJECT

    private:
        Helper* _helper;
        QHash<QMainWindow*,QList<QPointer<QToolBar>>> _windows;

    protected:
        bool tryRegisterToolBar(QPointer<QMainWindow> window, QPointer<QWidget> widget);
        void tryUnregisterToolBar(QPointer<QMainWindow> window, QPointer<QWidget> widget);

    public:
        explicit ToolsAreaManager(Helper* helper, QObject *parent = nullptr);
        ~ToolsAreaManager();

        bool eventFilter(QObject* watched, QEvent *event) override;

        QPair<QPalette,ToolsAreaPalette> toolsAreaPalette();

        void registerWidget(QWidget* widget);
        void unregisterWidget(QWidget* widget);

        QRect toolsAreaRect(const QMainWindow* window);
    };
}

#endif
