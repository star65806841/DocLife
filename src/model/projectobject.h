#ifndef PROJECTOBJECT_H
#define PROJECTOBJECT_H

#include <QObject>
#include <memory>

namespace doclife::model {
    class ProjectHead;
    class ProjectObject : public QObject
    {
        Q_OBJECT
    public:
        explicit ProjectObject(QObject* parent = nullptr);
        ~ProjectObject();
    signals:

    private:
        std::unique_ptr<ProjectHead> m_projectData = nullptr;
    private:
        void init();
    };
}
#endif // PROJECTOBJECT_H
